// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Compile-time regression probe for FingerTree.
//
// FingerTree uses the uniform-elem design (same as FT4) but replaces the
// four-way Digit variant with inplace_vector<ElemPtr,4>.  A single
// <T,Monoid,Meas> use produces one FingerTree instantiation and one
// Elem<T,Monoid> instantiation, regardless of tree depth.
//
// This file is the baseline target for the compile-time comparison:
//   FT2 DWARF count /  DWARF count  ≈  expected template expansion ratio
//
// Any significant increase in this file's DWARF count relative to FT4
// indicates a regression in the uniform-elem design (e.g., an accidental
// depth-typed specialization, or a new intermediate template being introduced).
//
// Measuring compile time (from .build/build-gcc-16):
//
//   touch ../../src/beman/fingertree/tree/finger_tree_compile_probe.cpp
//   time ninja -j1
//   src/beman/fingertree/tree/CMakeFiles/smd_tree_compile_probes.dir/Debug/finger_tree_compile_probe.cpp.o
//
// Counting DWARF class-type entries:
//
//   readelf --debug-dump=info
//   src/beman/fingertree/tree/CMakeFiles/smd_tree_compile_probes.dir/Debug/finger_tree_compile_probe.cpp.o
//   | grep -c DW_TAG_class_type
//
// GCC per-phase time breakdown (add temporarily; produces verbose output):
//   target_compile_options(smd_tree_compile_probes PRIVATE -ftime-report)

#include <beman/fingertree/tree/finger_tree.hpp>

#include <cstddef>

namespace {

struct WeightTag {
    int                          value;
    [[maybe_unused]] friend auto operator==(const WeightTag&, const WeightTag&) -> bool = default;
    friend auto                  operator>=(WeightTag a, WeightTag b) -> bool { return a.value >= b.value; }
};

struct WeightMeasure {
    auto operator()(int x) const -> WeightTag { return WeightTag{x}; }
};

struct weight_monoid {
    using value_type = WeightTag;
    constexpr auto identity() const -> WeightTag { return WeightTag{0}; }
    constexpr auto combine(WeightTag a, WeightTag b) const -> WeightTag { return WeightTag{a.value + b.value}; }
};

} // namespace

void finger_tree_probe_unit_measure() {
    using FT = beman::fingertree::tree::FingerTree<int>;

    auto t = FT{};
    for (int i = 0; i < 16; ++i)
        t = t.snoc(i);
    for (int i = 0; i < 4; ++i)
        t = t.cons(-i);

    auto m   = t.measure();
    auto d   = t.spine_depth();
    auto v   = t.flatten();
    auto h   = t.head();
    auto la  = t.last();
    auto vl  = t.view_l();
    auto vr  = t.view_r();
    auto ta  = t.tail();
    auto in  = t.init();
    auto ap  = t.append(t);
    auto sa  = t.split_at_measure(std::size_t{10});
    auto sp  = t.split([](std::size_t p) { return p > 5; });
    auto sr  = t.search([](std::size_t p) { return p > 5; });
    int  sum = 0;
    t.for_each([&](int x) { sum += x; });

    (void)m;
    (void)d;
    (void)v;
    (void)h;
    (void)la;
    (void)vl;
    (void)vr;
    (void)ta;
    (void)in;
    (void)ap;
    (void)sa;
    (void)sp;
    (void)sr;
    (void)sum;
}

void finger_tree_probe_custom_measure() {
    using FT = beman::fingertree::tree::FingerTree<int, weight_monoid, WeightMeasure>;

    auto t = FT{};
    for (int i = 1; i <= 8; ++i)
        t = t.snoc(i);

    auto sa = t.split_at_measure(WeightTag{15});
    auto ap = t.append(t);
    auto v  = t.flatten();
    auto sp = t.split([](WeightTag p) { return p.value > 10; });

    (void)sa;
    (void)ap;
    (void)v;
    (void)sp;
}
