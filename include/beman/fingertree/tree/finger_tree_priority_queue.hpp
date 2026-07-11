// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_PRIORITY_QUEUE_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_PRIORITY_QUEUE_HPP

#include <beman/fingertree/tree/finger_tree5.hpp>
#include <beman/fingertree/typeclass/applicative.hpp>
#include <beman/fingertree/typeclass/foldable.hpp>
#include <beman/fingertree/typeclass/monoid.hpp>
#include <beman/fingertree/typeclass/traversable.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace beman::fingertree::tree {

template <typename T>
struct MinTag {
    std::optional<T> d_value;
    friend bool      operator==(const MinTag&, const MinTag&) = default;
};

template <typename T>
struct MaxTag {
    std::optional<T> d_value;
    friend bool      operator==(const MaxTag&, const MaxTag&) = default;
};

template <typename T>
struct PriorityTag {
    MinTag<T>   d_min;
    MaxTag<T>   d_max;
    friend bool operator==(const PriorityTag&, const PriorityTag&) = default;
};

template <typename T>
struct PriorityMeasure {
    auto operator()(const T& value) const -> PriorityTag<T> {
        return PriorityTag<T>{MinTag<T>{value}, MaxTag<T>{value}};
    }
};

/** @brief Persistent double-ended priority queue backed by a finger tree.
 *
 * @tparam T    Element type; must be totally ordered.
 * @tparam Tree Backing finger tree type (must use PriorityTag<T> measure).
 *              Default: FingerTree5 — correct at all sizes.
 *
 * Complexity:
 * - push:       O(1) amortized
 * - min / max:  O(1)
 * - pop_min / pop_max: O(log n)
 * - size:       O(n) — the PriorityTag measure does not cache element count
 * - to_vector:  O(n)
 */
template <typename T, typename Tree = FingerTree5<T, PriorityTag<T>, PriorityMeasure<T>>>
class FingerTreePriorityQueue {
    Tree d_tree;

    explicit FingerTreePriorityQueue(Tree tree) : d_tree(std::move(tree)) {}

  public:
    FingerTreePriorityQueue() : d_tree(Tree{}) {}

    static auto from_values(std::vector<T> values) -> FingerTreePriorityQueue {
        return FingerTreePriorityQueue{Tree::from_sequence(std::move(values))};
    }

    auto empty() const -> bool { return d_tree.is_empty(); }

    /** Returns the number of elements; O(n) — the PriorityTag measure does not
     *  cache a count, so we fall back to for_each counting. */
    auto size() const -> std::size_t {
        std::size_t n = 0;
        d_tree.for_each([&](const T&) { ++n; });
        return n;
    }

    auto min() const -> std::optional<T> {
        auto m = d_tree.measure().d_min.d_value;
        return m.has_value() ? std::optional<T>{*m} : std::nullopt;
    }

    auto max() const -> std::optional<T> {
        auto m = d_tree.measure().d_max.d_value;
        return m.has_value() ? std::optional<T>{*m} : std::nullopt;
    }

    auto push(T value) const -> FingerTreePriorityQueue {
        return FingerTreePriorityQueue{d_tree.snoc(std::move(value))};
    }

    auto pop_min() const -> std::optional<std::pair<T, FingerTreePriorityQueue>> {
        auto tag = d_tree.measure();
        if (!tag.d_min.d_value.has_value())
            return std::nullopt;
        T    global_min = *tag.d_min.d_value;
        auto sp         = d_tree.split([global_min](const PriorityTag<T>& p) {
            return p.d_min.d_value.has_value() && *p.d_min.d_value <= global_min;
        });
        if (!sp.has_value())
            return std::nullopt;
        return std::pair<T, FingerTreePriorityQueue>{sp->d_pivot,
                                                     FingerTreePriorityQueue{Tree::concat(sp->d_left, sp->d_right)}};
    }

