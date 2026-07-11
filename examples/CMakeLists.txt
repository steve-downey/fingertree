# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(ALL_EXAMPLES finger_tree)
message("Examples to be built: ${ALL_EXAMPLES}")

foreach(example ${ALL_EXAMPLES})
    add_executable(beman.fingertree.examples.${example})
    target_sources(beman.fingertree.examples.${example} PRIVATE ${example}.cpp)
    target_link_libraries(
        beman.fingertree.examples.${example}
        PRIVATE beman::fingertree
    )
    if(BEMAN_FINGERTREE_USE_MODULES)
        set_target_properties(
            beman.fingertree.examples.${example}
            PROPERTIES CXX_MODULE_STD ON
        )
    endif()
endforeach()
