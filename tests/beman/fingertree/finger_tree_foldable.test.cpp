// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/fingertree/tree/finger_tree_foldable.hpp>
#include <beman/fingertree/tree/finger_tree_foldable.hpp> // Re-inclusion check

#include <beman/fingertree/typeclass/foldable.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

TEST_CASE("FingerTreeFoldableTest - LengthAndToVector") {
    using Tree           = beman::fingertree::tree::FingerTree<int>;
    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;
    auto        t        = Tree::from_sequence({1, 2, 3, 4, 5});
    CHECK(foldable.length(t) == 5);
    CHECK(foldable.to_vector(t) == (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST_CASE("FingerTreeFoldableTest - FoldMapAccumulates") {
    using Tree           = beman::fingertree::tree::FingerTree<int>;
    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;
    auto        t        = Tree::from_sequence({1, 2, 3});
    auto        sum = foldable.fold_map([](int x) { return x; }, t, beman::fingertree::typeclass::sum_monoid<int>{});
    CHECK(sum == 6);
}

TEST_CASE("FingerTreeFoldableTest - EmptyTreeHasLengthZero") {
    using Tree           = beman::fingertree::tree::FingerTree<int>;
    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;
    auto        t        = Tree{};
    CHECK(foldable.length(t) == 0);
    CHECK(foldable.empty(t));
    CHECK(foldable.to_vector(t) == (std::vector<int>{}));
}

TEST_CASE("FingerTreeFoldableTest - LargeTreeWalksAllLeaves") {
    // Ensures the recursive Elem unpack inside for_each reaches every leaf
    // even after the spine takes on Node3 structure.
    using Tree           = beman::fingertree::tree::FingerTree<int>;
    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;

    auto t = Tree{};
    for (int i = 1; i <= 100; ++i)
        t = t.snoc(i);

    auto sum = foldable.fold_map([](int x) { return x; }, t, beman::fingertree::typeclass::sum_monoid<int>{});
    CHECK(sum == 5050);
    CHECK(foldable.length(t) == 100);
}

namespace {
template <class M, class F, class T>
concept has_fold_map = requires(const M& m, F f, const T& t) { m.fold_map(f, t); };
} // namespace

TEST_CASE("FingerTreeFoldableTest - IntFoldMapNamesItsMonoid") {
    // int carries several monoids, so no Monoid<int> is registered and the
    // two-argument fold_map is not a candidate; the instance is passed.
    using Tree = beman::fingertree::tree::FingerTree<int>;
    using Map  = std::remove_cvref_t<decltype(beman::fingertree::foldable_typeclass<Tree>)>;
    static_assert(!has_fold_map<Map, decltype([](int x) { return x; }), Tree>);

    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;
    auto        t        = Tree::from_sequence({3, 1, 4, 1, 5});
    CHECK(foldable.fold_map([](int x) { return x; }, t, beman::fingertree::typeclass::max_monoid<int>{}) == 5);
    CHECK(foldable.combine_all(t, beman::fingertree::typeclass::product_monoid<int>{}) == 60);
    CHECK(foldable.find_first(t, [](int x) { return x > 3; }) == std::optional{4});
}

TEST_CASE("FingerTreeFoldableTest - StringFoldMap") {
    using Tree           = beman::fingertree::tree::FingerTree<std::string>;
    const auto& foldable = beman::fingertree::foldable_typeclass<Tree>;
    auto        t        = Tree::from_sequence({"a", "b", "c", "d"});
    auto        joined   = foldable.fold_map([](const std::string& s) { return s; }, t);
    CHECK(joined == "abcd");
}
