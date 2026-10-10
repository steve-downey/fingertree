// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TYPECLASS_MONOID_HPP
#define BEMAN_FINGERTREE_TYPECLASS_MONOID_HPP

#include <beman/fingertree/typeclass/typeclass_base.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace beman::fingertree::typeclass {

// This header is a copy of beman.transpose's monoid.hpp with the namespace
// renamed; keep the two in step rather than editing one.
//
// Monoid pattern invariants:
// - A monoid is an *instance object*: anything with `identity()` and
//   `combine(lhs, rhs)` over one value type. `monoid_object<M, T>` is the
//   concept.
// - Monoid<T> is the registry: the one canonical instance for a type, where
//   one exists (string and vector concatenation). monoid_v<T> is the lookup
//   object generic algorithms default to when the caller passes nothing.
// - Where a type carries more than one monoid -- numbers, booleans -- no
//   instance is registered; the caller passes one (`sum_monoid<int>{}`,
//   `max_monoid<int>{}`, `any_monoid{}`, ...). The value type is never
//   wrapped to select a monoid. See beman.transpose docs/decisions.md#monoid-selection.
// - Instances may carry state. The library may copy them and pass them by
//   value; the expectation (not a requirement) is that they are shaped like
//   stateless objects whose calls resolve statically.
// - identity and combine must stay coherent for a single associative law
//   domain.

/** Customization point for the Monoid registry.
 * Specialize this struct for type `VALUE_TYPE` and provide `identity()` and
 * `combine(lhs, rhs)` to make the canonical monoid of that type available
 * wherever a fold is called without an explicit instance.
 */
template <class VALUE_TYPE>
struct Monoid;

/** Canonical lookup object for Monoid<VALUE_TYPE>; used by generic algorithms.
 */
template <class VALUE_TYPE>
inline constexpr Monoid<VALUE_TYPE> monoid_v = Monoid<VALUE_TYPE>{};

/** The value type a monoid instance `M` operates on: what its `identity()`
 * returns.
 */
template <class M>
using monoid_value_t = std::remove_cvref_t<decltype(std::declval<const M&>().identity())>;

/** Satisfied when `M` is a monoid instance over `T`: `identity()` yields a
 * `T` and `combine` takes two `T`s to a `T`.
 */
template <class M, class T>
concept monoid_object = requires(const M& m, const T& a, const T& b) {
    { m.identity() } -> std::convertible_to<T>;
    { m.combine(a, b) } -> std::convertible_to<T>;
};

namespace detail {

/** Satisfied when `VALUE_TYPE` has a registered `Monoid`, i.e. a default
 * instance exists for it. Naming a `Monoid<VALUE_TYPE>` specialization is
 * always well-formed even when none exists -- the primary template is
 * declared, only undefined -- so the probe has to attempt to *construct*
 * one, which needs the type complete. This is the test a fold uses to decide
 * whether its two-argument form (no instance passed) is a candidate at all.
 */
template <class VALUE_TYPE>
concept has_registered_monoid = requires { Monoid<VALUE_TYPE>{}; };

} // namespace detail

// ---------------------------------------------------------------------------
// Instances over bare types. None of these is registered: each spells one
// of several monoids a type carries, and the caller names it at the fold.
// ---------------------------------------------------------------------------

/** Additive monoid on `T`: identity is the zero of `T`, combine adds. */
template <class T>
struct sum_monoid {
    using value_type = T;

    constexpr auto identity() const -> T { return T{}; }

    constexpr auto combine(const T& lhs, const T& rhs) const -> T { return lhs + rhs; }
};

/** Multiplicative monoid on `T`: identity is `T{1}`, combine multiplies. */
template <class T>
struct product_monoid {
    using value_type = T;

    constexpr auto identity() const -> T { return T{1}; }

    constexpr auto combine(const T& lhs, const T& rhs) const -> T { return lhs * rhs; }
};

/** Maximum monoid on `T`: combine takes the larger of the two.
 * The identity is the saturating lower bound of `T` -- negative infinity
 * where `T` has one, `std::numeric_limits<T>::lowest()` otherwise -- not an
 * adjoined identity element and not a `std::optional`. A `max_monoid<T>` on
 * a type whose `numeric_limits` is not specialized has no identity and
 * therefore is not a monoid; that is the correct outcome, not a gap.
 */
template <class T>
struct max_monoid {
    using value_type = T;

    constexpr auto identity() const -> T {
        if constexpr (std::numeric_limits<T>::has_infinity) {
            return -std::numeric_limits<T>::infinity();
        } else {
            return std::numeric_limits<T>::lowest();
        }
    }

    constexpr auto combine(const T& lhs, const T& rhs) const -> T { return lhs < rhs ? rhs : lhs; }
};

/** Minimum monoid on `T`: combine takes the smaller of the two.
 * The identity is the saturating upper bound of `T` -- positive infinity
 * where `T` has one, `std::numeric_limits<T>::max()` otherwise -- for the
 * same reasons `max_monoid` gives.
 */
