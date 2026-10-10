// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_PRIORITY_QUEUE_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_PRIORITY_QUEUE_HPP

#include <beman/fingertree/tree/finger_tree.hpp>
#include <beman/fingertree/typeclass/applicative.hpp>
#include <beman/fingertree/typeclass/foldable.hpp>
#include <beman/fingertree/typeclass/monoid.hpp>
#include <beman/fingertree/typeclass/traversable.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace beman::fingertree::tree {

/** Minimum monoid on `std::optional<T>` with an adjoined identity: `nullopt`
 * is the identity, and two engaged operands combine to the smaller. Unlike
 * `typeclass::min_monoid<T>`, which needs a saturating bound, this works for
 * any totally ordered `T`.
 */
template <typename T>
struct optional_min_monoid {
    using value_type = std::optional<T>;

    auto identity() const -> std::optional<T> { return std::nullopt; }

    auto combine(const std::optional<T>& lhs, const std::optional<T>& rhs) const -> std::optional<T> {
        if (!lhs.has_value())
            return rhs;
        if (!rhs.has_value())
            return lhs;
        return *lhs <= *rhs ? lhs : rhs;
    }
};

/** Maximum monoid on `std::optional<T>` with an adjoined identity. */
template <typename T>
struct optional_max_monoid {
    using value_type = std::optional<T>;

    auto identity() const -> std::optional<T> { return std::nullopt; }

    auto combine(const std::optional<T>& lhs, const std::optional<T>& rhs) const -> std::optional<T> {
        if (!lhs.has_value())
            return rhs;
        if (!rhs.has_value())
            return lhs;
        return *lhs >= *rhs ? lhs : rhs;
    }
};

/** The priority queue's measure value: element count, minimum, maximum. A
 * plain tuple; the monoid that combines it is `PriorityMonoid<T>`, a product
 * of three instances, so adding the count cost one more tuple element and
 * one more operand, not a new tag type and specialization.
 */
template <typename T>
using PriorityTag = std::tuple<std::size_t, std::optional<T>, std::optional<T>>;

template <typename T>
using PriorityMonoid =
    beman::fingertree::typeclass::tuple_monoid<beman::fingertree::typeclass::sum_monoid<std::size_t>,
                                               optional_min_monoid<T>,
                                               optional_max_monoid<T>>;

template <typename T>
struct PriorityMeasure {
    auto operator()(const T& value) const -> PriorityTag<T> { return PriorityTag<T>{1, value, value}; }
};

/** @brief Persistent double-ended priority queue backed by a finger tree.
 *
 * @tparam T    Element type; must be totally ordered.
 * @tparam Tree Backing finger tree type (must use PriorityMonoid<T> over
 *              PriorityMeasure<T>).  Default: FingerTree — correct at all
 *              sizes.
 *
 * Complexity:
 * - push:       O(1) amortized
 * - min / max:  O(1)
 * - pop_min / pop_max: O(log n)
 * - size:       O(1) — the count is the first component of the measure
 * - to_vector:  O(n)
 */
template <typename T, typename Tree = FingerTree<T, PriorityMonoid<T>, PriorityMeasure<T>>>
class FingerTreePriorityQueue {
    Tree d_tree;

    explicit FingerTreePriorityQueue(Tree tree) : d_tree(std::move(tree)) {}

  public:
    FingerTreePriorityQueue() : d_tree(Tree{}) {}

    static auto from_values(std::vector<T> values) -> FingerTreePriorityQueue {
        return FingerTreePriorityQueue{Tree::from_sequence(std::move(values))};
    }

    auto empty() const -> bool { return d_tree.is_empty(); }

    /** Returns the number of elements; O(1), read off the cached measure. */
    auto size() const -> std::size_t { return std::get<0>(d_tree.measure()); }

    auto min() const -> std::optional<T> { return std::get<1>(d_tree.measure()); }

    auto max() const -> std::optional<T> { return std::get<2>(d_tree.measure()); }

    auto push(T value) const -> FingerTreePriorityQueue {
        return FingerTreePriorityQueue{d_tree.snoc(std::move(value))};
    }

