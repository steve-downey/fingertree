<!--
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Measure-Based Splitting in FingerTree

## Overview

A finger tree splits on **measures** --- tagged metadata accumulated along the
sequence --- not just on element count. The default measure counts elements, so
count-based splitting falls out as the special case where the tag is
`std::size_t`, every element measures `1`, and the monoid is addition.
Everything else is the same machinery with a different monoid --- and the
monoid is a parameter of the tree, an *instance object* over the tag type,
not something looked up from the tag type. A `std::size_t` tag can be
combined by addition in one tree and by `max` in another; the two are
different tree types and never share nodes.

This note documents the three public split methods, what they return, and how
the split walks the spine.

## The methods

### `split_at(pred)` --- split at a predicate over the prefix

```cpp
template <typename PRED>
auto split_at(PRED&& pred) const -> SplitAt;

struct SplitAt {
    FingerTree d_left;   // elements where the predicate was still false
    FingerTree d_right;  // from the pivot onward (pivot included)
};
```

The traversal accumulates the measure left to right and calls `pred` on the
running prefix. The pivot is the first element at which `pred` flips to `true`,
and it lands in `d_right`. If `pred` never becomes true, the result is
`SplitAt{entire_tree, empty_tree}`.

`pred` takes a `const Tag&` (the accumulated prefix) and returns `bool`. It is
called on monotonically growing prefixes, so it must be monotone --- once true,
stay true --- or the answer is meaningless.

### `split_at_measure(threshold)` --- split at a measure threshold

```cpp
auto split_at_measure(const Tag& threshold) const -> SplitAt
    requires requires(const Tag& a, const Tag& b) {
        { a >= b } -> std::convertible_to<bool>;
    };
```

A convenience wrapper. It is exactly:

```cpp
return split_at([&threshold](const Tag& prefix) { return prefix >= threshold; });
```

so it splits where the accumulated measure first reaches or exceeds `threshold`.
The constraint is the whole implementation --- the `Tag` needs `operator>=`, and
nothing more.

### `split(pred)` --- isolate the pivot

```cpp
template <typename PRED>
auto split(PRED&& pred) const -> std::optional<Split>;

struct Split {
    FingerTree d_left;   // before the pivot
    T          d_pivot;  // the element where the predicate became true
    FingerTree d_right;  // after the pivot
};
```

Like `split_at`, but it hands back the pivot element on its own. Returns
`std::nullopt` when `pred` never fires. `split_at` is built on top of it:
`split`, then `cons` the pivot back onto `d_right`. There is also `search(pred)`,
which is `split(pred)` keeping only `d_pivot` --- the first element past the
threshold, or `nullopt`.

## How the split walks the spine

There is no separate segment structure to descend. The split walks the same
`shared_ptr` spine every other operation uses, and the recursion is in
`split_impl`, `split_digit`, and `split_within_elem` in
`include/beman/fingertree/tree/finger_tree.hpp`.

At a `Deep` node the tree is a left digit, a spine of nodes, and a right digit.
`split_impl` accumulates the prefix and decides where the pivot lives by
measure, cheapest region first:

1. Combine the prefix with the **left digit's** measure. If `pred` is already
   true, the pivot is in the left digit --- `split_digit` finds it directly.
2. Otherwise combine in the **spine's** measure. If `pred` is true, recurse into
   the spine to reach the node holding the pivot, then `split_within_elem` opens
   that Node2/Node3 to isolate the leaf.
3. Otherwise the pivot is in the **right digit**, and `split_digit` finds it
   there.

Only the region containing the pivot is rebuilt. The rest is shared by pointer,
so the split is O(log N) in time and rebuilds O(log N) structure. When the
measure carries pruning information --- an interval `d_max_end`, say --- a whole
subtree can be skipped the moment its cached measure shows it cannot satisfy the
predicate.

## Examples

### Count-based split (the default measure)

