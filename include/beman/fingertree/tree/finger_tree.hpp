// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TREE_FINGER_TREE_HPP
#define BEMAN_FINGERTREE_TREE_FINGER_TREE_HPP

// Hinze-Paterson 2-3 finger tree — uniform-elem, unified-digit implementation.
//
// Like finger_tree4, the spine is the same FingerTree type at every depth
// (shared_ptr<const FingerTree>), and Elem<T,MONOID> is the self-recursive
// variant<Leaf, Node2, Node3> carrying its cached measure.
//
// The structural change vs finger_tree4 is the digit representation:
// std::inplace_vector<ElemPtr<T,MONOID>, 4> with an explicit size, replacing
// finger_tree4's variant<One, Two, Three, Four>.  Cons/snoc/view/split/concat
// branch on size() rather than dispatching through a four-way std::visit at
// every level.

#include <beman/fingertree/fixpoint/overloaded.hpp>
#include <beman/fingertree/typeclass/monoid.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <inplace_vector>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace beman::fingertree::tree {

// ============================================================================
//                              DETAIL TYPES
// ============================================================================

namespace ft {

using beman::fingertree::fixpoint::overloaded;

// -- Measure vocabulary -------------------------------------------------------
//
// The tree is keyed on a monoid *instance* type MONOID, not on the measure
// value type: a cached measure is valid only under the monoid that computed
// it, so two trees over the same value type under different monoids (count
// and max over std::size_t, say) must not share Elem nodes. The value type is
// derived from the instance; the instance itself is stored on the tree and
// passed down to every helper that combines. See beman.transpose
// docs/decisions.md#monoid-selection.

template <typename MONOID>
using tag_t = beman::fingertree::typeclass::monoid_value_t<MONOID>;

// -- Elem: self-recursive element type ---------------------------------------

template <typename T, typename MONOID>
struct Elem;

template <typename T, typename MONOID>
using ElemPtr = std::shared_ptr<const Elem<T, MONOID>>;

template <typename T, typename MONOID>
struct Elem {
    tag_t<MONOID> d_measure;

    struct Leaf {
        T d_value;
    };
    struct Node2 {
        ElemPtr<T, MONOID> a;
        ElemPtr<T, MONOID> b;
    };
    struct Node3 {
        ElemPtr<T, MONOID> a;
        ElemPtr<T, MONOID> b;
        ElemPtr<T, MONOID> c;
    };

    std::variant<Leaf, Node2, Node3> d_data;
};

template <typename T, typename MONOID>
auto elem_measure(const ElemPtr<T, MONOID>& ep) -> tag_t<MONOID> {
    return ep->d_measure;
}

template <typename T, typename MONOID>
auto is_leaf(const ElemPtr<T, MONOID>& ep) -> bool {
    return std::holds_alternative<typename Elem<T, MONOID>::Leaf>(ep->d_data);
}

template <typename T, typename MONOID>
auto leaf_value(const ElemPtr<T, MONOID>& ep) -> const T& {
    return std::get<typename Elem<T, MONOID>::Leaf>(ep->d_data).d_value;
}

template <typename T, typename MONOID, typename Alloc, typename MeasFn>
auto make_leaf(const Alloc& alloc, MeasFn&& mf, T value) -> ElemPtr<T, MONOID> {
    // value is taken by-value: measured first (lvalue), then moved into Leaf.
    using EA = typename std::allocator_traits<Alloc>::template rebind_alloc<Elem<T, MONOID>>;
    EA   ea(alloc);
    auto m = mf(value);
    return std::allocate_shared<const Elem<T, MONOID>>(
        ea, Elem<T, MONOID>{std::move(m), typename Elem<T, MONOID>::Leaf{std::move(value)}});
}

template <typename T, typename MONOID, typename Alloc>
auto make_node2(const Alloc& alloc, const MONOID& monoid, ElemPtr<T, MONOID> a, ElemPtr<T, MONOID> b)
    -> ElemPtr<T, MONOID> {
    using EA = typename std::allocator_traits<Alloc>::template rebind_alloc<Elem<T, MONOID>>;
    EA   ea(alloc);
    auto m = monoid.combine(a->d_measure, b->d_measure);
    return std::allocate_shared<const Elem<T, MONOID>>(
        ea, Elem<T, MONOID>{std::move(m), typename Elem<T, MONOID>::Node2{std::move(a), std::move(b)}});
}

template <typename T, typename MONOID, typename Alloc>
auto make_node3(
    const Alloc& alloc, const MONOID& monoid, ElemPtr<T, MONOID> a, ElemPtr<T, MONOID> b, ElemPtr<T, MONOID> c)
    -> ElemPtr<T, MONOID> {
    using EA = typename std::allocator_traits<Alloc>::template rebind_alloc<Elem<T, MONOID>>;
    EA   ea(alloc);
    auto m = monoid.combine(monoid.combine(a->d_measure, b->d_measure), c->d_measure);
    return std::allocate_shared<const Elem<T, MONOID>>(
        ea, Elem<T, MONOID>{std::move(m), typename Elem<T, MONOID>::Node3{std::move(a), std::move(b), std::move(c)}});
}

template <typename T, typename MONOID>
void flatten_elem(const ElemPtr<T, MONOID>& ep, std::vector<T>& out) {
    using E = Elem<T, MONOID>;
    std::visit(overloaded{[&](const typename E::Leaf& lf) { out.push_back(lf.d_value); },
                          [&](const typename E::Node2& n) {
                              flatten_elem(n.a, out);
                              flatten_elem(n.b, out);
                          },
                          [&](const typename E::Node3& n) {
                              flatten_elem(n.a, out);
                              flatten_elem(n.b, out);
                              flatten_elem(n.c, out);
                          }},
               ep->d_data);
}

template <typename T, typename MONOID, typename F>
void for_each_elem(const ElemPtr<T, MONOID>& ep, const F& fn) {
    using E = Elem<T, MONOID>;
    std::visit(overloaded{[&](const typename E::Leaf& lf) { fn(lf.d_value); },
                          [&](const typename E::Node2& n) {
                              for_each_elem(n.a, fn);
                              for_each_elem(n.b, fn);
                          },
                          [&](const typename E::Node3& n) {
                              for_each_elem(n.a, fn);
                              for_each_elem(n.b, fn);
                              for_each_elem(n.c, fn);
                          }},
               ep->d_data);
}

// -- Digit: 1..4 ElemPtrs in an inplace_vector -------------------------------
//
// Invariant: when a Digit appears inside a Deep node, 1 <= size() <= 4.
// An empty Digit is only a transient construction artifact in helpers.

template <typename T, typename MONOID>
using Digit = std::inplace_vector<ElemPtr<T, MONOID>, 4>;

template <typename T, typename MONOID>
auto digit_measure(const MONOID& monoid, const Digit<T, MONOID>& d) -> tag_t<MONOID> {
    tag_t<MONOID> m = monoid.identity();
    for (const auto& ep : d)
        m = monoid.combine(m, ep->d_measure);
    return m;
}

template <typename T, typename MONOID>
auto digit_with_pushed_front(const Digit<T, MONOID>& d, ElemPtr<T, MONOID> x) -> Digit<T, MONOID> {
    assert(d.size() < 4);
    Digit<T, MONOID> out;
    out.push_back(std::move(x));
    for (const auto& e : d)
        out.push_back(e);
    return out;
}

template <typename T, typename MONOID>
auto digit_with_pushed_back(const Digit<T, MONOID>& d, ElemPtr<T, MONOID> x) -> Digit<T, MONOID> {
    assert(d.size() < 4);
    Digit<T, MONOID> out;
    for (const auto& e : d)
        out.push_back(e);
    out.push_back(std::move(x));
    return out;
}

template <typename T, typename MONOID>
auto digit_tail(const Digit<T, MONOID>& d) -> Digit<T, MONOID> {
    assert(!d.empty());
    Digit<T, MONOID> out;
    for (std::size_t i = 1; i < d.size(); ++i)
        out.push_back(d[i]);
    return out;
}

template <typename T, typename MONOID>
auto digit_init(const Digit<T, MONOID>& d) -> Digit<T, MONOID> {
    assert(!d.empty());
    Digit<T, MONOID> out;
    for (std::size_t i = 0; i + 1 < d.size(); ++i)
        out.push_back(d[i]);
    return out;
}

// Group a flat sequence of ElemPtrs into Node2/Node3 nodes for the next
// spine level.  Input size must be >= 2.
//
// Input capacity is bounded by 12 (4 right-digit + 4 middle + 4 left-digit
// in app3's Deep-Deep path).  Output has at most 4 nodes from 12 inputs;
// capacity 6 gives a safety margin.
template <typename T, typename MONOID, typename Alloc>
auto nodes_from(const Alloc& alloc, const MONOID& monoid, std::inplace_vector<ElemPtr<T, MONOID>, 12> elems)
    -> std::inplace_vector<ElemPtr<T, MONOID>, 6> {
    std::inplace_vector<ElemPtr<T, MONOID>, 6> result;
    auto                                       n = elems.size();
    std::size_t                                i = 0;
    while (n - i > 4) {
        result.push_back(make_node3<T, MONOID>(
            alloc, monoid, std::move(elems[i]), std::move(elems[i + 1]), std::move(elems[i + 2])));
        i += 3;
    }
    switch (n - i) {
    case 2:
        result.push_back(make_node2<T, MONOID>(alloc, monoid, std::move(elems[i]), std::move(elems[i + 1])));
        break;
    case 3:
        result.push_back(make_node3<T, MONOID>(
            alloc, monoid, std::move(elems[i]), std::move(elems[i + 1]), std::move(elems[i + 2])));
        break;
    case 4:
        result.push_back(make_node2<T, MONOID>(alloc, monoid, std::move(elems[i]), std::move(elems[i + 1])));
        result.push_back(make_node2<T, MONOID>(alloc, monoid, std::move(elems[i + 2]), std::move(elems[i + 3])));
        break;
    default:
        assert(false && "nodes_from: invalid count");
        std::unreachable();
    }
    return result;
}

// Unwrap a Node elem into a 2-or-3 element digit.
template <typename T, typename MONOID>
auto elem_to_digit(const ElemPtr<T, MONOID>& ep) -> Digit<T, MONOID> {
    using E = Elem<T, MONOID>;
    Digit<T, MONOID> out;
    std::visit(overloaded{[&](const typename E::Leaf&) {
                              assert(false && "elem_to_digit called on Leaf");
                              std::unreachable();
                          },
                          [&](const typename E::Node2& n) {
                              out.push_back(n.a);
                              out.push_back(n.b);
                          },
                          [&](const typename E::Node3& n) {
                              out.push_back(n.a);
                              out.push_back(n.b);
                              out.push_back(n.c);
                          }},
               ep->d_data);
    return out;
}

} // namespace ft

