<!--
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# FingerTree Allocator Design

## Context

`FingerTree` is an immutable, structurally-shared persistent container. Its
internal representation is a recursive type of `shared_ptr`-wrapped nodes
(Leaf, Node2, Node3, Deep) where each node carries a cached measure. Structural
sharing means a `cons` or `split` produces a new tree that shares O(log N) spine
nodes with the original.

Allocator support has to resolve a tension. The `shared_ptr` model is what makes
structural sharing cheap; the Lakos rule says every allocation reachable from a
single object graph must come from the same memory resource. Sharing spine nodes
across trees is exactly the operation that can violate it.

---

## The Lakos coherency rule

An object that participates in a memory resource (allocator) must satisfy:

> All heap pointers reachable from the object point into memory owned by the
> same resource, or into static program storage, or into memory owned by the
> object itself.

The violations are quiet at the point of creation and catastrophic at the point
of access:

- A monotonic buffer backed by a memory-mapped file must contain only pointers
  that are still valid after the file is remapped at the same address in a
  restarted process.
- Two program instances sharing a memory segment must agree that every pointer
  in the segment stays within the segment.
- Releasing one resource while a second still holds pointers into it is a
  use-after-free with no warning at the site that created the alias.

Nothing complains at allocation time. The defect may only surface under a
particular memory layout. So the design pays, up front and explicitly, to make
mixed graphs unrepresentable through the public API.

---

## Design decisions

### 1. Allocator as a 4th template parameter

```cpp
template <typename T,
          typename MONOID         = typeclass::sum_monoid<std::size_t>,
          typename MEASURE_POLICY = UnitMeasure<T, ft::tag_t<MONOID>>,
          typename ALLOCATOR      = std::allocator<std::byte>>
class FingerTree;
```

The monoid instance is stored on the tree beside the allocator, also under
`[[no_unique_address]]`, and travels the same way: every tree a helper builds
inherits the instance of the tree it was built from, so a stateful instance
reaches every node it combines.

`std::byte` as the `value_type` matches `std::pmr::polymorphic_allocator` and
signals that the tree rebinds internally to the node types it actually allocates
(`Elem<T,Monoid>`, `Deep`, spine shells).

Existing code using `FingerTree<int>` keeps working unchanged. The default
`std::allocator<std::byte>` is stateless and, under `[[no_unique_address]]`,
adds no storage.

### 2. Which allocations use the custom allocator

**Yes** — `Elem<T,Monoid>` (leaf and node objects, 64 bytes each) and `Deep` (digit
+ spine + measure, ~168 bytes each). These are the user-data nodes and the vast
majority of the allocation cost.

**No** — the `FingerTree` spine shells (a 24-byte variant holding a `DeepPtr`).
These are internal artifacts that hold no user data. They do not need to come
from the same resource for the Lakos property to hold on the user-visible data
graph.

The exception is deliberate. Using `allocate_shared<const FingerTree>` for the
shell would trigger the `std::uses_allocator` construction protocol, which calls
the allocator-extended constructor. That constructor enforces coherency by
rebuilding a tree from its leaf values --- but a spine shell may contain Node3
objects, not leaf values, so flattening and rebuilding would produce a
structurally wrong tree (leaves where nodes should be). `make_shared` avoids the
protocol entirely.

### 3. Coherency enforcement at the public API surface

Operations that **combine two trees** must not silently produce a mixed graph.
The rule: if both operands share the same allocator (same memory resource),
structural sharing is safe and fast. If they differ, the foreign subtree is
**rebuilt** using the left operand's allocator before the combination proceeds.

| Operation | Same allocator | Different allocators |
|-----------|----------------|----------------------|
| `append(right)` | O(log N), structural sharing | O(\|right\|) rebuild of `right` |
| copy `operator=` | O(1), share `d_repr` | O(N) rebuild |
| move `operator=` | O(1), move `d_repr` | O(N) rebuild |
| Extended ctor `FingerTree(FingerTree&&, alloc)` | O(1), share | O(N) rebuild |

The cost is accepted on purpose. Silent mixing is an unacceptable correctness
risk; the rebuild is bounded and predictable. We take the predictable cost.

### 4. Detecting allocator equality

```cpp
static auto alloc_equal(const ALLOCATOR& a, const ALLOCATOR& b) noexcept
    -> bool {
    if constexpr (std::allocator_traits<ALLOCATOR>::is_always_equal::value)
        return true;  // compile-time elimination for stateless allocators
    else
        return a == b;  // runtime check (resource pointer comparison for PMR)
}
```

For `std::allocator<std::byte>`, `is_always_equal::value` is `true` and the
runtime check compiles away --- zero overhead on the default path. For
`pmr::polymorphic_allocator`, the check compares `resource()` pointers.

### 5. The `uses_allocator` extended constructor

