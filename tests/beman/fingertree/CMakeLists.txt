# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(Catch2 3 REQUIRED)
include(Catch)

# Catch2-based unit tests: one executable per component.
set(BEMAN_FINGERTREE_TESTS
    finger_tree5
    finger_tree5_iterator
    finger_tree5_pmr
    finger_tree5_foldable
    finger_tree5_traversable
    finger_tree_random_access_ft5
    finger_tree_rope_ft5
    finger_tree_priority_queue_ft5
    finger_tree_interval_index_ft5
)

foreach(test ${BEMAN_FINGERTREE_TESTS})
    add_executable(beman.fingertree.tests.${test})
    target_sources(beman.fingertree.tests.${test} PRIVATE ${test}.test.cpp)
    target_link_libraries(
        beman.fingertree.tests.${test}
        PRIVATE beman::fingertree Catch2::Catch2WithMain
    )
    if(BEMAN_FINGERTREE_USE_MODULES)
        set_target_properties(
            beman.fingertree.tests.${test}
            PROPERTIES CXX_MODULE_STD ON
        )
    endif()
    catch_discover_tests(beman.fingertree.tests.${test})
endforeach()

# Self-enforcing proof: zero global heap allocations under a PMR resource.
# Owns its own main(); returns non-zero on any violation.
add_executable(beman.fingertree.tests.pmr_probe)
target_sources(
    beman.fingertree.tests.pmr_probe
    PRIVATE finger_tree5_pmr_probe.test.cpp
)
target_link_libraries(
    beman.fingertree.tests.pmr_probe
    PRIVATE beman::fingertree
)
if(BEMAN_FINGERTREE_USE_MODULES)
    set_target_properties(
        beman.fingertree.tests.pmr_probe
        PROPERTIES CXX_MODULE_STD ON
    )
endif()
add_test(
    NAME beman.fingertree.tests.pmr_probe
    COMMAND beman.fingertree.tests.pmr_probe
)

# Compile-only regression probe (no main()); building it is the test.
add_library(
    beman.fingertree.tests.compile_probe
    OBJECT
    finger_tree5_compile_probe.test.cpp
)
target_link_libraries(
    beman.fingertree.tests.compile_probe
    PRIVATE beman::fingertree
)
if(BEMAN_FINGERTREE_USE_MODULES)
    set_target_properties(
        beman.fingertree.tests.compile_probe
        PROPERTIES CXX_MODULE_STD ON
    )
endif()
