// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_FOLDABLE_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_FOLDABLE_HPP

#include <beman/fingertree/tree/finger_tree.hpp>
#include <beman/fingertree/typeclass/foldable.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace beman::fingertree {

/** Foldable typeclass implementation for FingerTree; uses for_each to
 * traverse leaves in left-to-right order without heap allocation.
 */
template <class T, class MONOID, class MEASURE_POLICY, class ALLOCATOR>
struct FingerTreeFoldableImpl {
    using element_type = T;

    // The result monoid is an instance object over the result type, passed
    // by value and distinct from the tree's own measure monoid MONOID; the
    // Impl never chooses one (beman.transpose
    // docs/decisions.md#monoid-selection).
    template <class F, class RESULT_MONOID>
    auto fold_map(this auto&&,
                  F&&                                                                              function,
                  const beman::fingertree::tree::FingerTree<T, MONOID, MEASURE_POLICY, ALLOCATOR>& tree,
                  RESULT_MONOID monoid) -> remove_cvref_t<std::invoke_result_t<F&, const T&>> {
        using Result = remove_cvref_t<std::invoke_result_t<F&, const T&>>;

        Result acc = monoid.identity();
        tree.for_each([&](const T& value) { acc = monoid.combine(std::move(acc), std::invoke(function, value)); });
        return acc;
    }
};

/** Foldable typeclass map entry for FingerTree. */
template <class T, class MONOID, class MEASURE_POLICY, class ALLOCATOR>
struct FingerTreeFoldableMap : Foldable<FingerTreeFoldableImpl<T, MONOID, MEASURE_POLICY, ALLOCATOR>> {};

/** Registers FingerTree as a Foldable for all monoid and measure combinations. */
template <class T, class MONOID, class MEASURE_POLICY, class ALLOCATOR>
inline constexpr auto foldable_typeclass<beman::fingertree::tree::FingerTree<T, MONOID, MEASURE_POLICY, ALLOCATOR>> =
    FingerTreeFoldableMap<T, MONOID, MEASURE_POLICY, ALLOCATOR>{};

} // namespace beman::fingertree

#endif