```cpp
FingerTree(FingerTree other, ALLOCATOR alloc)
    : d_alloc(std::move(alloc)) {
    if (alloc_equal(d_alloc, other.d_alloc))
        d_repr = std::move(other.d_repr);
    else
        *this = from_sequence(other.flatten(), d_alloc);
}
```

This is the trailing-allocator form the `std::uses_allocator` protocol requires.
`alloc` is taken **by value**, not `const&`, so that
`pmr::polymorphic_allocator<FingerTree>` (produced by
`allocator_traits::rebind_alloc`) can convert implicitly to
`pmr::polymorphic_allocator<std::byte>` (`= ALLOCATOR`).

The coherency rule is baked in: a different resource forces a full rebuild. You
cannot accidentally produce a mixed tree through the standard construction
protocol.

### 6. `propagate_on_container_*` traits

Copy and move assignment respect the standard traits:

- **Propagating allocators** (`std::allocator`): the allocator propagates, the
  data is shared or moved. No rebuild.
- **Non-propagating allocators** (`pmr::polymorphic_allocator`): the allocator
  stays; the data is shared if the resource matches, rebuilt if it differs.

Move assignment is **not** `noexcept` for non-propagating allocators with
different resources, because the rebuild may allocate. That is the correct
behavior per the standard, not a limitation to fix.

### 7. The `append` fast path

```cpp
auto append(const FingerTree& right) const -> FingerTree {
    if (!alloc_equal(d_alloc, right.d_alloc)) {
        auto rebuilt = from_sequence(right.flatten(), d_alloc);
        return app3(d_alloc, *this, {}, rebuilt);
    }
    return app3(d_alloc, *this, {}, right);
}
```

The rebuild path is O(|right|). For the common case --- both trees on the same
resource, which is the whole point of PMR --- the structural-sharing path runs
with no overhead.

### 8. Spine shell allocation --- `allocate_spine`

The spine shells (`FingerTree` value objects stored inside `SpinePtr`) are
allocated from the custom allocator via manual placement new.

**Why not `allocate_shared`?** `std::allocate_shared<const FingerTree>(fta, t)`
would fire `std::uses_allocator_construction_args`, which selects the public
extended constructor `FingerTree(FingerTree&&, ALLOCATOR)`. That constructor's
coherency check calls `flatten()` when the allocator differs --- but spine
shells contain `Node3`/`Node2` elements, not leaf `T` values, so `flatten()`
would produce a structurally invalid tree (leaves where nodes should be) and
corrupt the spine.

The implemented approach in `allocate_spine(alloc, t)`:

1. `allocator_traits::allocate(fta, 1)` --- the shell object, from the custom
   allocator.
2. `::new(raw) FingerTree(std::move(t))` --- placement new, which bypasses
   `uses_allocator_construction_args` entirely. No protocol, no rebuild.
3. `shared_ptr(raw, deleter, fta)` --- the control block, from the custom
   allocator via the 3-arg `shared_ptr` constructor. Two allocations rather than
   `make_shared`'s one, but both from the correct resource.

**The self-enforcing test** (`finger_tree_pmr_probe.test.cpp`): the arena is
backed by `null_memory_resource()` as its upstream. Any allocation that tries to
escape to the global heap throws `std::bad_alloc` on the spot, so the test is an
automatic proof that nothing bypasses the arena. Zero global allocations, checked
by construction.

---

## Known limitations

### Static `leaf()` and `from_sequence()` factories use `ALLOCATOR{}`

```cpp
static auto leaf(T value) -> FingerTree;
static auto from_sequence(std::vector<T> values) -> FingerTree;
```

These are static, so they have no allocator context and use `ALLOCATOR{}` for
every allocation. For PMR trees the allocator-aware overload must be used:

```cpp
auto t = pmr::FingerTree<int>::from_sequence({1, 2, 3}, &mr);
```

---

## `beman::fingertree::tree::pmr::FingerTree`

```cpp
namespace beman::fingertree::tree::pmr {
template <typename T,
          typename MONOID         = typeclass::sum_monoid<std::size_t>,
          typename MEASURE_POLICY = UnitMeasure<T, ft::tag_t<MONOID>>>
using FingerTree =
    beman::fingertree::tree::FingerTree<T, MONOID, MEASURE_POLICY,
                                        std::pmr::polymorphic_allocator<std::byte>>;
}
```

An alias that wires `pmr::polymorphic_allocator<std::byte>` in as the allocator.
It inherits all the coherency guarantees above. `get_allocator().resource()`
returns the underlying `memory_resource*`.

Structural sharing across two `pmr::FingerTree` instances backed by **different**
resources is always safe: `append` detects the mismatch and rebuilds; assignment
and the extended constructor do the same. The result is always coherent with the
left-hand operand's resource.
