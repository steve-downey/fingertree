// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises FingerTreeRandomAccess with an explicitly -backed tree.

#include <beman/fingertree/tree/finger_tree_random_access.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <vector>

// Explicit -backed type alias.
using Seq = beman::fingertree::tree::FingerTreeRandomAccess<
    int,
    beman::fingertree::tree::FingerTree<int,
                                        beman::fingertree::typeclass::sum_monoid<std::size_t>,
                                        beman::fingertree::tree::UnitMeasure<int, std::size_t>>>;

TEST_CASE("RandomAccess - EmptyAndSize") {
    Seq seq;
    CHECK(seq.empty());
    CHECK(seq.size() == 0U);
}

TEST_CASE("RandomAccess - PushBackAndAt") {
    auto seq = Seq{};
    for (int i = 0; i < 10; ++i)
        seq = seq.push_back(i);

    CHECK(seq.size() == 10U);
    for (int i = 0; i < 10; ++i)
        CHECK(seq.at(static_cast<std::size_t>(i)) == std::optional{i});

    CHECK(!seq.at(10).has_value());
}

TEST_CASE("RandomAccess - PushFront") {
    auto seq = Seq{};
    for (int i = 0; i < 5; ++i)
        seq = seq.push_front(i); // 4 3 2 1 0

    CHECK(seq.to_vector() == (std::vector<int>{4, 3, 2, 1, 0}));
}

TEST_CASE("RandomAccess - InsertAndErase") {
    auto seq = Seq::from_sequence({1, 2, 4, 5});
    auto ins = seq.insert(2, 3); // {1, 2, 3, 4, 5}
    CHECK(ins.to_vector() == (std::vector<int>{1, 2, 3, 4, 5}));

    auto era = ins.erase(2); // {1, 2, 4, 5}
    CHECK(era.to_vector() == (std::vector<int>{1, 2, 4, 5}));
}

TEST_CASE("RandomAccess - Update") {
    auto seq = Seq::from_sequence({1, 2, 3, 4, 5});
    auto upd = seq.update(2, 99);
    CHECK(upd.to_vector() == (std::vector<int>{1, 2, 99, 4, 5}));
}

TEST_CASE("RandomAccess - LargeTree") {
    // Exercises FingerTreeRandomAccess with an explicitly -backed tree.
    auto seq = Seq{};
    for (int i = 0; i < 300; ++i)
        seq = seq.push_back(i);

    CHECK(seq.size() == 300U);
    CHECK(seq.at(0) == std::optional{0});
    CHECK(seq.at(150) == std::optional{150});
    CHECK(seq.at(299) == std::optional{299});
}
