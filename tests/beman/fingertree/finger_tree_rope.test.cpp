// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include <beman/fingertree/tree/finger_tree_rope.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using Rope = beman::fingertree::tree::FingerTreeRope<
    beman::fingertree::tree::FingerTree<std::string,
                                        beman::fingertree::typeclass::sum_monoid<std::size_t>,
                                        beman::fingertree::tree::RopeChunkMeasure>>;

TEST_CASE("Rope - FromTextAndToString") {
    auto r = Rope::from_text("hello world", 4);
    CHECK(r.to_string() == "hello world");
    CHECK(r.size_bytes() == 11U);
}

TEST_CASE("Rope - InsertAndErase") {
    auto r   = Rope::from_text("hello world");
    auto ins = r.insert(5, ", dear");
    CHECK(ins.to_string() == "hello, dear world");

    auto era = ins.erase(5, 6);
    CHECK(era.to_string() == "hello world");
}

TEST_CASE("Rope - Replace") {
    auto r   = Rope::from_text("foo bar baz");
    auto rep = r.replace(4, 3, "qux");
    CHECK(rep.to_string() == "foo qux baz");
}
