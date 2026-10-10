// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_FINGERTREE_TYPECLASS_TYPECLASS_BASE_HPP
#define BEMAN_FINGERTREE_TYPECLASS_TYPECLASS_BASE_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace beman::fingertree {

// This header is a copy of beman.transpose's detail/typeclass_base.hpp with
// the namespace renamed; keep the two in step rather than editing one.
//
// Design invariants for the typeclass object pattern:
// - Per-concept lookup objects (for example *_typeclass<T>) are the
//   customization lookup points for typeclass dispatch.
// - Generic algorithms call through looked-up typeclass objects.
// - New concepts should keep lookup static and explicit.
// - Avoid adding parallel ADL-only customization paths for the same concept.

/** Always false, but dependent on a template parameter pack.
 *
 * A `static_assert` written directly with `false` fires as soon as the
 * enclosing template is parsed, even if the member it guards is never
 * called. Making the condition depend on the pack defers the check to
 * instantiation, so it fires only when someone actually calls the guarded
 * operation -- which is what lets a member give a custom "this operation
 * does not exist, here is why" diagnostic instead of a bare "no member"
 * error.
 */
template <class...>
inline constexpr bool always_false_v = false;

/** Local spelling of `std::remove_cvref_t`, kept for the typeclass headers
 * and tree adapters written against it.
 */
template <class T>
using remove_cvref_t = std::remove_cvref_t<T>;

/** The `Impl` sub-object reference a typeclass base's member should address,
 * given the deduced type of its `self` parameter.
 *
 * Every derived operation on a typeclass base takes the same shape: probe
 * `Impl` for a native version of the operation, forward to it if it is
 * there, derive otherwise. Both halves address `Impl` rather than `self`,
 * which is what keeps two mutually-derivable operations from recursing into
 * each other. This spells the const propagation that addressing needs, once.
 *
 * The cast itself cannot live here. Each base inherits its `Impl`
 * protectedly, so only a member of that base may perform the conversion;
 * each base carries a three-line private `impl_of` that does, and they all
 * name this alias. See `beman.transpose docs/decisions.md#impl-access-through-bases`.
 */
template <class IMPL, class SELF>
using impl_ref_t = std::conditional_t<std::is_const_v<std::remove_reference_t<SELF>>, const IMPL, IMPL>&;

/** Trait that extracts the element type from an applicative container.
 * Primary template uses the nested `value_type` alias when present.
 */
template <class T, class = void>
struct applicative_value;

template <class T>
struct applicative_value<T, std::void_t<typename std::remove_cvref_t<T>::value_type>> {
    using type = typename std::remove_cvref_t<T>::value_type;
};

template <class T>
struct applicative_value<std::optional<T>, void> {
    using type = T;
};

/** Convenience alias for `applicative_value<T>::type`. */
//! \expos
template <class T>
using applicative_value_t = typename applicative_value<std::remove_cvref_t<T>>::type;

