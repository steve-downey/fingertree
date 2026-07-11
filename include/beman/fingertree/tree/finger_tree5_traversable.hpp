// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE5_TRAVERSABLE_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE5_TRAVERSABLE_HPP

// Traversable constraint for FingerTree5:
// - Materialization: traverse materializes via flatten() into a vector.
// - Preservation: monoid measure semantics are preserved through the traversal.
// - Reconstruction: results are rebuilt via FingerTree5<U>::from_sequence(),
//   re-applying the default UnitMeasure5 to the result type.
// - Applicative semantics: traversals run left-to-right independent of tree
//   structure.
//
// Rationale matches finger_tree_traversable.hpp: traversal is not a primary
// performance path, so O(n) materialization is acceptable. Wrappers that need
// structure-preserving traversal can override.

#include <beman/fingertree/tree/finger_tree5.hpp>
#include <beman/fingertree/typeclass/applicative.hpp>
#include <beman/fingertree/typeclass/traversable.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace beman::fingertree {

/** Traversable typeclass implementation for FingerTree5; materialises via
 * flatten() then reconstructs with from_sequence(); O(n).
 */
template <class T, class TAG_TYPE, class MEASURE_POLICY, class ALLOCATOR>
struct FingerTree5TraversableImpl {
    using element_type = T;

    template <class APPLICATIVE, class F>
    auto traverse(this auto&&,
                  const APPLICATIVE&                                                                  applicative,
                  F&&                                                                                 function,
                  const beman::fingertree::tree::FingerTree5<T, TAG_TYPE, MEASURE_POLICY, ALLOCATOR>& tree) {
        using Context = remove_cvref_t<std::invoke_result_t<F, const T&>>;
        using U       = beman::fingertree::applicative_value_t<Context>;

        auto accumulated = applicative.pure(std::vector<U>{});

        for (const auto& value : tree.flatten()) {
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
                return beman::fingertree::tree::FingerTree5<U>::from_sequence(std::move(values));
            },
            std::move(accumulated));
    }
};

/** Traversable typeclass map entry for FingerTree5. */
template <class T, class TAG_TYPE, class MEASURE_POLICY, class ALLOCATOR>
struct FingerTree5TraversableMap : Traversable<FingerTree5TraversableImpl<T, TAG_TYPE, MEASURE_POLICY, ALLOCATOR>> {
    using FingerTree5TraversableImpl<T, TAG_TYPE, MEASURE_POLICY, ALLOCATOR>::traverse;
};

/** Registers FingerTree5 as a Traversable for all tag and measure
 * combinations.
 */
template <class T, class TAG_TYPE, class MEASURE_POLICY, class ALLOCATOR>
inline constexpr auto
    traversable_typeclass<beman::fingertree::tree::FingerTree5<T, TAG_TYPE, MEASURE_POLICY, ALLOCATOR>> =
        FingerTree5TraversableMap<T, TAG_TYPE, MEASURE_POLICY, ALLOCATOR>{};

} // namespace beman::fingertree

#endif