// ============================================================================
//                            FINGER_TREE
// ============================================================================

template <typename T, typename TAG_TYPE>
struct UnitMeasure {
    auto operator()(const T&) const -> TAG_TYPE { return TAG_TYPE{1}; }
};

// Forward declaration so FingerTree can declare begin()/end() members
// whose return type is FingerTreeIterator.  The full definition is in
// finger_tree_iterator.hpp, included at the bottom of this file.
template <typename T, typename MONOID, typename MP, typename Alloc = std::allocator<std::byte>>
class FingerTreeIterator;

// MONOID is a monoid instance type over the measure value type (the tag);
// MEASURE_POLICY maps an element to a tag. The default is the element count:
// every element measures 1 under addition. A tree with a different monoid
// names its instance -- `FingerTree<int, typeclass::max_monoid<std::size_t>,
// MaxMeasure>` -- and the instance may carry state, in which case it is
// passed to the constructor or to from_sequence and stored on the tree.
template <typename T,
          typename MONOID         = beman::fingertree::typeclass::sum_monoid<std::size_t>,
          typename MEASURE_POLICY = UnitMeasure<T, ft::tag_t<MONOID>>,
          typename ALLOCATOR      = std::allocator<std::byte>>
class FingerTree {
    template <typename, typename, typename, typename>
    friend class FingerTreeIterator; // iterator needs access to Deep, Repr,
                                     // etc.

    using Tag  = ft::tag_t<MONOID>;
    using Meas = MEASURE_POLICY;
    using E    = ft::Elem<T, MONOID>;
    using EP   = ft::ElemPtr<T, MONOID>;

    // ElemPtr vector using the rebind-allocated allocator so temporary
    // scaffolding vectors (from_sequence, from_elems_impl) don't hit the
    // global heap when a custom allocator is in use.
    using EpAlloc = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<EP>;
    using EpVec   = std::vector<EP, EpAlloc>;
    using Digit   = ft::Digit<T, MONOID>;

    using SpinePtr = std::shared_ptr<const FingerTree>;

    static auto meas_fn() -> Meas { return Meas{}; }

    // -- Representation ------------------------------------------------------

    struct Empty {};

    struct Single {
        EP d_elem;
    };

    struct Deep {
        Tag      d_measure;
        Digit    d_left;
        SpinePtr d_spine;
        Digit    d_right;
    };

    using DeepPtr = std::shared_ptr<const Deep>;
    using Repr    = std::variant<Empty, Single, DeepPtr>;
    Repr                            d_repr;
    [[no_unique_address]] ALLOCATOR d_alloc;
    [[no_unique_address]] MONOID    d_monoid;

