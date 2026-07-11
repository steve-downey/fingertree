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

option(
    BEMAN_FINGERTREE_USE_MODULES
    "Provide beman.fingertree as a C++ module"
    OFF
)

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

# beman.optional supplies the Applicative/Functor typeclass instances that the
# finger-tree wrappers specialize against.
find_package(beman.optional REQUIRED)

if(BEMAN_FINGERTREE_USE_MODULES)
    add_library(beman.fingertree STATIC)
else()
    add_library(beman.fingertree INTERFACE)
endif()
add_library(beman::fingertree ALIAS beman.fingertree)

# Clang-only warning suppressions applied in our build tree (not exported to
# consumers). Both are spurious for this code base:
#  * -Wdeprecated-declarations: libstdc++ 16's <inplace_vector> internally uses
#    std::is_trivial_v, deprecated in C++26; Clang surfaces it from our
#    inplace_vector instantiation. GCC does not warn.
#  * -Wmissing-braces: a low-value, false-positive-prone diagnostic that fires
#    (instantiation-dependent) on the aggregate Elem initialization.
target_compile_options(
    beman.fingertree
    INTERFACE
        $<BUILD_INTERFACE:$<$<CXX_COMPILER_ID:Clang>:-Wno-deprecated-declarations;-Wno-missing-braces>>
)

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
    # finger_tree5 requires std::inplace_vector (C++26); no fallback exists.
    target_compile_features(beman.fingertree PUBLIC cxx_std_26)
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
    # finger_tree5 requires std::inplace_vector (C++26); no fallback exists.
    target_compile_features(beman.fingertree INTERFACE cxx_std_26)
endif()

add_subdirectory(include/beman/fingertree)

if(BEMAN_FINGERTREE_USE_MODULES)
    target_link_libraries(beman.fingertree PUBLIC beman::optional)
else()
    target_link_libraries(beman.fingertree INTERFACE beman::optional)
endif()

# find_package(beman.optional) above pulls in the dependency as a subproject,
# which re-includes its own (possibly older) beman_install_library definition.
# Re-include ours so the DEPENDENCIES argument below is understood.
include(infra/cmake/beman-install-library.cmake)

beman_install_library(
    beman.fingertree
    TARGETS beman.fingertree
    DEPENDENCIES "beman.optional"
)
configure_build_telemetry()

if(BEMAN_FINGERTREE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/fingertree)
endif()

if(BEMAN_FINGERTREE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Coverage
# A fetched beman dependency (e.g. beman.optional) may already define a global
# process_coverage target; guard against a duplicate-target configure error.
if(NOT TARGET process_coverage)
    configure_file("cmake/gcovr.cfg.in" gcovr.cfg @ONLY)

    add_custom_target(
        process_coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running gcovr to process coverage results"
        COMMAND mkdir -p coverage
        COMMAND gcovr --config gcovr.cfg .
    )
endif()
