// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises FingerTreePriorityQueue with an explicitly -backed tree.

#include <beman/fingertree/tree/finger_tree_priority_queue.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

using PQ = beman::fingertree::tree::FingerTreePriorityQueue<
    int,
    beman::fingertree::tree::
        FingerTree<int, beman::fingertree::tree::PriorityMonoid<int>, beman::fingertree::tree::PriorityMeasure<int>>>;

TEST_CASE("PriorityQueue - PushAndMinMax") {
    auto pq = PQ{};
    for (int v : {5, 1, 3, 9, 2})
        pq = pq.push(v);

    CHECK(pq.min() == std::optional{1});
    CHECK(pq.max() == std::optional{9});
}

TEST_CASE("PriorityQueue - PopMin") {
    auto pq = PQ::from_values({3, 1, 4, 1, 5, 9, 2, 6});

    auto r1 = pq.pop_min();
    REQUIRE(r1.has_value());
    CHECK(r1->first == 1);

    auto r2 = r1->second.pop_min();
    REQUIRE(r2.has_value());
    CHECK(r2->first == 1);
}

TEST_CASE("PriorityQueue - SizeIsReadOffTheMeasure") {
    // The count rides along in the tuple measure, so size() is O(1): it is
    // the first component of what the tree already caches.
    auto pq = PQ::from_values({3, 1, 4, 1, 5, 9, 2, 6});
    CHECK(pq.size() == 8U);
    CHECK(pq.push(7).size() == 9U);
    auto r = pq.pop_min();
    REQUIRE(r.has_value());
    CHECK(r->second.size() == 7U);
    CHECK(PQ{}.size() == 0U);
}

TEST_CASE("PriorityQueue - PopMax") {
    auto pq = PQ::from_values({3, 1, 4, 1, 5, 9, 2, 6});

    auto r = pq.pop_max();
    REQUIRE(r.has_value());
    CHECK(r->first == 9);
}