    // Private constructor used by factory helpers; sets repr, alloc and
    // monoid together. Every tree a helper builds inherits the instance of
    // the tree it was built from, so a stateful instance travels with its
    // nodes.
    FingerTree(Repr r, const ALLOCATOR& alloc, const MONOID& monoid)
        : d_repr(std::move(r)), d_alloc(alloc), d_monoid(monoid) {}

    // -- Allocation helpers --------------------------------------------------

    // Returns true when both allocators are the same resource.
    // is_always_equal short-circuits at compile time for stateless allocators
    // (e.g. std::allocator) so runtime comparison is only done for stateful
    // ones (e.g. pmr::polymorphic_allocator).
    static auto alloc_equal(const ALLOCATOR& a, const ALLOCATOR& b) noexcept -> bool {
        if constexpr (std::allocator_traits<ALLOCATOR>::is_always_equal::value)
            return true;
        else
            return a == b;
    }

    // Allocate a spine shell (the FingerTree wrapper that holds a DeepPtr)
    // entirely from the custom allocator without triggering the uses_allocator
    // coherency-rebuild protocol.
    //
    // The spine shells contain Node2/Node3 elements, not leaf T values, so
    // flatten()+from_sequence() would reconstruct a structurally different
    // tree.  The coherency rebuild is therefore incorrect for spine shells
    // and must be bypassed.
    //
    // Technique: placement new into allocator-owned memory, then wrap in a
    // shared_ptr with a custom deleter.  ::new bypasses uses_allocator_
    // construction_args (which would otherwise fire the public extended
    // constructor and run the coherency check).  Both the shell object and
    // the shared_ptr control block are allocated from alloc.  Two separate
    // allocations — unavoidable without allocate_shared, which triggers the
    // unwanted protocol.
    //
    // Precondition: t.d_alloc == alloc (always true at call sites; any
    // future breakage manifests as allocator mismatch rather than silently
    // corrupting the tree structure).
    static auto allocate_spine(const ALLOCATOR& alloc, FingerTree t) -> SpinePtr {
        using FTA = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<FingerTree>;
        FTA         fta(alloc);
        FingerTree* raw = std::allocator_traits<FTA>::allocate(fta, 1);
        try {
            ::new (raw) FingerTree(std::move(t)); // move ctor, no uses_allocator
        } catch (...) {
            std::allocator_traits<FTA>::deallocate(fta, raw, 1);
            throw;
        }
        // Control block allocated from fta (custom alloc).
        return SpinePtr(
            static_cast<const FingerTree*>(raw),
            [alloc](const FingerTree* p) mutable {
                FTA   fa(alloc);
                auto* mp = const_cast<FingerTree*>(p);
                std::allocator_traits<FTA>::destroy(fa, mp);
                std::allocator_traits<FTA>::deallocate(fa, mp, 1);
            },
            fta);
    }

    auto allocate_spine(FingerTree t) const -> SpinePtr { return allocate_spine(d_alloc, std::move(t)); }

    auto wrap_leaf(T value) const -> EP { return ft::make_leaf<T, MONOID>(d_alloc, meas_fn(), std::move(value)); }

    static auto make_single(const ALLOCATOR& alloc, const MONOID& monoid, EP elem) -> FingerTree {
        return FingerTree(Repr{Single{std::move(elem)}}, alloc, monoid);
    }

    // Allocator-aware make_deep: uses alloc for the Deep allocation.
    static auto make_deep(const ALLOCATOR& alloc, const MONOID& monoid, Digit left, SpinePtr spine, Digit right)
        -> FingerTree {
        using DA = typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<Deep>;
        DA   da(alloc);
        auto m = monoid.combine(
            monoid.combine(ft::digit_measure(monoid, left), spine ? spine->measure() : monoid.identity()),
            ft::digit_measure(monoid, right));
        return FingerTree(Repr{std::allocate_shared<const Deep>(
                              da, Deep{std::move(m), std::move(left), std::move(spine), std::move(right)})},
                          alloc,
                          monoid);
    }

    // Convenience overload for call sites that already have an allocator in
    // scope.
    auto make_deep(Digit left, SpinePtr spine, Digit right) const -> FingerTree {
        return make_deep(d_alloc, d_monoid, std::move(left), std::move(spine), std::move(right));
    }

    // -- Rebalancing helpers used by view ------------------------------------

    static auto make_digit(std::initializer_list<EP> elems) -> Digit {
        Digit d;
        for (const auto& e : elems)
            d.push_back(e);
        return d;
    }

    static auto digit_to_tree(const ALLOCATOR& alloc, const MONOID& monoid, const Digit& d) -> FingerTree {
        assert(!d.empty());
        switch (d.size()) {
        case 1:
            return make_single(alloc, monoid, d[0]);
        case 2:
            return make_deep(alloc, monoid, make_digit({d[0]}), SpinePtr{}, make_digit({d[1]}));
        case 3:
            return make_deep(alloc, monoid, make_digit({d[0], d[1]}), SpinePtr{}, make_digit({d[2]}));
        case 4:
            return make_deep(alloc, monoid, make_digit({d[0], d[1]}), SpinePtr{}, make_digit({d[2], d[3]}));
        default:
            std::unreachable();
        }
    }

    static auto deep_l(const ALLOCATOR& alloc, const MONOID& monoid, SpinePtr spine, Digit right) -> FingerTree {
        if (!spine || spine->is_empty())
            return digit_to_tree(alloc, monoid, right);
        auto vl = spine->view_l_internal();
        assert(vl.has_value());
        auto     new_left = ft::elem_to_digit<T, MONOID>(vl->d_elem);
        SpinePtr new_spine;
        if (!vl->d_rest.is_empty())
            new_spine = allocate_spine(alloc, std::move(vl->d_rest));
        return make_deep(alloc, monoid, std::move(new_left), std::move(new_spine), std::move(right));
    }

    static auto deep_r(const ALLOCATOR& alloc, const MONOID& monoid, Digit left, SpinePtr spine) -> FingerTree {
        if (!spine || spine->is_empty())
            return digit_to_tree(alloc, monoid, left);
        auto vr = spine->view_r_internal();
        assert(vr.has_value());
        auto     new_right = ft::elem_to_digit<T, MONOID>(vr->d_elem);
        SpinePtr new_spine;
        if (!vr->d_rest.is_empty())
            new_spine = allocate_spine(alloc, std::move(vr->d_rest));
        return make_deep(alloc, monoid, std::move(left), std::move(new_spine), std::move(new_right));
    }

    // -- Internal view (works on ElemPtrs) -----------------------------------

    struct InternalView {
        EP         d_elem;
        FingerTree d_rest;
    };

