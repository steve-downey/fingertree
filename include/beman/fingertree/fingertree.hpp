// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_FINGERTREE_FINGERTREE_HPP
#define BEMAN_FINGERTREE_FINGERTREE_HPP

#include <beman/fingertree/config.hpp>

#if BEMAN_FINGERTREE_USE_MODULES() && !defined(BEMAN_FINGERTREE_INCLUDED_FROM_INTERFACE_UNIT)

import beman.fingertree;

#else

    // Persistent measured sequence (Paper C): the container, its
    // iterator/container and PMR/AllocatorAware layers, the typeclass
    // adapters, and the four capability wrappers.
    #include <beman/fingertree/tree/finger_tree5.hpp>
    #include <beman/fingertree/tree/finger_tree5_iterator.hpp>
    #include <beman/fingertree/tree/finger_tree5_pmr.hpp>
    #include <beman/fingertree/tree/finger_tree5_foldable.hpp>
    #include <beman/fingertree/tree/finger_tree5_traversable.hpp>
    #include <beman/fingertree/tree/finger_tree_random_access.hpp>
    #include <beman/fingertree/tree/finger_tree_rope.hpp>
    #include <beman/fingertree/tree/finger_tree_priority_queue.hpp>
    #include <beman/fingertree/tree/finger_tree_interval_index.hpp>

#endif // BEMAN_FINGERTREE_USE_MODULES() &&
       // !defined(BEMAN_FINGERTREE_INCLUDED_FROM_INTERFACE_UNIT)

#endif // BEMAN_FINGERTREE_FINGERTREE_HPP
