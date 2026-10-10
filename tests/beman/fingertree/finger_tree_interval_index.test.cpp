// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises FingerTreeIntervalIndex with an explicitly -backed tree.

#include <beman/fingertree/tree/finger_tree_interval_index.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using Entry   = beman::fingertree::tree::Interval<std::string>;
using Monoid  = beman::fingertree::tree::IntervalMonoid;
using Measure = beman::fingertree::tree::IntervalMeasure<std::string>;

using Idx =
    beman::fingertree::tree::FingerTreeIntervalIndex<std::string,
                                                     beman::fingertree::tree::FingerTree<Entry, Monoid, Measure>>;

TEST_CASE("IntervalIndex - QueryPoint") {
    auto idx = Idx{};
    idx      = idx.insert({0, 10, "A"});
    idx      = idx.insert({5, 15, "B"});
    idx      = idx.insert({12, 20, "C"});

    auto r7 = idx.query_point(7);
    REQUIRE(r7.size() == 2U);
    CHECK(r7[0] == "A");
    CHECK(r7[1] == "B");

    auto r14 = idx.query_point(14);
    REQUIRE(r14.size() == 2U);
    CHECK(r14[0] == "B");
    CHECK(r14[1] == "C");
}

TEST_CASE("IntervalIndex - QueryOverlap") {
    auto idx = Idx{};
    idx      = idx.insert({0, 5, "A"});
    idx      = idx.insert({3, 8, "B"});
    idx      = idx.insert({7, 12, "C"});

    // A=[0,5): 0<9 ∧ 4<5 → overlaps; B=[3,8): 3<9 ∧ 4<8 → overlaps;
    // C=[7,12): 7<9 ∧ 4<12 → overlaps.  All three match [4,9).
    auto r = idx.query_overlap(4, 9);
    REQUIRE(r.size() == 3U);
    CHECK(r[0] == "A");
    CHECK(r[1] == "B");
    CHECK(r[2] == "C");
}
