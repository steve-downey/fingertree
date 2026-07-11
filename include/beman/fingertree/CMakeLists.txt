# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

if(BEMAN_FINGERTREE_USE_MODULES)
    target_sources(
        beman.fingertree
        PUBLIC
            FILE_SET CXX_MODULES FILES fingertree.cppm
            FILE_SET HEADERS
                FILES
                    config.hpp
                    fingertree.hpp
                    todo.hpp
                    "${PROJECT_BINARY_DIR}/include/beman/fingertree/config_generated.hpp"
    )
else()
    target_sources(
        beman.fingertree
        PUBLIC
            FILE_SET HEADERS
                FILES
                    config.hpp
                    fingertree.hpp
                    todo.hpp
                    "${PROJECT_BINARY_DIR}/include/beman/fingertree/config_generated.hpp"
    )
endif()