    auto view_l_internal() const -> std::optional<InternalView> {
        return std::visit(
            ft::overloaded{[](const Empty&) -> std::optional<InternalView> { return std::nullopt; },
                           [this](const Single& s) -> std::optional<InternalView> {
                               return InternalView{s.d_elem, FingerTree(d_alloc, d_monoid)};
                           },
                           [this](const DeepPtr& d) -> std::optional<InternalView> {
                               EP h = d->d_left.front();
                               if (d->d_left.size() > 1) {
                                   auto t = ft::digit_tail<T, MONOID>(d->d_left);
                                   return InternalView{
                                       h, make_deep(d_alloc, d_monoid, std::move(t), d->d_spine, d->d_right)};
                               }
                               return InternalView{h, deep_l(d_alloc, d_monoid, d->d_spine, d->d_right)};
                           }},
            d_repr);
    }

    auto view_r_internal() const -> std::optional<InternalView> {
        return std::visit(
            ft::overloaded{[](const Empty&) -> std::optional<InternalView> { return std::nullopt; },
                           [this](const Single& s) -> std::optional<InternalView> {
                               return InternalView{s.d_elem, FingerTree(d_alloc, d_monoid)};
                           },
                           [this](const DeepPtr& d) -> std::optional<InternalView> {
                               EP l = d->d_right.back();
                               if (d->d_right.size() > 1) {
                                   auto i = ft::digit_init<T, MONOID>(d->d_right);
                                   return InternalView{
                                       l, make_deep(d_alloc, d_monoid, d->d_left, d->d_spine, std::move(i))};
                               }
                               return InternalView{l, deep_r(d_alloc, d_monoid, d->d_left, d->d_spine)};
                           }},
            d_repr);
    }

    // -- Internal cons/snoc (works on ElemPtrs) ------------------------------

    auto cons_internal(EP x) const -> FingerTree {
        return std::visit(
            ft::overloaded{
                [this, &x](const Empty&) { return make_single(d_alloc, d_monoid, std::move(x)); },
                [&](const Single& s) {
                    Digit l;
                    l.push_back(std::move(x));
                    Digit r;
                    r.push_back(s.d_elem);
                    return make_deep(std::move(l), SpinePtr{}, std::move(r));
                },
                [&](const DeepPtr& d) -> FingerTree {
                    if (d->d_left.size() < 4) {
                        return make_deep(
                            ft::digit_with_pushed_front<T, MONOID>(d->d_left, std::move(x)), d->d_spine, d->d_right);
                    }
                    // Left digit is full: take last 3 as a Node3, push down
                    // spine.
                    auto node = ft::make_node3<T, MONOID>(d_alloc, d_monoid, d->d_left[1], d->d_left[2], d->d_left[3]);
                    SpinePtr sp;
                    if (d->d_spine)
                        sp = allocate_spine(d->d_spine->cons_internal(std::move(node)));
                    else
                        sp = allocate_spine(make_single(d_alloc, d_monoid, std::move(node)));
                    Digit new_left;
                    new_left.push_back(std::move(x));
                    new_left.push_back(d->d_left[0]);
                    return make_deep(std::move(new_left), std::move(sp), d->d_right);
                }},
            d_repr);
    }

    auto snoc_internal(EP x) const -> FingerTree {
        return std::visit(
            ft::overloaded{[this, &x](const Empty&) { return make_single(d_alloc, d_monoid, std::move(x)); },
                           [&](const Single& s) {
                               Digit l;
                               l.push_back(s.d_elem);
                               Digit r;
                               r.push_back(std::move(x));
                               return make_deep(std::move(l), SpinePtr{}, std::move(r));
                           },
                           [&](const DeepPtr& d) -> FingerTree {
                               if (d->d_right.size() < 4) {
                                   return make_deep(d->d_left,
                                                    d->d_spine,
                                                    ft::digit_with_pushed_back<T, MONOID>(d->d_right, std::move(x)));
                               }
                               // Right digit is full: take first 3 as a Node3, push down
                               // spine.
                               auto node = ft::make_node3<T, MONOID>(
                                   d_alloc, d_monoid, d->d_right[0], d->d_right[1], d->d_right[2]);
                               SpinePtr sp;
                               if (d->d_spine)
                                   sp = allocate_spine(d->d_spine->snoc_internal(std::move(node)));
                               else
                                   sp = allocate_spine(make_single(d_alloc, d_monoid, std::move(node)));
                               Digit new_right;
                               new_right.push_back(d->d_right[3]);
                               new_right.push_back(std::move(x));
                               return make_deep(d->d_left, std::move(sp), std::move(new_right));
                           }},
            d_repr);
    }

    // -- Internal split ------------------------------------------------------
    //
    // Predicate is templated end-to-end: no std::function allocation per
    // recursive call.  The predicate flows by const reference; the running
    // prefix is threaded as a runtime value, so a single Pred type satisfies
    // the entire descent.

    struct InternalSplit {
        FingerTree d_left;
        EP         d_pivot;
        FingerTree d_right;
    };

    struct DigitSplit {
        std::optional<Digit> d_left;
        EP                   d_pivot;
        std::optional<Digit> d_right;
    };

    template <typename PRED>
    auto split_digit(const PRED& pred, Tag prefix, const Digit& d) const -> std::optional<DigitSplit> {
        Tag running = prefix;
        for (std::size_t i = 0; i < d.size(); ++i) {
            running = d_monoid.combine(running, d[i]->d_measure);
            if (pred(running)) {
                std::optional<Digit> left_d;
                std::optional<Digit> right_d;
                if (i > 0) {
                    Digit ld;
                    for (std::size_t k = 0; k < i; ++k)
                        ld.push_back(d[k]);
                    left_d = std::move(ld);
                }
                if (i + 1 < d.size()) {
                    Digit rd;
                    for (std::size_t k = i + 1; k < d.size(); ++k)
                        rd.push_back(d[k]);
                    right_d = std::move(rd);
                }
                return DigitSplit{std::move(left_d), d[i], std::move(right_d)};
            }
        }
        return std::nullopt;
    }

    template <typename PRED>
    auto split_within_elem(const PRED& pred, Tag prefix, const EP& node_ep) const -> DigitSplit {
        auto children = ft::elem_to_digit<T, MONOID>(node_ep);
        auto ds       = split_digit(pred, prefix, children);
        assert(ds.has_value() && "split_within_elem: predicate never triggered");
        return std::move(*ds);
    }

    static auto
    assemble_l(const ALLOCATOR& alloc, const MONOID& monoid, std::optional<Digit> left_d, SpinePtr spine, Digit right)
        -> FingerTree {
        if (left_d.has_value())
            return make_deep(alloc, monoid, std::move(*left_d), std::move(spine), std::move(right));
        return deep_l(alloc, monoid, std::move(spine), std::move(right));
    }

