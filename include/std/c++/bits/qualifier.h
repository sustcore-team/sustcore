/**
 * @file qualifier.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief qualifiers (const, volatile, references and etc.)
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace std {
    // Const - Volatile Modifiers
    template <typename _Tp>
    struct remove_const {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_const<const _Tp> {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_volatile {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_volatile<volatile _Tp> {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_cv {
        using type = __remove_cv(_Tp);
    };

    template <typename _Tp>
    struct add_const {
        using type = const _Tp;
    };
    template <typename _Tp>
    struct add_volatile {
        using type = volatile _Tp;
    };
    template <typename _Tp>
    struct add_cv {
        using type = volatile const _Tp;
    };

    template <typename _Tp>
    using remove_const_t = typename remove_const<_Tp>::type;
    template <typename _Tp>
    using remove_volatile_t = typename remove_volatile<_Tp>::type;
    template <typename _Tp>
    using remove_cv_t = typename remove_cv<_Tp>::type;
    template <typename _Tp>
    using add_const_t = typename add_const<_Tp>::type;
    template <typename _Tp>
    using add_volatile_t = typename add_volatile<_Tp>::type;
    template <typename _Tp>
    using add_cv_t = typename add_cv<_Tp>::type;

    // Reference Modifiers
    template <typename _Tp>
    struct remove_reference {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_reference<_Tp&> {
        using type = _Tp;
    };
    template <typename _Tp>
    struct remove_reference<_Tp&&> {
        using type = _Tp;
    };

    template <typename _Tp>
    struct add_lvalue_reference {
        using type = _Tp&;
    };
    template <typename _Tp>
    struct add_lvalue_reference<_Tp&> {
        using type = _Tp&;
    };
    template <typename _Tp>
    struct add_lvalue_reference<_Tp&&> {
        using type = _Tp&;
    };

    template <typename _Tp>
    struct add_rvalue_reference {
        using type = _Tp&&;
    };
    template <typename _Tp>
    struct add_rvalue_reference<_Tp&> {
        using type = _Tp&&;
    };
    template <typename _Tp>
    struct add_rvalue_reference<_Tp&&> {
        using type = _Tp&&;
    };

    template <typename _Tp>
    using remove_reference_t = typename remove_reference<_Tp>::type;
    template <typename _Tp>
    using add_lvalue_reference_t = typename add_lvalue_reference<_Tp>::type;
    template <typename _Tp>
    using add_rvalue_reference_t = typename add_rvalue_reference<_Tp>::type;
    template <typename _Tp>
    using remove_cvref_t = remove_cv_t<remove_reference_t<_Tp>>;

    // Note: a temporary implementation of decay_t, which only removes const
    // and volatile qualifiers, as well as references.
    template <typename _Tp>
    using decay_t = remove_cvref_t<_Tp>;
}  // namespace std
