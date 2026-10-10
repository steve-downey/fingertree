# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(BEMAN_FINGERTREE_PUBLIC_HEADERS
    config.hpp
    fingertree.hpp
    fixpoint/overloaded.hpp
    typeclass/typeclass_base.hpp
    typeclass/functor.hpp
    typeclass/applicative.hpp
    typeclass/foldable.hpp
    typeclass/traversable.hpp
    typeclass/monoid.hpp
    tree/finger_tree.hpp
    tree/finger_tree_iterator.hpp
    tree/finger_tree_pmr.hpp
    tree/finger_tree_foldable.hpp
    tree/finger_tree_traversable.hpp
    tree/finger_tree_random_access.hpp
    tree/finger_tree_rope.hpp
    tree/finger_tree_priority_queue.hpp
    tree/finger_tree_interval_index.hpp
    "${PROJECT_BINARY_DIR}/include/beman/fingertree/config_generated.hpp"
)

if(BEMAN_FINGERTREE_USE_MODULES)
    target_sources(
        beman.fingertree
        PUBLIC
            FILE_SET CXX_MODULES FILES fingertree.cppm
            FILE_SET HEADERS FILES ${BEMAN_FINGERTREE_PUBLIC_HEADERS}
    )
else()
    target_sources(
        beman.fingertree
        PUBLIC FILE_SET HEADERS FILES ${BEMAN_FINGERTREE_PUBLIC_HEADERS}
    )
endif()