    static auto
    assemble_r(const ALLOCATOR& alloc, const MONOID& monoid, Digit left, SpinePtr spine, std::optional<Digit> right_d)
        -> FingerTree {
        if (right_d.has_value())
            return make_deep(alloc, monoid, std::move(left), std::move(spine), std::move(*right_d));
        return deep_r(alloc, monoid, std::move(left), std::move(spine));
    }

    template <typename PRED>
    auto split_impl(const PRED& pred, Tag prefix) const -> std::optional<InternalSplit> {
        if (is_empty())
            return std::nullopt;

        if (auto* s = std::get_if<Single>(&d_repr)) {
            if (pred(d_monoid.combine(prefix, s->d_elem->d_measure)))
                return InternalSplit{FingerTree(d_alloc, d_monoid), s->d_elem, FingerTree(d_alloc, d_monoid)};
            return std::nullopt;
        }

        const auto& d = *std::get<DeepPtr>(d_repr);

        auto vl = d_monoid.combine(prefix, ft::digit_measure(d_monoid, d.d_left));
        if (pred(vl)) {
            auto ds = split_digit(pred, prefix, d.d_left);
            if (!ds.has_value())
                return std::nullopt;
            auto lt =
                ds->d_left.has_value() ? digit_to_tree(d_alloc, d_monoid, *ds->d_left) : FingerTree(d_alloc, d_monoid);
            auto rt = assemble_l(d_alloc, d_monoid, std::move(ds->d_right), d.d_spine, d.d_right);
            return InternalSplit{std::move(lt), std::move(ds->d_pivot), std::move(rt)};
        }

        auto spine_m = d.d_spine ? d.d_spine->measure() : d_monoid.identity();
        auto vm      = d_monoid.combine(vl, spine_m);
        if (pred(vm)) {
            if (!d.d_spine || d.d_spine->is_empty())
                return std::nullopt;

            auto ss = d.d_spine->split_impl(pred, vl);
            if (!ss.has_value())
                return std::nullopt;

            auto node_prefix = d_monoid.combine(vl, ss->d_left.measure());
            auto ns          = split_within_elem(pred, node_prefix, ss->d_pivot);

            SpinePtr sl;
            if (!ss->d_left.is_empty())
                sl = allocate_spine(std::move(ss->d_left));
            SpinePtr sr;
            if (!ss->d_right.is_empty())
                sr = allocate_spine(std::move(ss->d_right));

            auto lt = assemble_r(d_alloc, d_monoid, d.d_left, std::move(sl), std::move(ns.d_left));
            auto rt = assemble_l(d_alloc, d_monoid, std::move(ns.d_right), std::move(sr), d.d_right);
            return InternalSplit{std::move(lt), std::move(ns.d_pivot), std::move(rt)};
        }

        auto ds = split_digit(pred, vm, d.d_right);
        if (!ds.has_value())
            return std::nullopt;
        auto lt = assemble_r(d_alloc, d_monoid, d.d_left, d.d_spine, std::move(ds->d_left));
        auto rt =
            ds->d_right.has_value() ? digit_to_tree(d_alloc, d_monoid, *ds->d_right) : FingerTree(d_alloc, d_monoid);
        return InternalSplit{std::move(lt), std::move(ds->d_pivot), std::move(rt)};
    }

    // -- app3: Hinze-Paterson concatenation ----------------------------------

    static auto app3(const ALLOCATOR&           alloc,
                     const MONOID&              monoid,
                     const FingerTree&          left,
                     std::inplace_vector<EP, 6> middle,
                     const FingerTree&          right) -> FingerTree {
        if (left.is_empty()) {
            auto result = right;
            for (auto it = middle.rbegin(); it != middle.rend(); ++it)
                result = result.cons_internal(std::move(*it));
            return result;
        }
        if (right.is_empty()) {
            auto result = left;
            for (auto& elem : middle)
                result = result.snoc_internal(std::move(elem));
            return result;
        }
        if (auto* ls = std::get_if<Single>(&left.d_repr)) {
            auto result = right;
            for (auto it = middle.rbegin(); it != middle.rend(); ++it)
                result = result.cons_internal(std::move(*it));
            return result.cons_internal(ls->d_elem);
        }
        if (auto* rs = std::get_if<Single>(&right.d_repr)) {
            auto result = left;
            for (auto& elem : middle)
                result = result.snoc_internal(std::move(elem));
            return result.snoc_internal(rs->d_elem);
        }

        const auto& ld = *std::get<DeepPtr>(left.d_repr);
        const auto& rd = *std::get<DeepPtr>(right.d_repr);

        // combined ≤ 4 (right digit) + 4 (middle) + 4 (left digit) = 12
        std::inplace_vector<EP, 12> combined;
        for (const auto& ep : ld.d_right)
            combined.push_back(ep);
        for (auto& ep : middle)
            combined.push_back(std::move(ep));
        for (const auto& ep : rd.d_left)
            combined.push_back(ep);
        auto ns = ft::nodes_from<T, MONOID>(alloc, monoid, std::move(combined));

        auto     left_spine  = ld.d_spine ? *ld.d_spine : FingerTree(alloc, monoid);
        auto     right_spine = rd.d_spine ? *rd.d_spine : FingerTree(alloc, monoid);
        auto     new_spine   = FingerTree::app3(alloc, monoid, left_spine, std::move(ns), right_spine);
        SpinePtr sp;
        if (!new_spine.is_empty())
            sp = allocate_spine(alloc, std::move(new_spine));
        return make_deep(alloc, monoid, ld.d_left, std::move(sp), rd.d_right);
    }

    // -- Bottom-up construction from a flat ElemPtr sequence -----------------
    //
    // Builds a balanced finger tree from a pre-allocated sequence of ElemPtrs
    // without going through repeated snoc.  The snoc path allocates O(N) Deep
    // and SpinePtr objects; this path allocates only O(log N) of each.
    //
    // The N-element input is split: 2 elements go to the left digit, 2 to the
    // right digit, and the remaining N-4 elements are grouped into Node2/Node3
    // objects which are fed into a recursive call.  The recursion depth is
    // O(log_3 N), and total Elem allocations are 1.5N vs 2N for repeated snoc.
    //
    // Small cases (N ≤ 8) are placed directly into left/right digits with an
    // empty spine, avoiding the node-grouping pass entirely.