template <class T>
struct min_monoid {
    using value_type = T;

    constexpr auto identity() const -> T {
        if constexpr (std::numeric_limits<T>::has_infinity) {
            return std::numeric_limits<T>::infinity();
        } else {
            return std::numeric_limits<T>::max();
        }
    }

    constexpr auto combine(const T& lhs, const T& rhs) const -> T { return rhs < lhs ? rhs : lhs; }
};

/** Disjunctive monoid on `bool`: identity is `false`, combine is logical or.
 */
struct any_monoid {
    using value_type = bool;

    constexpr auto identity() const -> bool { return false; }

    constexpr auto combine(bool lhs, bool rhs) const -> bool { return lhs || rhs; }
};

/** Conjunctive monoid on `bool`: identity is `true`, combine is logical and.
 */
struct all_monoid {
    using value_type = bool;

    constexpr auto identity() const -> bool { return true; }

    constexpr auto combine(bool lhs, bool rhs) const -> bool { return lhs && rhs; }
};

/** First-engaged monoid on `std::optional<T>`: identity is `nullopt`,
 * combine keeps the left operand when it is engaged and the right otherwise.
 * This is what `find_first` folds with.
 */
template <class T>
struct first_monoid {
    using value_type = std::optional<T>;

    constexpr auto identity() const -> std::optional<T> { return {}; }

    constexpr auto combine(const std::optional<T>& lhs, const std::optional<T>& rhs) const -> std::optional<T> {
        return lhs.has_value() ? lhs : rhs;
    }
};

// ---------------------------------------------------------------------------
// Combinators: an instance built from instances. Operands are held by value
// as their concrete types, so composing instances erases nothing the
// compiler could otherwise see.
// ---------------------------------------------------------------------------

/** The dual of `M`: same value type, same identity, combine with its
 * arguments flipped. If (T, ·, e) is a monoid then (T, ·ᵒᵖ, e) with
 * a ·ᵒᵖ b = b · a is also a monoid.
 */
template <class M>
struct dual_monoid {
    using value_type = monoid_value_t<M>;

    M d_inner{};

    constexpr auto identity() const -> value_type { return d_inner.identity(); }

    constexpr auto combine(const value_type& lhs, const value_type& rhs) const -> value_type {
        return d_inner.combine(rhs, lhs);
    }
};

/** The product of monoids `M...` over `std::tuple<monoid_value_t<M>...>`:
 * identity and combine are elementwise.
 */
template <class... M>
struct tuple_monoid {
    using value_type = std::tuple<monoid_value_t<M>...>;

    std::tuple<M...> d_inners{};

    constexpr auto identity() const -> value_type {
        return std::apply([](const M&... inner) { return value_type{inner.identity()...}; }, d_inners);
    }

    constexpr auto combine(const value_type& lhs, const value_type& rhs) const -> value_type {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return value_type{std::get<I>(d_inners).combine(std::get<I>(lhs), std::get<I>(rhs))...};
        }(std::index_sequence_for<M...>{});
    }
};

// ---------------------------------------------------------------------------
// Registered instances: the one canonical monoid of a type.
// ---------------------------------------------------------------------------

/** Monoid<std::string>: concatenation monoid with identity "". */
template <>
struct Monoid<std::string> {
    using value_type = std::string;

    auto identity() const -> std::string { return {}; }

    auto combine(const std::string& lhs, const std::string& rhs) const -> std::string { return lhs + rhs; }
};

/** Monoid<std::vector<T>>: concatenation monoid with identity empty vector. */
template <class VALUE_TYPE>
struct Monoid<std::vector<VALUE_TYPE>> {
    using value_type = std::vector<VALUE_TYPE>;

    auto identity() const -> std::vector<VALUE_TYPE> { return {}; }

    auto combine(std::vector<VALUE_TYPE> lhs, const std::vector<VALUE_TYPE>& rhs) const -> std::vector<VALUE_TYPE> {
        lhs.insert(lhs.end(), rhs.begin(), rhs.end());
        return lhs;
    }
};

} // namespace beman::fingertree::typeclass

namespace beman::fingertree {

/** Returns the identity element of the registered Monoid of VALUE_TYPE. */
template <class VALUE_TYPE>
auto monoid_identity() -> VALUE_TYPE {
    return typeclass::monoid_v<VALUE_TYPE>.identity();
}

/** Combines two values using the registered Monoid of VALUE_TYPE. */
template <class VALUE_TYPE>
auto monoid_combine(const VALUE_TYPE& lhs, const VALUE_TYPE& rhs) -> VALUE_TYPE {
    return typeclass::monoid_v<VALUE_TYPE>.combine(lhs, rhs);
}

} // namespace beman::fingertree

#endif // BEMAN_FINGERTREE_TYPECLASS_MONOID_HPP
