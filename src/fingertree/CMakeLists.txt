add_library(fingertree STATIC "")

target_sources(
  fingertree
  PRIVATE
  fingertree.cpp)

include(GNUInstallDirs)

target_include_directories(fingertree PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_LOWER_PROJECT_NAME}>  # <prefix>/include/fingertree
  )

install(
  TARGETS fingertree
  EXPORT ${TARGETS_EXPORT_NAME}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

string(TOLOWER ${CMAKE_PROJECT_NAME} CMAKE_LOWER_PROJECT_NAME)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_LOWER_PROJECT_NAME}
  FILES_MATCHING PATTERN "*.h"
  )


## Tests
add_executable(fingertree_test "")

target_sources(
  fingertree_test
  PRIVATE
  fingertree.t.cpp)

target_link_libraries(fingertree_test fingertree)
target_link_libraries(fingertree_test gtest)
target_link_libraries(fingertree_test gtest_main)

include(GoogleTest)
gtest_discover_tests(fingertree_test)