namespace detail {

/** Whether `T`, ignoring cv-qualification and reference, is a
 * `std::optional` specialization. Lets an `invoke` deduce its operands
 * through forwarding references -- which is what carries the caller's value
 * category into the operation -- while still declining operands that are not
 * of the context it composes. */
template <class T>
struct is_optional : std::false_type {};

template <class U>
struct is_optional<std::optional<U>> : std::true_type {};

//! \expos
template <class T>
inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

/** The type `forward_contained` yields for an operand of type `OPERAND`,
 * for naming in a trailing return type. Spelled before the function so that
 * the function can name it and keep a trailing return type, rather than
 * deducing its return type from a body that a probe would have to
 * instantiate.
 *
 * An rvalue operand's value is handed over; an lvalue operand's value is a
 * `const` lvalue whether or not the operand itself is `const`. Simply
 * forwarding the operand would conflate two different things: carrying the
 * caller's value category, which is the point, and stripping `const` from a
 * non-`const` lvalue, which is not. The latter hands `function` a mutable
 * reference into the caller's operand -- enough to select a `T&` overload
 * over a `const T&` one, and to write through it -- where the composition is
 * specified only to invoke `function` on the value the operand holds.
 */
//! \expos
template <class OPERAND>
using contained_ref_t = std::conditional_t<std::is_lvalue_reference_v<OPERAND>,
                                           const std::remove_reference_t<decltype(*std::declval<OPERAND&>())>&,
                                           decltype(*std::declval<OPERAND>())>;

/** The value an applicative operand holds, with the operand's own value
 * category.
 *
 * An operation that takes its operands by `const&` can only ever copy the
 * values out of them, and a traversal that accumulates through such an
 * operation copies its whole accumulated prefix at every step. Deducing the
 * operand and forwarding through this is what lets a caller who has an
 * rvalue -- the traversal loop, moving its accumulator forward -- hand the
 * held value over instead of duplicating it. Only an rvalue: see
 * `contained_ref_t`.
 */
template <class OPERAND>
constexpr auto forward_contained(OPERAND&& operand) -> contained_ref_t<OPERAND> {
    return *std::forward<OPERAND>(operand);
}

/** `container[index]`, with `container`'s own value category.
 *
 * The lanewise applicative objects read each position of each operand
 * exactly once, so an rvalue operand's element may be handed over rather
 * than copied. As with `forward_contained`, this is what keeps a traversal
 * that accumulates through such an object from duplicating its whole
 * accumulated result at every lane of every step -- and, as there, an
 * lvalue operand's element is a `const` lvalue whether or not the operand
 * itself is `const`. `container[index]` on a non-`const` lvalue operand
 * would be a mutable reference into the caller's container.
 */
template <class CONTAINER>
constexpr auto forward_at(CONTAINER&& container, std::size_t index) -> decltype(auto) {
    if constexpr (std::is_lvalue_reference_v<CONTAINER>) {
        return std::as_const(container)[index];
    } else {
        return std::move(container[index]);
    }
}

/** The type `forward_at` yields for a container of type `CONTAINER`. */
//! \expos
template <class CONTAINER>
using element_ref_t = decltype(forward_at(std::declval<CONTAINER>(), std::size_t{}));

/** Builds a `std::array<U, N>` by calling `generator` with each index in turn.
 *
 * Default-constructing the array and then assigning its elements is the
 * obvious way to fill a fixed-extent result, and it silently requires the
 * element type to be default-constructible AND assignable -- neither of which
 * the operations that build such results actually need. Constructing the
 * array directly from the generated elements requires only what the operation
 * documents: that each element is constructible from its own result.
 *
 * The initializer-clauses of a braced-init-list are sequenced left to right
 * ([dcl.init.list]/4), so lane 0's element is produced before lane 1's.
 */
template <class U, std::size_t N, class GENERATOR, std::size_t... INDICES>
constexpr auto make_array(GENERATOR&& generator, std::index_sequence<INDICES...>) -> std::array<U, N> {
    return std::array<U, N>{generator(INDICES)...};
}

/** Representative witness callable for probing a one-argument derived
 * operation that is templated over an arbitrary callable -- `fmap`, `map`,
 * `fold_map`, `traverse`, and similar. A concept can check membership for
 * only one concrete instantiation, never for every possible callable; this
 * is that one instantiation, parameterized by what the probed operation
 * needs the callable to produce. Its presence in a concept is a witness
 * that the operation exists, not a proof that it exists for every callable.
 *
 * The body establishes the result TYPE and nothing else. Returning `RESULT{}`
 * would be simpler, and would impose a default-construction requirement that
 * no operation's specification asks for: a probe appears inside a
 * requires-expression, but probing a derived operation with a deduced return
 * type instantiates enough of that operation's body to instantiate this call,
 * so `RESULT{}` reaches the compiler and rejects contexts over
 * non-default-constructible elements. The body is never executed -- a concept
 * is checked, not evaluated -- so `std::unreachable` is the honest spelling.
 */
//! \expos
template <class RESULT>
struct probe_witness {
    template <class ARGUMENT>
    constexpr auto operator()(const ARGUMENT&) const -> RESULT {
        std::unreachable();
    }
};

/** Representative witness for probing an operation in the consuming
 * category: it accepts its argument only as a non-`const` rvalue, so an
 * operation that hands the argument on as a `const` lvalue does not match
 * it.
 *
 * `probe_witness` takes `const ARGUMENT&` and so accepts every category,
 * which is what makes it the right witness for asking whether an operation
 * exists at all, and the wrong one for asking which category an operation
 * passes its argument on in: it answers yes either way. The argument type
 * is named rather than deduced so that `ARGUMENT&&` is an rvalue reference
 * and not a forwarding reference.
 *
 * Its body establishes only the result type, for the reason `probe_witness`
 * records.
 */
//! \expos
template <class ARGUMENT, class RESULT>
struct consuming_probe_witness {
    constexpr auto operator()(ARGUMENT&&) const -> RESULT { std::unreachable(); }
};

/** The two-argument counterpart of `probe_witness`, for probing derived
 * operations templated over an arbitrary binary callable (`zip_with`, and
 * the state-combining function `fold_left`/`fold_right` take). A single
 * callable type cannot serve both roles: the dual-basis `invoke` derivation
 * curries its callable one argument at a time, and a callable invocable
 * with either one argument or two would be accepted after the first and
 * never reach the second, silently truncating the arity the probe means to
 * exercise.
 *
 * Its body establishes only the result type, for the reason `probe_witness`
 * records.
 */
//! \expos
template <class RESULT>
struct probe_witness2 {
    template <class FIRST, class SECOND>
    constexpr auto operator()(const FIRST&, const SECOND&) const -> RESULT {
        std::unreachable();
    }
};

} // namespace detail

} // namespace beman::fingertree

#endif // BEMAN_FINGERTREE_TYPECLASS_TYPECLASS_BASE_HPP