    static auto from_elems_impl(const ALLOCATOR& alloc, const MONOID& monoid, EpVec elems) -> FingerTree {
        const auto n = elems.size();
        if (n == 0)
            return FingerTree(alloc, monoid);
        if (n == 1)
            return make_single(alloc, monoid, std::move(elems[0]));

        if (n <= 8) {
            const auto lsz = std::min(n / 2, std::size_t{4});
            Digit      left_d, right_d;
            for (std::size_t i = 0; i < lsz; ++i)
                left_d.push_back(std::move(elems[i]));
            for (std::size_t i = lsz; i < n; ++i)
                right_d.push_back(std::move(elems[i]));
            return make_deep(alloc, monoid, std::move(left_d), SpinePtr{}, std::move(right_d));
        }

        // n >= 9: 2 left, 2 right, group middle (n-4) into nodes.
        Digit left_d, right_d;
        left_d.push_back(std::move(elems[0]));
        left_d.push_back(std::move(elems[1]));
        right_d.push_back(std::move(elems[n - 2]));
        right_d.push_back(std::move(elems[n - 1]));

        EpVec nodes{EpAlloc(alloc)};
        nodes.reserve((n - 4 + 2) / 3);
        std::size_t i   = 2;
        const auto  end = n - 2;
        while (end - i > 4) {
            nodes.push_back(ft::make_node3<T, MONOID>(
                alloc, monoid, std::move(elems[i]), std::move(elems[i + 1]), std::move(elems[i + 2])));
            i += 3;
        }
        switch (end - i) {
        case 2:
            nodes.push_back(ft::make_node2<T, MONOID>(alloc, monoid, std::move(elems[i]), std::move(elems[i + 1])));
            break;
        case 3:
            nodes.push_back(ft::make_node3<T, MONOID>(
                alloc, monoid, std::move(elems[i]), std::move(elems[i + 1]), std::move(elems[i + 2])));
            break;
        case 4:
            nodes.push_back(ft::make_node2<T, MONOID>(alloc, monoid, std::move(elems[i]), std::move(elems[i + 1])));
            nodes.push_back(
                ft::make_node2<T, MONOID>(alloc, monoid, std::move(elems[i + 2]), std::move(elems[i + 3])));
            break;
        default:
            std::unreachable();
        }
        auto     spine = from_elems_impl(alloc, monoid, std::move(nodes));
        SpinePtr sp    = spine.is_empty() ? SpinePtr{} : allocate_spine(alloc, std::move(spine));
        return make_deep(alloc, monoid, std::move(left_d), std::move(sp), std::move(right_d));
    }

    // -- Internal flatten / for_each -----------------------------------------

    void flatten_elems(std::vector<T>& out) const {
        std::visit(ft::overloaded{[](const Empty&) {},
                                  [&](const Single& s) { ft::flatten_elem(s.d_elem, out); },
                                  [&](const DeepPtr& d) {
                                      for (const auto& ep : d->d_left)
                                          ft::flatten_elem(ep, out);
                                      if (d->d_spine)
                                          d->d_spine->flatten_elems(out);
                                      for (const auto& ep : d->d_right)
                                          ft::flatten_elem(ep, out);
                                  }},
                   d_repr);
    }

    template <typename F>
    void for_each_internal(const F& fn) const {
        std::visit(ft::overloaded{[](const Empty&) {},
                                  [&](const Single& s) { ft::for_each_elem(s.d_elem, fn); },
                                  [&](const DeepPtr& d) {
                                      for (const auto& ep : d->d_left)
                                          ft::for_each_elem(ep, fn);
                                      if (d->d_spine)
                                          d->d_spine->for_each_internal(fn);
                                      for (const auto& ep : d->d_right)
                                          ft::for_each_elem(ep, fn);
                                  }},
                   d_repr);
    }

    // ========================================================================
    //                          PUBLIC INTERFACE
    // ========================================================================
  public:
    // -----------------------------------------------------------------------
    // Container / ReversibleContainer named requirements
    // -----------------------------------------------------------------------

    using value_type             = T;
    using tag_type               = Tag;      // non-standard: the measure value type
    using monoid_type            = MONOID;   // non-standard: the monoid instance type
    using reference              = const T&; // immutable: all access is const
    using const_reference        = const T&;
    using allocator_type         = ALLOCATOR;
    using iterator               = FingerTreeIterator<T, MONOID, MEASURE_POLICY, ALLOCATOR>;
    using const_iterator         = iterator; // immutable: no mutable iteration
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using difference_type        = std::ptrdiff_t;
    using size_type              = std::size_t;

    // -- Construction --------------------------------------------------------

    // Default constructor: variant value-initialises to its first alternative
    // (Empty), so FingerTree{} is a valid empty tree — no factory call needed.
    FingerTree() = default;
    explicit FingerTree(const ALLOCATOR& alloc, const MONOID& monoid = MONOID{}) : d_alloc(alloc), d_monoid(monoid) {}

    FingerTree(const FingerTree&) = default;
    FingerTree(FingerTree&&)      = default;

    // Allocator-extended move constructor required by the std::uses_allocator
    // protocol (trailing form: T(T&&, Alloc)).  Taking alloc by value allows
    // implicit conversion from rebind_alloc<FingerTree> → ALLOCATOR, which
    // is how std::allocate_shared<const FingerTree> invokes this.
    //
    // Coherency rule: if the incoming tree was allocated from the same
    // resource, its nodes are shared (O(1)).  If from a different resource,
    // every node is rebuilt using alloc (O(N)) — no dangling cross-resource
    // pointers are ever created silently.
    FingerTree(FingerTree other, ALLOCATOR alloc) : d_alloc(std::move(alloc)), d_monoid(other.d_monoid) {
        if (alloc_equal(d_alloc, other.d_alloc))
            d_repr = std::move(other.d_repr);
        else
            *this = from_sequence(other.flatten(), d_alloc, d_monoid);
    }

    // Copy assignment.
    // Propagating allocators: alloc and data both taken from other.
    // Non-propagating, same resource: share data (O(1), structural sharing).
    // Non-propagating, different resource: rebuild data using this allocator
    //   (O(N)) — allocator coherency is maintained; no cross-resource sharing.
    auto operator=(const FingerTree& other) & -> FingerTree& {
        if constexpr (std::allocator_traits<ALLOCATOR>::propagate_on_container_copy_assignment::value) {
            d_alloc = other.d_alloc;
            d_repr  = other.d_repr;
        } else if (alloc_equal(d_alloc, other.d_alloc)) {
            d_repr = other.d_repr;
        } else {
            *this = from_sequence(other.flatten(), d_alloc, d_monoid);
        }
        return *this;
    }

    // Move assignment.
    // Non-propagating with different resource: falls back to element-wise
    // rebuild (cannot move cross-resource and stay coherent).  Not noexcept
    // in that path; noexcept holds for same-resource and always-equal cases.
    auto operator=(FingerTree&& other) & -> FingerTree& {
        if constexpr (std::allocator_traits<ALLOCATOR>::propagate_on_container_move_assignment::value) {
            d_alloc = std::move(other.d_alloc);
            d_repr  = std::move(other.d_repr);
        } else if (alloc_equal(d_alloc, other.d_alloc)) {
            d_repr = std::move(other.d_repr);
        } else {
            *this = from_sequence(other.flatten(), d_alloc, d_monoid);
        }
        return *this;
    }

