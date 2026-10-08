// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_PMR_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_PMR_HPP

// Convenience aliases for FingerTree backed by
// std::pmr::polymorphic_allocator.
//
// Usage:
//   #include <beman/fingertree/tree/finger_tree_pmr.hpp>
//
//   std::pmr::monotonic_buffer_resource buf;
//   beman::fingertree::tree::pmr::FingerTree<int> t(&buf);
//   t = t.snoc(42);
//
// All node allocations (Leaf, Node2/Node3 Elem, Deep, SpinePtr) go through
// the polymorphic allocator.  Structural sharing works across trees with
// different allocators because each node stores its own allocator in the
// shared_ptr control block.

#include <beman/fingertree/tree/finger_tree.hpp>

#include <memory_resource>

namespace beman::fingertree::tree::pmr {

template <typename T,
          typename MONOID         = beman::fingertree::typeclass::sum_monoid<std::size_t>,
          typename MEASURE_POLICY = UnitMeasure<T, ft::tag_t<MONOID>>>
using FingerTree =
    beman::fingertree::tree::FingerTree<T, MONOID, MEASURE_POLICY, std::pmr::polymorphic_allocator<std::byte>>;

} // namespace beman::fingertree::tree::pmr

#endif