    auto pop_max() const -> std::optional<std::pair<T, FingerTreePriorityQueue>> {
        auto tag = d_tree.measure();
        if (!tag.d_max.d_value.has_value())
            return std::nullopt;
        T    global_max = *tag.d_max.d_value;
        auto sp         = d_tree.split([global_max](const PriorityTag<T>& p) {
            return p.d_max.d_value.has_value() && *p.d_max.d_value >= global_max;
        });
        if (!sp.has_value())
            return std::nullopt;
        return std::pair<T, FingerTreePriorityQueue>{sp->d_pivot,
                                                     FingerTreePriorityQueue{Tree::concat(sp->d_left, sp->d_right)}};
    }

    auto to_vector() const -> std::vector<T> { return d_tree.flatten(); }
};

} // namespace beman::fingertree::tree

namespace beman::fingertree::typeclass {

template <typename T>
struct Monoid<beman::fingertree::tree::MinTag<T>> {
    auto identity() const -> beman::fingertree::tree::MinTag<T> { return {std::nullopt}; }
    auto combine(const beman::fingertree::tree::MinTag<T>& lhs, const beman::fingertree::tree::MinTag<T>& rhs) const
        -> beman::fingertree::tree::MinTag<T> {
        if (!lhs.d_value.has_value())
            return rhs;
        if (!rhs.d_value.has_value())
            return lhs;
        return lhs.d_value.value() <= rhs.d_value.value() ? lhs : rhs;
    }
};

template <typename T>
struct Monoid<beman::fingertree::tree::MaxTag<T>> {
    auto identity() const -> beman::fingertree::tree::MaxTag<T> { return {std::nullopt}; }
    auto combine(const beman::fingertree::tree::MaxTag<T>& lhs, const beman::fingertree::tree::MaxTag<T>& rhs) const
        -> beman::fingertree::tree::MaxTag<T> {
        if (!lhs.d_value.has_value())
            return rhs;
        if (!rhs.d_value.has_value())
            return lhs;
        return lhs.d_value.value() >= rhs.d_value.value() ? lhs : rhs;
    }
};

template <typename T>
struct Monoid<beman::fingertree::tree::PriorityTag<T>> {
    auto identity() const -> beman::fingertree::tree::PriorityTag<T> {
        return {Monoid<beman::fingertree::tree::MinTag<T>>{}.identity(),
                Monoid<beman::fingertree::tree::MaxTag<T>>{}.identity()};
    }
    auto combine(const beman::fingertree::tree::PriorityTag<T>& lhs,
                 const beman::fingertree::tree::PriorityTag<T>& rhs) const -> beman::fingertree::tree::PriorityTag<T> {
        return {Monoid<beman::fingertree::tree::MinTag<T>>{}.combine(lhs.d_min, rhs.d_min),
                Monoid<beman::fingertree::tree::MaxTag<T>>{}.combine(lhs.d_max, rhs.d_max)};
    }
};

} // namespace beman::fingertree::typeclass

namespace beman::fingertree {

template <class T, class Tree>
struct FingerTreePriorityQueueFoldableImpl {
    template <class F>
    auto fold_map(this auto&&, F&& function, const beman::fingertree::tree::FingerTreePriorityQueue<T, Tree>& queue)
        -> remove_cvref_t<std::invoke_result_t<F, const T&>> {
        using Result = remove_cvref_t<std::invoke_result_t<F, const T&>>;
        return std::ranges::fold_left(queue.to_vector(),
                                      beman::fingertree::typeclass::monoid_v<Result>.identity(),
                                      [&](Result acc, const auto& value) {
                                          return beman::fingertree::typeclass::monoid_v<Result>.combine(
                                              std::move(acc), std::invoke(function, value));
                                      });
    }
};

template <class T, class Tree>
struct FingerTreePriorityQueueFoldableMap : Foldable<FingerTreePriorityQueueFoldableImpl<T, Tree>> {
    using FingerTreePriorityQueueFoldableImpl<T, Tree>::fold_map;
};

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