    [[nodiscard]] auto get_allocator() const noexcept -> allocator_type { return d_alloc; }

    // The monoid instance this tree's cached measures were computed under.
    [[nodiscard]] auto get_monoid() const noexcept -> monoid_type { return d_monoid; }

    // True for the default count measure, where measure() is the element
    // count and size(), operator== and flatten() take O(1) shortcuts.
    static constexpr bool has_unit_count_measure =
        std::same_as<MONOID, beman::fingertree::typeclass::sum_monoid<std::size_t>> &&
        std::same_as<Meas, UnitMeasure<T, std::size_t>>;

    [[nodiscard]] static auto leaf(T value) -> FingerTree {
        ALLOCATOR alloc{};
        MONOID    monoid{};
        return make_single(alloc, monoid, ft::make_leaf<T, MONOID>(alloc, meas_fn(), std::move(value)));
    }

    // -- Container query members ---------------------------------------------

    [[nodiscard]] auto empty() const noexcept -> bool { return std::holds_alternative<Empty>(d_repr); }

    // is_empty() is an alias kept for backward compatibility.
    [[nodiscard]] auto is_empty() const noexcept -> bool { return empty(); }

    // size() is O(1) for the default unit measure; O(N) for custom measures.
    [[nodiscard]] auto size() const -> size_type {
        if constexpr (has_unit_count_measure) {
            return measure();
        } else {
            size_type n = 0;
            for_each([&](const T&) { ++n; });
            return n;
        }
    }

    [[nodiscard]] auto max_size() const noexcept -> size_type { return std::numeric_limits<size_type>::max(); }

    void swap(FingerTree& other) noexcept {
        std::swap(d_repr, other.d_repr);
        std::swap(d_monoid, other.d_monoid);
    }

    friend void swap(FingerTree& a, FingerTree& b) noexcept { a.swap(b); }

    // element-wise equality; short-circuits on size for unit-measure trees
    // Hidden friend: element-wise equality via flatten() avoids requiring the
    // iterator to be complete at class-body parse time.
    friend auto operator==(const FingerTree& lhs, const FingerTree& rhs) -> bool {
        if constexpr (has_unit_count_measure) {
            if (lhs.measure() != rhs.measure())
                return false;
        }
        return lhs.flatten() == rhs.flatten();
    }

    // front / back (SequenceContainer-style convenience; no mutation)
    [[nodiscard]] auto front() const -> const_reference { return head_ref(); }
    [[nodiscard]] auto back() const -> const_reference { return last_ref(); }

    [[nodiscard]] auto is_leaf() const -> bool { return std::holds_alternative<Single>(d_repr); }

    [[nodiscard]] auto is_branch() const -> bool { return std::holds_alternative<DeepPtr>(d_repr); }

    [[nodiscard]] auto measure() const -> Tag {
        return std::visit(ft::overloaded{[this](const Empty&) { return d_monoid.identity(); },
                                         [](const Single& s) -> Tag { return s.d_elem->d_measure; },
                                         [](const DeepPtr& d) -> Tag { return d->d_measure; }},
                          d_repr);
    }

    [[nodiscard]] auto value() const -> const T& {
        assert(is_leaf());
        auto& s = std::get<Single>(d_repr);
        assert(ft::is_leaf(s.d_elem));
        return ft::leaf_value(s.d_elem);
    }

    // -- spine_depth: O(log N) -----------------------------------------------
    //
    // Returns the number of nested spine levels (0 for Empty/Single, 1 for a
    // Deep with no spine, 1 + spine->spine_depth() otherwise).
    //
    // Structural invariant: spine_depth() <= floor(log2(N)) for any N-element
    // tree.  Proof sketch: an element at spine level d is a Node covering >=
    // 2^d leaves, so a non-empty spine at level d requires N >= 2^d.
    //
    // Consequence: every recursive call chain in this class — flatten_elems,
    // for_each_internal, view_l/r spine-borrowing, and the shared_ptr
    // destructor cascade — is bounded by O(log N) frames and cannot
    // stack-overflow for any tree that can physically exist.  This bound is
    // tested by "FingerTree - SpineDepthIsLogarithmic" and exercised at
    // N=10'000 by "FingerTree - LargeTreeRecursionNoStackOverflow".

    [[nodiscard]] auto spine_depth() const -> std::size_t {
        return std::visit(ft::overloaded{[](const Empty&) -> std::size_t { return 0; },
                                         [](const Single&) -> std::size_t { return 0; },
                                         [](const DeepPtr& d) -> std::size_t {
                                             if (!d->d_spine || d->d_spine->is_empty())
                                                 return 1;
                                             return 1 + d->d_spine->spine_depth();
                                         }},
                          d_repr);
    }

    // -- begin / end: O(1) or O(N) depending on measure ----------------------
    //
    // Returns a bidirectional iterator to the first / past-the-end element.
    // Defined out-of-line below (after finger_tree_iterator.hpp is included).
    // For the default measure <T, std::size_t, UnitMeasure> the size is
    // computed in O(1) via measure(); for custom measures it is O(N).

    auto begin() const -> iterator;
    auto end() const -> iterator;

    // Definitions are out-of-line in finger_tree_iterator.hpp (iterator must
    // be a complete type for std::reverse_iterator instantiation).
    auto cbegin() const -> const_iterator;
    auto cend() const -> const_iterator;
    auto rbegin() const -> reverse_iterator;
    auto rend() const -> reverse_iterator;
    auto crbegin() const -> const_reverse_iterator;
    auto crend() const -> const_reverse_iterator;

    // -- cons / snoc: O(1) amortized -----------------------------------------

    [[nodiscard]] auto cons(T x) const -> FingerTree { return cons_internal(wrap_leaf(std::move(x))); }

    [[nodiscard]] auto snoc(T x) const -> FingerTree { return snoc_internal(wrap_leaf(std::move(x))); }

    // -- view_l / view_r: O(1) amortized -------------------------------------

    struct View {
        T          d_value;
        FingerTree d_rest;
    };

    [[nodiscard]] auto view_l() const -> std::optional<View> {
        auto iv = view_l_internal();
        if (!iv.has_value())
            return std::nullopt;
        assert(ft::is_leaf(iv->d_elem));
        return View{ft::leaf_value(iv->d_elem), std::move(iv->d_rest)};
    }

    [[nodiscard]] auto view_r() const -> std::optional<View> {
        auto iv = view_r_internal();
        if (!iv.has_value())
            return std::nullopt;
        assert(ft::is_leaf(iv->d_elem));
        return View{ft::leaf_value(iv->d_elem), std::move(iv->d_rest)};
    }

