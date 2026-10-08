// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_INTERVAL_INDEX_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_INTERVAL_INDEX_HPP

#include <beman/fingertree/tree/finger_tree.hpp>
#include <beman/fingertree/typeclass/applicative.hpp>
#include <beman/fingertree/typeclass/foldable.hpp>
#include <beman/fingertree/typeclass/monoid.hpp>
#include <beman/fingertree/typeclass/traversable.hpp>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace beman::fingertree::tree {

template <typename PAYLOAD_TYPE>
struct Interval {
    std::size_t  d_start;
    std::size_t  d_end;
    PAYLOAD_TYPE d_payload;
};

/** Measure policy: an interval measures its end point. Under
 * `max_monoid<std::size_t>` the cached measure of any subtree is its
 * maximum end point, which is what lets a query skip a subtree whole. The
 * measure value type is a plain `std::size_t`; the monoid is chosen by the
 * tree's instance type, not by wrapping the value.
 */
template <typename PAYLOAD_TYPE>
struct IntervalMeasure {
    auto operator()(const Interval<PAYLOAD_TYPE>& interval) const -> std::size_t { return interval.d_end; }
};

/** The monoid instance the interval index measures under. */
using IntervalMonoid = beman::fingertree::typeclass::max_monoid<std::size_t>;

/** @brief Persistent interval index supporting stabbing and overlap queries.
 *
 * @tparam PAYLOAD_TYPE Arbitrary data associated with each interval.
 * @tparam Tree         Backing finger tree type (must use IntervalMonoid over
 *                      IntervalMeasure).  Default: FingerTree — correct at all
 *                      sizes.
 *
 * Complexity:
 * - insert:        O(1) amortized
 * - query_point:   O(log n + k) where k is the number of results
 * - query_overlap: O(log n + k) where k is the number of results
 * - entries:       O(n)
 */
template <typename PAYLOAD_TYPE,
          typename Tree = FingerTree<Interval<PAYLOAD_TYPE>, IntervalMonoid, IntervalMeasure<PAYLOAD_TYPE>>>
class FingerTreeIntervalIndex {
    using Entry = Interval<PAYLOAD_TYPE>;

    Tree d_tree;

    explicit FingerTreeIntervalIndex(Tree tree) : d_tree(std::move(tree)) {}

  public:
    FingerTreeIntervalIndex() : d_tree(Tree{}) {}

    static auto from_intervals(std::vector<Entry> entries) -> FingerTreeIntervalIndex {
        return FingerTreeIntervalIndex{Tree::from_sequence(std::move(entries))};
    }

    auto insert(Entry entry) const -> FingerTreeIntervalIndex {
        return FingerTreeIntervalIndex{d_tree.snoc(std::move(entry))};
    }

    auto query_point(std::size_t point) const -> std::vector<PAYLOAD_TYPE> {
        std::vector<PAYLOAD_TYPE> out;
        auto                      parts = d_tree.split_at([point](std::size_t max_end) { return max_end > point; });
        parts.d_right.for_each([&](const Entry& e) {
            if (e.d_start <= point && point < e.d_end)
                out.push_back(e.d_payload);
        });
        return out;
    }

    auto query_overlap(std::size_t start, std::size_t end) const -> std::vector<PAYLOAD_TYPE> {
        std::vector<PAYLOAD_TYPE> out;
        auto                      parts = d_tree.split_at([start](std::size_t max_end) { return max_end > start; });
        parts.d_right.for_each([&](const Entry& e) {
            if (e.d_start < end && start < e.d_end)
                out.push_back(e.d_payload);
        });
        return out;
    }

    auto entries() const -> std::vector<Entry> { return d_tree.flatten(); }
};

} // namespace beman::fingertree::tree

namespace beman::fingertree {

template <class PAYLOAD_TYPE, class Tree>
struct FingerTreeIntervalIndexFoldableImpl {
    using element_type = PAYLOAD_TYPE;

    // The monoid is an instance object over the result type, passed by
    // value; the Impl never chooses one (beman.transpose
    // docs/decisions.md#monoid-selection).
    template <class F, class RESULT_MONOID>
    auto fold_map(this auto&&,
                  F&&                                                                         function,
                  const beman::fingertree::tree::FingerTreeIntervalIndex<PAYLOAD_TYPE, Tree>& index,
                  RESULT_MONOID monoid) -> remove_cvref_t<std::invoke_result_t<F&, const PAYLOAD_TYPE&>> {
        using Result = remove_cvref_t<std::invoke_result_t<F&, const PAYLOAD_TYPE&>>;
        return std::ranges::fold_left(index.entries(), Result(monoid.identity()), [&](Result acc, const auto& value) {
            return monoid.combine(std::move(acc), std::invoke(function, value.d_payload));
        });
    }
};

template <class PAYLOAD_TYPE, class Tree>
struct FingerTreeIntervalIndexFoldableMap : Foldable<FingerTreeIntervalIndexFoldableImpl<PAYLOAD_TYPE, Tree>> {};

template <class PAYLOAD_TYPE, class Tree>
inline constexpr auto foldable_typeclass<beman::fingertree::tree::FingerTreeIntervalIndex<PAYLOAD_TYPE, Tree>> =
    FingerTreeIntervalIndexFoldableMap<PAYLOAD_TYPE, Tree>{};

template <class PAYLOAD_TYPE, class Tree>
struct FingerTreeIntervalIndexTraversableImpl {
    using element_type = PAYLOAD_TYPE;

    template <class APPLICATIVE, class F>
    auto traverse(this auto&&,
                  const APPLICATIVE&                                                          applicative,
                  F&&                                                                         function,
                  const beman::fingertree::tree::FingerTreeIntervalIndex<PAYLOAD_TYPE, Tree>& index) {
        using Context = remove_cvref_t<std::invoke_result_t<F, const PAYLOAD_TYPE&>>;
        using U       = beman::fingertree::applicative_value_t<Context>;

        auto accumulated = applicative.pure(std::vector<beman::fingertree::tree::Interval<U>>{});

        for (const auto& entry : index.entries()) {
            auto lifted = std::invoke(function, entry.d_payload);
            accumulated = applicative.invoke(
                [start = entry.d_start, end = entry.d_end](std::vector<beman::fingertree::tree::Interval<U>> values,
                                                           U                                                 payload) {
                    values.push_back(beman::fingertree::tree::Interval<U>{start, end, std::move(payload)});
                    return values;
                },
                std::move(accumulated),
                std::move(lifted));
        }

        return applicative.invoke(
            [](std::vector<beman::fingertree::tree::Interval<U>> values) {
                return beman::fingertree::tree::FingerTreeIntervalIndex<U>::from_intervals(std::move(values));
            },
            std::move(accumulated));
    }
};

template <class PAYLOAD_TYPE, class Tree>
struct FingerTreeIntervalIndexTraversableMap
    : Traversable<FingerTreeIntervalIndexTraversableImpl<PAYLOAD_TYPE, Tree>> {
    using FingerTreeIntervalIndexTraversableImpl<PAYLOAD_TYPE, Tree>::traverse;
};

template <class PAYLOAD_TYPE, class Tree>
inline constexpr auto traversable_typeclass<beman::fingertree::tree::FingerTreeIntervalIndex<PAYLOAD_TYPE, Tree>> =
    FingerTreeIntervalIndexTraversableMap<PAYLOAD_TYPE, Tree>{};

} // namespace beman::fingertree

#endif
