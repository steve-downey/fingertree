// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/fingertree/config.hpp>
#include <catch2/catch_all.hpp>
#include <beman/fingertree/todo.hpp>

TEST_CASE("todo", "[fingertree::todo]") {
    const bool todo = true;
    CHECK(todo);
}