    [[nodiscard]] auto head() const -> T {
        auto v = view_l();
        assert(v.has_value());
        return std::move(v->d_value);
    }

    // head_ref / last_ref: O(1) amortized, return const T& into the leaf.
    // Safe while *this is alive (the leaf is structurally shared with the
    // tree).  Avoids copying T for read-only access of large element types.
    [[nodiscard]] auto head_ref() const -> const T& {
        auto iv = view_l_internal();
        assert(iv.has_value() && ft::is_leaf(iv->d_elem));
        return ft::leaf_value(iv->d_elem);
    }

    [[nodiscard]] auto tail() const -> FingerTree {
        auto v = view_l();
        return v.has_value() ? std::move(v->d_rest) : FingerTree(d_alloc, d_monoid);
    }

    [[nodiscard]] auto last() const -> T {
        auto v = view_r();
        assert(v.has_value());
        return std::move(v->d_value);
    }

    [[nodiscard]] auto last_ref() const -> const T& {
        auto iv = view_r_internal();
        assert(iv.has_value() && ft::is_leaf(iv->d_elem));
        return ft::leaf_value(iv->d_elem);
    }

    [[nodiscard]] auto init() const -> FingerTree {
        auto v = view_r();
        return v.has_value() ? std::move(v->d_rest) : FingerTree(d_alloc, d_monoid);
    }

    // -- flatten / for_each: O(n) --------------------------------------------

    [[nodiscard]] auto flatten() const -> std::vector<T> {
        std::vector<T> out;
        // Reserve when measure() equals element count (unit measure case).
        // For custom measures (rope byte count, interval max-end, etc.) this
        // condition is false and we fall back to push_back reallocation.
        if constexpr (has_unit_count_measure) {
            out.reserve(measure());
        }
        flatten_elems(out);
        return out;
    }

    template <typename F>
    void for_each(F&& fn) const {
        for_each_internal(fn);
    }

    // Build from a vector. All node and scaffolding allocations use alloc;
    // every cached measure is computed under monoid, which the resulting
    // tree stores. Takes values by const ref so the input vector is not
    // copied; each element is individually copied into its Leaf node via
    // make_leaf.
    [[nodiscard]] static auto from_sequence(const std::vector<T>& values,
                                            const ALLOCATOR&      alloc  = ALLOCATOR{},
                                            const MONOID&         monoid = MONOID{}) -> FingerTree {
        if (values.empty())
            return FingerTree(alloc, monoid);
        auto  mf = meas_fn();
        EpVec elems{EpAlloc(alloc)};
        elems.reserve(values.size());
        for (const auto& v : values)
            elems.push_back(ft::make_leaf<T, MONOID>(alloc, mf, v));
        return from_elems_impl(alloc, monoid, std::move(elems));
    }

    // -- append / concat: O(log min(n, m)) -----------------------------------

    // append: O(log min(n,m)) when allocators match; O(|right|) rebuild when
    // they differ.  The rebuild guarantees every node in the result comes from
    // d_alloc — no silent cross-resource sharing.  To opt into the O(log N)
    // fast path with a known-safe mixed tree, call app3 directly.
    [[nodiscard]] auto append(const FingerTree& right) const -> FingerTree {
        if (!alloc_equal(d_alloc, right.d_alloc)) {
            auto rebuilt = from_sequence(right.flatten(), d_alloc);
            return app3(d_alloc, d_monoid, *this, {}, rebuilt);
        }
        return app3(d_alloc, d_monoid, *this, {}, right);
    }

    [[nodiscard]] static auto concat(const FingerTree& left, const FingerTree& right) -> FingerTree {
        return left.append(right);
    }

    // -- split: O(log n) -----------------------------------------------------

    struct Split {
        FingerTree d_left;
        T          d_pivot;
        FingerTree d_right;
    };

    struct SplitAt {
        FingerTree d_left;
        FingerTree d_right;
    };

    template <typename PRED>
    [[nodiscard]] auto search(PRED&& pred) const -> std::optional<T> {
        auto sp = split(std::forward<PRED>(pred));
        if (!sp.has_value())
            return std::nullopt;
        return std::move(sp->d_pivot);
    }

    template <typename PRED>
    [[nodiscard]] auto split(PRED&& pred) const -> std::optional<Split> {
        auto is = split_impl(pred, d_monoid.identity());
        if (!is.has_value())
            return std::nullopt;
        assert(ft::is_leaf(is->d_pivot));
        return Split{std::move(is->d_left), ft::leaf_value(is->d_pivot), std::move(is->d_right)};
    }

    template <typename PRED>
    [[nodiscard]] auto split_at(PRED&& pred) const -> SplitAt {
        auto sp = split(std::forward<PRED>(pred));
        if (!sp.has_value())
            return SplitAt{*this, FingerTree(d_alloc, d_monoid)};
        return SplitAt{std::move(sp->d_left), sp->d_right.cons(std::move(sp->d_pivot))};
    }

    [[nodiscard]] auto split_at_measure(const Tag& threshold) const -> SplitAt
        requires requires(const Tag& a, const Tag& b) {
            { a >= b } -> std::convertible_to<bool>;
        }
    {
        return split_at([&threshold](const Tag& prefix) { return prefix >= threshold; });
    }

    // -- reversed: O(N) eager rebuild ----------------------------------------
    //
    // Returns a new tree containing the same elements in reverse order.
    // The result is keyed on dual_monoid<MONOID>: the same tag type, the same
    // measure policy, combine with its arguments flipped -- so prefix
    // measures on the reversed tree grow from the new left edge (old right
    // edge), and split_at_measure takes the same plain threshold it takes on
    // the original.
    //
    // Implementation is Option A from the design doc: flatten, reverse,
    // rebuild. Cost: O(N) time and O(N) space.  A lazy structural reverse is
    // possible only for commutative monoids (cached measures would otherwise be
    // wrong); the eager rebuild is always correct.

    [[nodiscard]] auto reversed() const
        -> FingerTree<T, beman::fingertree::typeclass::dual_monoid<MONOID>, Meas, ALLOCATOR> {
        using DT = FingerTree<T, beman::fingertree::typeclass::dual_monoid<MONOID>, Meas, ALLOCATOR>;
        auto v   = flatten();
        std::reverse(v.begin(), v.end());
        return DT::from_sequence(std::move(v), d_alloc, beman::fingertree::typeclass::dual_monoid<MONOID>{d_monoid});
    }
};

} // namespace beman::fingertree::tree

// Iterator support: include after the class so the iterator can reference the
// complete FingerTree type.  This brings FingerTreeIterator into scope and
// defines the free begin/end overloads AND the out-of-line definitions of
// FingerTree::begin() / FingerTree::end() declared in the class body above.
#include <beman/fingertree/tree/finger_tree_iterator.hpp>

#endif
