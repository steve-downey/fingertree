// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises FingerTreePriorityQueue with an explicitly FT5-backed tree.

#include <beman/fingertree/tree/finger_tree_priority_queue.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

using FT5PQ = beman::fingertree::tree::FingerTreePriorityQueue<
    int,
    beman::fingertree::tree::
        FingerTree5<int, beman::fingertree::tree::PriorityTag<int>, beman::fingertree::tree::PriorityMeasure<int>>>;

TEST_CASE("PriorityQueueFT5 - PushAndMinMax") {
    auto pq = FT5PQ{};
    for (int v : {5, 1, 3, 9, 2})
        pq = pq.push(v);

    CHECK(pq.min() == std::optional{1});
    CHECK(pq.max() == std::optional{9});
}

TEST_CASE("PriorityQueueFT5 - PopMin") {
    auto pq = FT5PQ::from_values({3, 1, 4, 1, 5, 9, 2, 6});

    auto r1 = pq.pop_min();
    REQUIRE(r1.has_value());
    CHECK(r1->first == 1);

    auto r2 = r1->second.pop_min();
    REQUIRE(r2.has_value());
    CHECK(r2->first == 1);
}

TEST_CASE("PriorityQueueFT5 - PopMax") {
    auto pq = FT5PQ::from_values({3, 1, 4, 1, 5, 9, 2, 6});

    auto r = pq.pop_max();
    REQUIRE(r.has_value());
    CHECK(r->first == 9);
}