    auto pop_min() const -> std::optional<std::pair<T, FingerTreePriorityQueue>> {
        auto current = min();
        if (!current.has_value())
            return std::nullopt;
        T    global_min = *current;
        auto sp         = d_tree.split([global_min](const PriorityTag<T>& p) {
            const auto& m = std::get<1>(p);
            return m.has_value() && *m <= global_min;
        });
        if (!sp.has_value())
            return std::nullopt;
        return std::pair<T, FingerTreePriorityQueue>{sp->d_pivot,
                                                     FingerTreePriorityQueue{Tree::concat(sp->d_left, sp->d_right)}};
    }

    auto pop_max() const -> std::optional<std::pair<T, FingerTreePriorityQueue>> {
        auto current = max();
        if (!current.has_value())
            return std::nullopt;
        T    global_max = *current;
        auto sp         = d_tree.split([global_max](const PriorityTag<T>& p) {
            const auto& m = std::get<2>(p);
            return m.has_value() && *m >= global_max;
        });
        if (!sp.has_value())
            return std::nullopt;
        return std::pair<T, FingerTreePriorityQueue>{sp->d_pivot,
                                                     FingerTreePriorityQueue{Tree::concat(sp->d_left, sp->d_right)}};
    }

    auto to_vector() const -> std::vector<T> { return d_tree.flatten(); }
};

} // namespace beman::fingertree::tree

namespace beman::fingertree {

template <class T, class Tree>
struct FingerTreePriorityQueueFoldableImpl {
    using element_type = T;

    // The monoid is an instance object over the result type, passed by
    // value; the Impl never chooses one (beman.transpose
    // docs/decisions.md#monoid-selection).
    template <class F, class RESULT_MONOID>
    auto fold_map(this auto&&,
                  F&&                                                              function,
                  const beman::fingertree::tree::FingerTreePriorityQueue<T, Tree>& queue,
                  RESULT_MONOID monoid) -> remove_cvref_t<std::invoke_result_t<F&, const T&>> {
        using Result = remove_cvref_t<std::invoke_result_t<F&, const T&>>;
        return std::ranges::fold_left(
            queue.to_vector(), Result(monoid.identity()), [&](Result acc, const auto& value) {
                return monoid.combine(std::move(acc), std::invoke(function, value));
            });
    }
};

template <class T, class Tree>
struct FingerTreePriorityQueueFoldableMap : Foldable<FingerTreePriorityQueueFoldableImpl<T, Tree>> {};

template <class T, class Tree>
inline constexpr auto foldable_typeclass<beman::fingertree::tree::FingerTreePriorityQueue<T, Tree>> =
    FingerTreePriorityQueueFoldableMap<T, Tree>{};

template <class T, class Tree>
struct FingerTreePriorityQueueTraversableImpl {
    using element_type = T;

    template <class APPLICATIVE, class F>
    auto traverse(this auto&&,
                  const APPLICATIVE&                                               applicative,
                  F&&                                                              function,
                  const beman::fingertree::tree::FingerTreePriorityQueue<T, Tree>& queue) {
        using Context = remove_cvref_t<std::invoke_result_t<F, const T&>>;
        using U       = beman::fingertree::applicative_value_t<Context>;

        auto accumulated = applicative.pure(std::vector<U>{});

        for (const auto& value : queue.to_vector()) {
            auto lifted = std::invoke(function, value);
            accumulated = applicative.invoke(
                [](std::vector<U> values, U element) {
                    values.push_back(std::move(element));
                    return values;
                },
                std::move(accumulated),
                std::move(lifted));
        }

        return applicative.invoke(
            [](std::vector<U> values) {
                return beman::fingertree::tree::FingerTreePriorityQueue<U>::from_values(std::move(values));
            },
            std::move(accumulated));
    }
};

template <class T, class Tree>
struct FingerTreePriorityQueueTraversableMap : Traversable<FingerTreePriorityQueueTraversableImpl<T, Tree>> {
    using FingerTreePriorityQueueTraversableImpl<T, Tree>::traverse;
};

template <class T, class Tree>
inline constexpr auto traversable_typeclass<beman::fingertree::tree::FingerTreePriorityQueue<T, Tree>> =
    FingerTreePriorityQueueTraversableMap<T, Tree>{};

} // namespace beman::fingertree

#endif
