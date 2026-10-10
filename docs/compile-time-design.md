<!--
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Compile-Time Design: Why the Spine Is a Uniform Type

The textbook finger tree, from Hinze and Paterson, is a nested type. `FingerTree a`
holds a `FingerTree (Node a)` in its middle, which holds a `FingerTree (Node
(Node a))`, and so on. The element type changes at every level of the spine, and
operations that walk the spine use polymorphic recursion --- a recursive call at
a different type from its caller. Haskell does this directly. C++ cannot.

This note records why `include/beman/fingertree/tree/finger_tree.hpp` does not
translate that structure literally, and why the compile-probe test exists to keep
it that way.

## What the nested type does to a C++ compiler

Translate the nested type literally and each spine level is a *distinct*
`FingerTree<Node<T,Monoid>>` instantiation. One use of the tree instantiates the
whole ladder, and every operation that walks it --- `flatten`, `measure`,
`search`, `split`, `split_at`, `split_at_measure` --- expands recursively through
all of them.

An earlier, compatibility-oriented implementation did exactly this. It parsed
fine. `leaf()` was cheap. Custom measure tags were cheap. The cliff arrived the
moment a translation unit odr-used `flatten()`, and everything downstream of
`flatten()` inherited the fall. Isolated probe translation units under an 8 GB
virtual-memory cap:

- Header-only and `leaf()` probes finished in seconds on both Clang 23 and
  GCC 16.
- Weighted-tag probes stayed cheap until they touched the flatten-based path.
- `flatten()` alone triggered the pathological expansion. `measure()`,
  `search()`, and `split()` fell because they all route through it.

Before the rewrite, Clang 23 could finish the combined probe --- in about 28
minutes and 6.3 GB RSS. GCC 16 did not finish; it exhausted the 8 GB cap after
about 6 minutes and 7.9 GB RSS. A flat persistent-vector backend kept the public
API and collapsed compile time back to seconds, but it is not a finger tree and
was only ever an emergency fallback.

## The design choice: one element type, a runtime variant

`FingerTree` keeps the same public surface without the nested type. The spine is
the same `FingerTree` type at every depth --- `shared_ptr<const Deep>` all the
way down --- and the element is a single self-recursive type:

```cpp
template <typename T, typename MONOID>
struct Elem {
    tag_t<MONOID> d_measure;   // the measure value type, derived from the instance
    struct Leaf  { T d_value; };
    struct Node2 { ElemPtr a, b; };
    struct Node3 { ElemPtr a, b, c; };
    std::variant<Leaf, Node2, Node3> d_data;
};
```

The 2-3 structure that the type system used to carry is now a runtime
`variant`. Depth is data, not type. A single `FingerTree<T, Monoid, Meas>` use
produces one `FingerTree` instantiation and one `Elem<T,Monoid>` instantiation,
no matter how deep the tree grows at runtime. (`Elem` is keyed on the monoid
*instance* type, not the measure value type: a cached measure is valid only
under the monoid that computed it, so a count tree and a max tree over the same
`std::size_t` must not share `Elem` instantiations.) The recursion the compiler used to
chase now happens over values.

The one further change from the earlier uniform-element design is the digit:
`std::inplace_vector<ElemPtr, 4>` with an explicit size, in place of a four-way
`variant<One, Two, Three, Four>`. `cons`, `snoc`, `view`, `split`, and `concat`
branch on `size()` instead of dispatching a four-way `std::visit` at every level.

## The probe that guards it

`tests/beman/fingertree/finger_tree_compile_probe.test.cpp` is the baseline. It
instantiates `FingerTree<int>` and `FingerTree<int, weight_monoid, WeightMeasure>`
and exercises the whole API that used to fall off the cliff --- `flatten`,
`measure`, `split`, `split_at_measure`, `search`, `append`, the views, `for_each`.

It is checked by template-expansion count, not by wall-clock. Count the DWARF
class-type entries in the probe's object file:

```sh
readelf --debug-dump=info <probe>.o | grep -c DW_TAG_class_type
```

A single `<T,Monoid,Meas>` use should produce one `FingerTree` and one `Elem<T,Monoid>`.
A significant jump in that count against the baseline means a regression in the
uniform-element design --- an accidental depth-typed specialization, or a new
intermediate template on a hot path --- reintroducing the very expansion this
design exists to avoid.

## The rule for the next structural pass

If you rewrite the structure, do not reintroduce unbounded recursive compile-time
expansion through a depth-typed spine on inline paths. Concretely:

- Treat `flatten()` as a compile-time hotspot, not just a runtime helper. It is
  the canary.
- Watch recursive `Node`/`Digit` instantiation that is visible in headers.
- Check a minimal ladder of probes --- header only, `leaf()`, `flatten()`,
  `measure()`, `search()`, `split()` --- before scaling back up to the full
  tests.

If `flatten()` regresses, the rest of the API will follow it down. That is the
whole reason the spine is a uniform type.
