<!--
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Measure-Based Splitting in FingerTree

## Overview

A finger tree splits on **measures** --- tagged metadata accumulated along the
sequence --- not just on element count. The default measure counts elements, so
count-based splitting falls out as the special case where the tag is
`std::size_t` and every element measures `1`. Everything else is the same
machinery with a different monoid.

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

The default `UnitMeasure` tags every element with `1` and the `Monoid<std::size_t>`
adds, so the prefix is just the element count. `split_at_measure(3)` puts the
first two elements left, and the third --- the one that brings the count to 3 ---
and everything after it right.

### A weighted measure

Custom measures need three things: a tag type, a measure policy (the functor
mapping an element to its tag), and a `Monoid` for the tag. Here is all three:

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

namespace beman::fingertree::typeclass {
template <>
struct Monoid<Weighted> {
    auto identity() const -> Weighted { return Weighted{0}; }
    auto combine(Weighted a, Weighted b) const -> Weighted {
        return Weighted{a.d_total + b.d_total};
    }
};
}

using WeightedTree =
    beman::fingertree::tree::FingerTree<int, Weighted, WeightedMeasure>;

auto tree  = WeightedTree::from_sequence({1, 2, 3, 4});  // weights 10, 20, 30, 40
auto split = tree.split_at_measure(Weighted{35});

// 10 + 20 = 30 (< 35, keep going), then + 30 = 60 (>= 35, split here)
assert(split.d_left.flatten()  == std::vector<int>{1, 2});
assert(split.d_right.flatten() == std::vector<int>{3, 4});
```

Forget the `Monoid<Weighted>` specialization and it does not compile. That is the
point of the typeclass --- the measure is not usable until its algebra is
declared.

### Interval pruning

The interval index carries `IntervalMaxEndTag`, whose monoid combines by `max`.
Split on it to partition intervals by their running maximum endpoint:

```cpp
#include <beman/fingertree/tree/finger_tree_interval_index.hpp>

using beman::fingertree::tree::Interval;
using beman::fingertree::tree::IntervalMaxEndTag;
using beman::fingertree::tree::IntervalMeasure;

using IntervalTree = beman::fingertree::tree::FingerTree<
    Interval<std::string>,
    IntervalMaxEndTag<std::string>,
    IntervalMeasure<std::string>>;

auto tree = IntervalTree::from_sequence({
    Interval<std::string>{0,  5,  "A"},   // max_end = 5
    Interval<std::string>{3,  10, "B"},   // max_end = 10
    Interval<std::string>{8,  12, "C"},   // max_end = 12
    Interval<std::string>{10, 15, "D"},   // max_end = 15
});

auto split = tree.split_at_measure(IntervalMaxEndTag<std::string>{10});
auto kept  = split.d_left.flatten();   // {[0,5,"A"], [3,10,"B"]}
```

The prefix is the running `max` of the endpoints:

```
A: max_end 5,  prefix max(_, 5)   = 5   (< 10, keep)
B: max_end 10, prefix max(5, 10)  = 10  (>= 10, split here)
C: max_end 12, prefix max(10, 12) = 12  (right)
D: max_end 15, prefix max(12, 15) = 15  (right)
```

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

Splitting is monoid accumulation and nothing else. The default `std::size_t`
tag uses the additive monoid:

```cpp
namespace beman::fingertree::typeclass {
template <>
struct Monoid<std::size_t> {
    constexpr auto identity() const -> std::size_t { return 0U; }
    constexpr auto combine(std::size_t lhs, std::size_t rhs) const -> std::size_t {
        return lhs + rhs;
    }
};
}
```

Change the monoid and you change what "prefix" means --- a count, a maximum
endpoint, a total weight, a concatenated string. The split code does not know or
care which; it combines and compares. See
[allocator-design.md](allocator-design.md) for how those combined trees stay
coherent across allocators, and `finger_tree.test.cpp` for the exercised
behavior.
