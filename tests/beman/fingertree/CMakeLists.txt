# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(Catch2 3 REQUIRED)

add_executable(beman.fingertree.tests.todo)
target_sources(beman.fingertree.tests.todo PRIVATE todo.test.cpp)
target_link_libraries(
    beman.fingertree.tests.todo
    PRIVATE beman::fingertree Catch2::Catch2WithMain
)
if(BEMAN_EXEMPLAR_USE_MODULES)
    set_target_properties(
        beman.fingertree.tests.todo
        PROPERTIES CXX_MODULE_STD ON
    )
endif()

include(Catch)
catch_discover_tests(beman.fingertree.tests.todo)
