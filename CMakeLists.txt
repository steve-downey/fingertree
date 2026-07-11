# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.3)

include(infra/cmake/enable-experimental-import-std.cmake)

project(
    beman.fingertree
    DESCRIPTION "A Persistent Sequence Container"
    LANGUAGES CXX
    VERSION 0.1.0
)

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_FINGERTREE_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_FINGERTREE_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

option(BEMAN_FINGERTREE_USE_MODULES "Provide beman.fingertree as a C++ module" OFF)

if(BEMAN_FINGERTREE_USE_MODULES)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/include/beman/fingertree/config_generated.hpp.in"
    "${PROJECT_BINARY_DIR}/include/beman/fingertree/config_generated.hpp"
    @ONLY
)

# for find of beman_install_library and configure_build_telemetry
include(infra/cmake/beman-install-library.cmake)
include(infra/cmake/BuildTelemetryConfig.cmake)

if(BEMAN_FINGERTREE_USE_MODULES)
    add_library(beman.fingertree STATIC)
else()
    add_library(beman.fingertree INTERFACE)
endif()
add_library(beman::fingertree ALIAS beman.fingertree)

if(BEMAN_FINGERTREE_USE_MODULES)
    target_sources(
        beman.fingertree
        PUBLIC
            FILE_SET CXX_MODULES
            FILE_SET HEADERS
                BASE_DIRS
                    "${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "${CMAKE_CURRENT_BINARY_DIR}/include"
    )
    set_target_properties(beman.fingertree PROPERTIES CXX_MODULE_STD ON)
    target_compile_features(beman.fingertree PUBLIC cxx_std_23)
else()
    target_sources(
        beman.fingertree
        PUBLIC
            FILE_SET HEADERS
                BASE_DIRS
                    "${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "${CMAKE_CURRENT_BINARY_DIR}/include"
    )
    set_target_properties(
        beman.fingertree
        PROPERTIES VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL}
    )
endif()

add_subdirectory(include/beman/fingertree)

beman_install_library(beman.fingertree TARGETS beman.fingertree)
configure_build_telemetry()

if(BEMAN_FINGERTREE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/fingertree)
endif()

if(BEMAN_FINGERTREE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Coverage
configure_file("cmake/gcovr.cfg.in" gcovr.cfg @ONLY)

add_custom_target(
    process_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running gcovr to process coverage results"
    COMMAND mkdir -p coverage
    COMMAND gcovr --config gcovr.cfg .
)
