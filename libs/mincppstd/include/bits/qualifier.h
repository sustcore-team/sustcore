/**
 * @file qualifier.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 mincppstd 的 C++ 标准库兼容层提供 cv 与引用限定符工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace std {
    // Const - Volatile Modifiers
    template <typename _Tp>
    struct remove_const {
        using type = __remove_const(_Tp);
    };
    template <typename _Tp>
    struct remove_volatile {
        using type = __remove_volatile(_Tp);
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
        using type = __remove_reference_t(_Tp);
    };

    template <typename _Tp>
    struct add_lvalue_reference {
        using type = __add_lvalue_reference(_Tp);
    };

    template <typename _Tp>
    struct add_rvalue_reference {
        using type = __add_rvalue_reference(_Tp);
    };

    template <typename _Tp>
    using remove_reference_t = typename remove_reference<_Tp>::type;
    template <typename _Tp>
    using add_lvalue_reference_t = typename add_lvalue_reference<_Tp>::type;
    template <typename _Tp>
    using add_rvalue_reference_t = typename add_rvalue_reference<_Tp>::type;

    template <typename _Tp>
    struct remove_cvref {
        using type = __remove_cvref(_Tp);
    };

    template <typename _Tp>
    using remove_cvref_t = typename remove_cvref<_Tp>::type;

    template <typename _Tp>
    struct decay {
        using type = __decay(_Tp);
    };

    template <typename _Tp>
    using decay_t = typename decay<_Tp>::type;
}  // namespace std