```cpp
using CountTree = beman::fingertree::tree::FingerTree<int>;

auto tree  = CountTree::from_sequence({1, 2, 3, 4, 5});
auto split = tree.split_at_measure(std::size_t{3});

assert(split.d_left.flatten()  == std::vector<int>{1, 2});
assert(split.d_right.flatten() == std::vector<int>{3, 4, 5});
```

The default `UnitMeasure` tags every element with `1` and the default monoid
instance, `typeclass::sum_monoid<std::size_t>`, adds, so the prefix is just the
element count. `split_at_measure(3)` puts the first two elements left, and the
third --- the one that brings the count to 3 --- and everything after it right.

### A weighted measure

Custom measures need three things: a tag type, a measure policy (the functor
mapping an element to its tag), and a monoid instance over the tag --- an
object with `identity()` and `combine(a, b)`. The instance is a template
parameter of the tree, so it is named where the tree is, not registered
against the tag type. Here is all three:

```cpp
struct Weighted {
    std::size_t d_total;
    friend bool operator>=(const Weighted& a, const Weighted& b) {
        return a.d_total >= b.d_total;
    }
};

struct WeightedMeasure {
    auto operator()(int value) const -> Weighted {
        return Weighted{static_cast<std::size_t>(value * 10)};
    }
};

struct weighted_monoid {
    using value_type = Weighted;
    auto identity() const -> Weighted { return Weighted{0}; }
    auto combine(Weighted a, Weighted b) const -> Weighted {
        return Weighted{a.d_total + b.d_total};
    }
};

using WeightedTree =
    beman::fingertree::tree::FingerTree<int, weighted_monoid, WeightedMeasure>;

auto tree  = WeightedTree::from_sequence({1, 2, 3, 4});  // weights 10, 20, 30, 40
auto split = tree.split_at_measure(Weighted{35});

// 10 + 20 = 30 (< 35, keep going), then + 30 = 60 (>= 35, split here)
assert(split.d_left.flatten()  == std::vector<int>{1, 2});
assert(split.d_right.flatten() == std::vector<int>{3, 4});
```

Leave the instance out and it does not compile: the tree's second parameter
*is* its algebra. Because the tag is a plain struct and the instance is
separate, a second tree over `Weighted` under a different monoid --- the
maximum weight, say --- is one more small instance type, not a second
`Weighted`.

An instance may carry state. Addition modulo a runtime value is a monoid on
`std::size_t`; pass the instance to `from_sequence` (or the allocator-taking
constructor) and the tree stores it and combines every node under it:

```cpp
struct modular_sum {
    using value_type = std::size_t;
    std::size_t d_modulus;
    auto identity() const -> std::size_t { return 0U; }
    auto combine(std::size_t a, std::size_t b) const -> std::size_t {
        return (a + b) % d_modulus;
    }
};

using ModTree = beman::fingertree::tree::FingerTree<int, modular_sum>;
auto tree = ModTree::from_sequence({1, 2, 3, 4}, std::allocator<std::byte>{},
                                   modular_sum{5});
assert(tree.measure() == 0U);        // 10 mod 5
assert(tree.get_monoid().d_modulus == 5U);
```

The library copies instances freely and passes them by value; the expectation
is that an instance is shaped like a stateless object whose calls resolve
statically, and `sum_monoid`, `max_monoid`, and friends are exactly that.

### Interval pruning

The interval index measures each interval by its end point, a plain
`std::size_t`, and combines under `typeclass::max_monoid<std::size_t>`
(spelled `IntervalMonoid`). Split on it to partition intervals by their running
maximum endpoint:

```cpp
#include <beman/fingertree/tree/finger_tree_interval_index.hpp>

using beman::fingertree::tree::Interval;
using beman::fingertree::tree::IntervalMeasure;
using beman::fingertree::tree::IntervalMonoid;

using IntervalTree = beman::fingertree::tree::FingerTree<
    Interval<std::string>,
    IntervalMonoid,
    IntervalMeasure<std::string>>;

auto tree = IntervalTree::from_sequence({
    Interval<std::string>{0,  5,  "A"},   // end = 5
    Interval<std::string>{3,  10, "B"},   // end = 10
    Interval<std::string>{8,  12, "C"},   // end = 12
    Interval<std::string>{10, 15, "D"},   // end = 15
});

auto split = tree.split_at([](std::size_t max_end) { return max_end > 10; });
auto kept  = split.d_left.flatten();   // {[0,5,"A"], [3,10,"B"]}
```

The prefix is the running `max` of the endpoints:

```
A: end 5,  prefix max(_, 5)   = 5   (<= 10, keep)
B: end 10, prefix max(5, 10)  = 10  (<= 10, keep)
C: end 12, prefix max(10, 12) = 12  (> 10, split here)
D: end 15, prefix max(12, 15) = 15  (right)
```

Nothing about `std::size_t` says "maximum"; the instance does. The same tag
type under `sum_monoid` is the count tree above.

### A custom predicate

`split_at` takes any monotone predicate over the prefix, so a running-sum cutoff
is direct:

```cpp
auto tree  = CountTree::from_sequence({1, 2, 3, 4, 5});
auto split = tree.split_at([](std::size_t prefix) { return prefix > 7; });

// prefix runs 1 -> 3 -> 6 -> 10 (first > 7 at the 4th element... but the
// default measure counts, so this splits on count, not on element value)
```

If you want to split on the *sum of the values* rather than the count, that is a
different measure --- tag each element with its own value and let the monoid add.
The predicate does not change the measure; the measure policy does.

### Extracting the pivot with `split`

```cpp
auto tree = CountTree::from_sequence({1, 2, 3, 4, 5});

if (auto s = tree.split([](std::size_t count) { return count >= 3; })) {
    assert(s->d_left.flatten()  == std::vector<int>{1, 2});
    assert(s->d_pivot == 3);
    assert(s->d_right.flatten() == std::vector<int>{4, 5});
}
```

## Complexity

- **Time:** `split`, `split_at`, and `split_at_measure` are all O(log N). The
  depth is O(log N) and each level discards one region by measure.
- **Space:** O(log N). Only the spine down to the pivot is rebuilt; every
  untouched subtree is shared by `shared_ptr`, not copied.
- **Pruning:** when the tag summarizes a subtree well enough to rule it out --- an
  interval `d_max_end` below the threshold --- that subtree is skipped whole,
  never descended.

## The monoid underneath

Splitting is monoid accumulation and nothing else. The default tree,
`FingerTree<T>`, is `FingerTree<T, typeclass::sum_monoid<std::size_t>,
UnitMeasure<T, std::size_t>>`, and `sum_monoid` is the whole of the algebra:

```cpp
template <class T>
struct sum_monoid {
    using value_type = T;
    constexpr auto identity() const -> T { return T{}; }
    constexpr auto combine(const T& lhs, const T& rhs) const -> T {
        return lhs + rhs;
    }
};
```

There is deliberately no `Monoid<std::size_t>` registration to fall back on:
a number carries addition, product, maximum and minimum, and the library does
not pick one on the caller's behalf (beman.transpose
`docs/decisions.md#monoid-selection`). Change the instance and you change what
"prefix" means --- a count, a maximum endpoint, a total weight, a concatenated
string --- without changing the tag type. `reversed()` is the same move: it
returns a tree keyed on `typeclass::dual_monoid<MONOID>`, same tag, combine
with its arguments flipped, so `split_at_measure` on a reversed tree takes the
same plain threshold. The split code does not know or care which instance it
has; it combines and compares. See
[allocator-design.md](allocator-design.md) for how those combined trees stay
coherent across allocators, and `finger_tree.test.cpp` for the exercised
behavior.
