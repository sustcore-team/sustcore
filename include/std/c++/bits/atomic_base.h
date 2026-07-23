/**
 * @file atomic_base.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief atomic base
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/move.h>
#include <bits/stdlib_assert.h>
#include <feature/attributes.h>

#include <new>
#include <type_traits>

#define ATOMIC_BOOL_LOCK_FREE     __GCC_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE     __GCC_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE  __GCC_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_CHAR8_T_LOCK_FREE  __GCC_ATOMIC_CHAR8_T_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __GCC_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __GCC_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE    __GCC_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE      __GCC_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE     __GCC_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE    __GCC_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE  __GCC_ATOMIC_POINTER_LOCK_FREE

namespace std {

    /**
     * @defgroup atomics Atomics
     *
     * Components for performing atomic operations.
     * @{
     */

    /// Enumeration for memory_order
    enum class memory_order : int {
        relaxed,
        consume,
        acquire,
        release,
        acq_rel,
        seq_cst
    };

    inline constexpr memory_order memory_order_relaxed = memory_order::relaxed;
    inline constexpr memory_order memory_order_consume = memory_order::consume;
    inline constexpr memory_order memory_order_acquire = memory_order::acquire;
    inline constexpr memory_order memory_order_release = memory_order::release;
    inline constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
    inline constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

    /// @cond undocumented
    enum __memory_order_modifier {
        __memory_order_mask          = 0x0ffff,
        __memory_order_modifier_mask = 0xffff0000,
        __memory_order_hle_acquire   = 0x10000,
        __memory_order_hle_release   = 0x20000
    };
    /// @endcond

    constexpr memory_order operator|(
        memory_order order, __memory_order_modifier modifier) noexcept {
        return memory_order(int(order) | int(modifier));
    }

    constexpr memory_order operator&(
        memory_order order, __memory_order_modifier modifier) noexcept {
        return memory_order(int(order) & int(modifier));
    }

    /// @cond undocumented

    // 比较交换失败时, 失败序不能保留 release 语义.
    constexpr memory_order __cmpexch_failure_order2(
        memory_order order) noexcept {
        return order == memory_order_acq_rel   ? memory_order_acquire
               : order == memory_order_release ? memory_order_relaxed
                                               : order;
    }

    constexpr memory_order __cmpexch_failure_order(
        memory_order order) noexcept {
        return memory_order(
            __cmpexch_failure_order2(order & __memory_order_mask) |
            __memory_order_modifier(order & __memory_order_modifier_mask));
    }

    constexpr bool __is_valid_cmpexch_failure_order(
        memory_order order) noexcept {
        return (order & __memory_order_mask) != memory_order_release &&
               (order & __memory_order_mask) != memory_order_acq_rel;
    }

    // 原子类型底层实现的前置声明.
    template <typename _IntTp>
    struct __atomic_base;

    /// @endcond

    __ATTR_ALWAYS_INLINE__ void atomic_thread_fence(
        memory_order order) noexcept {
        __atomic_thread_fence(int(order));
    }

    __ATTR_ALWAYS_INLINE__ void atomic_signal_fence(
        memory_order order) noexcept {
        __atomic_signal_fence(int(order));
    }

    /// 返回一个与输入值相同、但不延续依赖链的副本.
    template <typename _Tp>
    inline _Tp kill_dependency(_Tp value) noexcept {
        _Tp result(value);
        return result;
    }

    /// @cond undocumented
    /// @endcond

#define ATOMIC_VAR_INIT(_VI) \
    {                        \
        _VI                  \
    }

    template <typename _Tp>
    struct atomic;

    template <typename _Tp>
    struct atomic<_Tp*>;

    // 某些目标平台上的 test-and-set 置位值未必等于 1.
#if __GCC_ATOMIC_TEST_AND_SET_TRUEVAL == 1
    using __atomic_flag_data_type = bool;
#else
    using __atomic_flag_data_type = unsigned char;
#endif

    /// @cond undocumented

    /*
     *  atomic_flag 的底层存储布局.
     *
     *  该类型仅保存一份平凡数据, 便于 atomic_flag 继承后保持标准布局,
     *  也便于底层原子内建统一按这份存储进行操作.
     */
    extern "C" {

    struct __atomic_flag_base {
        __atomic_flag_data_type _value = {};
    };
    }

    /// @endcond

#define ATOMIC_FLAG_INIT \
    {                    \
        0                \
    }

    /// atomic_flag
    struct atomic_flag : public __atomic_flag_base {
        atomic_flag() noexcept                              = default;
        ~atomic_flag() noexcept                             = default;
        atomic_flag(const atomic_flag&)                     = delete;
        atomic_flag& operator=(const atomic_flag&)          = delete;
        atomic_flag& operator=(const atomic_flag&) volatile = delete;

        // 允许用与 ATOMIC_FLAG_INIT 对应的值初始化.
        constexpr atomic_flag(bool value) noexcept
            : __atomic_flag_base{_S_init(value)} {}

        __ATTR_ALWAYS_INLINE__ bool test_and_set(
            memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_test_and_set(&_value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ bool test_and_set(
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_test_and_set(&_value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ bool test(
            memory_order order = memory_order_seq_cst) const noexcept {
            __atomic_flag_data_type current_value;
            __atomic_load(&_value, &current_value, int(order));
            return current_value == __GCC_ATOMIC_TEST_AND_SET_TRUEVAL;
        }

        __ATTR_ALWAYS_INLINE__ bool test(
            memory_order order = memory_order_seq_cst) const volatile noexcept {
            __atomic_flag_data_type current_value;
            __atomic_load(&_value, &current_value, int(order));
            return current_value == __GCC_ATOMIC_TEST_AND_SET_TRUEVAL;
        }

        __ATTR_ALWAYS_INLINE__ void clear(
            memory_order order = memory_order_seq_cst) noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_consume);
            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            __atomic_clear(&_value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ void clear(
            memory_order order = memory_order_seq_cst) volatile noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_consume);
            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            __atomic_clear(&_value, int(order));
        }

    private:
        static constexpr __atomic_flag_data_type _S_init(bool value) {
            return value ? __GCC_ATOMIC_TEST_AND_SET_TRUEVAL : 0;
        }
    };

    /// @cond undocumented

    /// 整数原子的公共底层实现.
    //
    // 该实现面向编译器原子内建支持的整数标量类型, 默认假设其宽度满足
    // 目标平台原子访存的基本要求.

    namespace __atomic_impl {
        template <typename _Tp>
        using _Val = typename remove_volatile<_Tp>::type;

        template <typename _Tp>
        _Tp __fetch_min(_Tp* ptr, _Val<_Tp> value, memory_order order) noexcept;

        template <typename _Tp>
        _Tp __fetch_max(_Tp* ptr, _Val<_Tp> value, memory_order order) noexcept;
    }  // namespace __atomic_impl

    template <typename _ITp>
    struct __atomic_base {
        using value_type      = _ITp;
        using difference_type = value_type;

    private:
        using __int_type = _ITp;

        static constexpr int _S_alignment = sizeof(_ITp) > alignof(_ITp)
                                                ? sizeof(_ITp)
                                                : alignof(_ITp);

        alignas(_S_alignment) __int_type _value = 0;

    public:
        __atomic_base() noexcept                                = default;
        ~__atomic_base() noexcept                               = default;
        __atomic_base(const __atomic_base&)                     = delete;
        __atomic_base& operator=(const __atomic_base&)          = delete;
        __atomic_base& operator=(const __atomic_base&) volatile = delete;

        constexpr __atomic_base(__int_type value) noexcept : _value(value) {}

        operator __int_type() const noexcept {
            return load();
        }

        operator __int_type() const volatile noexcept {
            return load();
        }

        __int_type operator=(__int_type value) noexcept {
            store(value);
            return value;
        }

        __int_type operator=(__int_type value) volatile noexcept {
            store(value);
            return value;
        }

        __int_type operator++(int) noexcept {
            return fetch_add(1);
        }

        __int_type operator++(int) volatile noexcept {
            return fetch_add(1);
        }

        __int_type operator--(int) noexcept {
            return fetch_sub(1);
        }

        __int_type operator--(int) volatile noexcept {
            return fetch_sub(1);
        }

        __int_type operator++() noexcept {
            return __atomic_add_fetch(&_value, 1, int(memory_order_seq_cst));
        }

        __int_type operator++() volatile noexcept {
            return __atomic_add_fetch(&_value, 1, int(memory_order_seq_cst));
        }

        __int_type operator--() noexcept {
            return __atomic_sub_fetch(&_value, 1, int(memory_order_seq_cst));
        }

        __int_type operator--() volatile noexcept {
            return __atomic_sub_fetch(&_value, 1, int(memory_order_seq_cst));
        }

        __int_type operator+=(__int_type value) noexcept {
            return __atomic_add_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator+=(__int_type value) volatile noexcept {
            return __atomic_add_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator-=(__int_type value) noexcept {
            return __atomic_sub_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator-=(__int_type value) volatile noexcept {
            return __atomic_sub_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator&=(__int_type value) noexcept {
            return __atomic_and_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator&=(__int_type value) volatile noexcept {
            return __atomic_and_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator|=(__int_type value) noexcept {
            return __atomic_or_fetch(&_value, value, int(memory_order_seq_cst));
        }

        __int_type operator|=(__int_type value) volatile noexcept {
            return __atomic_or_fetch(&_value, value, int(memory_order_seq_cst));
        }

        __int_type operator^=(__int_type value) noexcept {
            return __atomic_xor_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        __int_type operator^=(__int_type value) volatile noexcept {
            return __atomic_xor_fetch(&_value, value,
                                      int(memory_order_seq_cst));
        }

        bool is_lock_free() const noexcept {
            // 传入按最小对齐构造的伪地址, 仅用于查询 lock-free 属性.
            return __atomic_is_lock_free(
                sizeof(_value), reinterpret_cast<void*>(-_S_alignment));
        }

        bool is_lock_free() const volatile noexcept {
            // 传入按最小对齐构造的伪地址, 仅用于查询 lock-free 属性.
            return __atomic_is_lock_free(
                sizeof(_value), reinterpret_cast<void*>(-_S_alignment));
        }

        __ATTR_ALWAYS_INLINE__ void store(
            __int_type value,
            memory_order order = memory_order_seq_cst) noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);
            __stdlib_assert(basic_order != memory_order_consume);

            __atomic_store_n(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ void store(
            __int_type value,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);
            __stdlib_assert(basic_order != memory_order_consume);

            __atomic_store_n(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        load(memory_order order = memory_order_seq_cst) const noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_release);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            return __atomic_load_n(&_value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type load(
            memory_order order = memory_order_seq_cst) const volatile noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_release);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            return __atomic_load_n(&_value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        exchange(__int_type value,
                 memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_exchange_n(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        exchange(__int_type value,
                 memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_exchange_n(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __int_type& expected, __int_type desired,
            memory_order success_order, memory_order failure_order) noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_value, &expected, desired, 1,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __int_type& expected, __int_type desired,
            memory_order success_order,
            memory_order failure_order) volatile noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_value, &expected, desired, 1,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __int_type& expected, __int_type desired,
            memory_order order = memory_order_seq_cst) noexcept {
            return compare_exchange_weak(expected, desired, order,
                                         __cmpexch_failure_order(order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __int_type& expected, __int_type desired,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return compare_exchange_weak(expected, desired, order,
                                         __cmpexch_failure_order(order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __int_type& expected, __int_type desired,
            memory_order success_order, memory_order failure_order) noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_value, &expected, desired, 0,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __int_type& expected, __int_type desired,
            memory_order success_order,
            memory_order failure_order) volatile noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_value, &expected, desired, 0,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __int_type& expected, __int_type desired,
            memory_order order = memory_order_seq_cst) noexcept {
            return compare_exchange_strong(expected, desired, order,
                                           __cmpexch_failure_order(order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __int_type& expected, __int_type desired,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return compare_exchange_strong(expected, desired, order,
                                           __cmpexch_failure_order(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_add(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_add(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_add(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_add(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_sub(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_sub(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_sub(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_sub(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_and(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_and(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_and(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_and(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_or(__int_type value,
                 memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_or(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_or(__int_type value,
                 memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_or(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_xor(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_xor(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_xor(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_xor(&_value, value, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_min(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_min(&_value, value, order);
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_min(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_min(&_value, value, order);
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_max(__int_type value,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_max(&_value, value, order);
        }

        __ATTR_ALWAYS_INLINE__ __int_type
        fetch_max(__int_type value,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_max(&_value, value, order);
        }
    };

    /// 指针类型的底层原子实现.
    template <typename _PTp>
    struct __atomic_base<_PTp*> {
    private:
        using __pointer_type = _PTp*;

        __pointer_type _pointer = nullptr;

        static constexpr ptrdiff_t _S_type_size(ptrdiff_t offset) {
            return offset * sizeof(_PTp);
        }

    public:
        __atomic_base() noexcept                                = default;
        ~__atomic_base() noexcept                               = default;
        __atomic_base(const __atomic_base&)                     = delete;
        __atomic_base& operator=(const __atomic_base&)          = delete;
        __atomic_base& operator=(const __atomic_base&) volatile = delete;

        // 仅接收与底层指针类型兼容的初始值.
        constexpr __atomic_base(__pointer_type pointer) noexcept
            : _pointer(pointer) {}

        operator __pointer_type() const noexcept {
            return load();
        }

        operator __pointer_type() const volatile noexcept {
            return load();
        }

        __pointer_type operator=(__pointer_type pointer) noexcept {
            store(pointer);
            return pointer;
        }

        __pointer_type operator=(__pointer_type pointer) volatile noexcept {
            store(pointer);
            return pointer;
        }

        __pointer_type operator++(int) noexcept {
            return fetch_add(1);
        }

        __pointer_type operator++(int) volatile noexcept {
            return fetch_add(1);
        }

        __pointer_type operator--(int) noexcept {
            return fetch_sub(1);
        }

        __pointer_type operator--(int) volatile noexcept {
            return fetch_sub(1);
        }

        __pointer_type operator++() noexcept {
            return __atomic_add_fetch(&_pointer, _S_type_size(1),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator++() volatile noexcept {
            return __atomic_add_fetch(&_pointer, _S_type_size(1),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator--() noexcept {
            return __atomic_sub_fetch(&_pointer, _S_type_size(1),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator--() volatile noexcept {
            return __atomic_sub_fetch(&_pointer, _S_type_size(1),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator+=(ptrdiff_t offset) noexcept {
            return __atomic_add_fetch(&_pointer, _S_type_size(offset),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator+=(ptrdiff_t offset) volatile noexcept {
            return __atomic_add_fetch(&_pointer, _S_type_size(offset),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator-=(ptrdiff_t offset) noexcept {
            return __atomic_sub_fetch(&_pointer, _S_type_size(offset),
                                      int(memory_order_seq_cst));
        }

        __pointer_type operator-=(ptrdiff_t offset) volatile noexcept {
            return __atomic_sub_fetch(&_pointer, _S_type_size(offset),
                                      int(memory_order_seq_cst));
        }

        bool is_lock_free() const noexcept {
            // 传入按最小对齐构造的伪地址, 仅用于查询 lock-free 属性.
            return __atomic_is_lock_free(
                sizeof(_pointer),
                reinterpret_cast<void*>(-__alignof(_pointer)));
        }

        bool is_lock_free() const volatile noexcept {
            // 传入按最小对齐构造的伪地址, 仅用于查询 lock-free 属性.
            return __atomic_is_lock_free(
                sizeof(_pointer),
                reinterpret_cast<void*>(-__alignof(_pointer)));
        }

        __ATTR_ALWAYS_INLINE__ void store(
            __pointer_type pointer,
            memory_order order = memory_order_seq_cst) noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;

            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);
            __stdlib_assert(basic_order != memory_order_consume);

            __atomic_store_n(&_pointer, pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ void store(
            __pointer_type pointer,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_acquire);
            __stdlib_assert(basic_order != memory_order_acq_rel);
            __stdlib_assert(basic_order != memory_order_consume);

            __atomic_store_n(&_pointer, pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        load(memory_order order = memory_order_seq_cst) const noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_release);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            return __atomic_load_n(&_pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type load(
            memory_order order = memory_order_seq_cst) const volatile noexcept {
            memory_order basic_order __attribute__((__unused__)) =
                order & __memory_order_mask;
            __stdlib_assert(basic_order != memory_order_release);
            __stdlib_assert(basic_order != memory_order_acq_rel);

            return __atomic_load_n(&_pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        exchange(__pointer_type pointer,
                 memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_exchange_n(&_pointer, pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        exchange(__pointer_type pointer,
                 memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_exchange_n(&_pointer, pointer, int(order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __pointer_type& expected, __pointer_type desired,
            memory_order success_order, memory_order failure_order) noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_pointer, &expected, desired, 1,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            __pointer_type& expected, __pointer_type desired,
            memory_order success_order,
            memory_order failure_order) volatile noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_pointer, &expected, desired, 1,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __pointer_type& expected, __pointer_type desired,
            memory_order success_order, memory_order failure_order) noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_pointer, &expected, desired, 0,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            __pointer_type& expected, __pointer_type desired,
            memory_order success_order,
            memory_order failure_order) volatile noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            return __atomic_compare_exchange_n(&_pointer, &expected, desired, 0,
                                               int(success_order),
                                               int(failure_order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_add(ptrdiff_t offset,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_add(&_pointer, _S_type_size(offset),
                                      int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_add(ptrdiff_t offset,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_add(&_pointer, _S_type_size(offset),
                                      int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_sub(ptrdiff_t offset,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_sub(&_pointer, _S_type_size(offset),
                                      int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_sub(ptrdiff_t offset,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_fetch_sub(&_pointer, _S_type_size(offset),
                                      int(order));
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_min(__pointer_type pointer,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_min(&_pointer, pointer, order);
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_min(__pointer_type pointer,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_min(&_pointer, pointer, order);
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_max(__pointer_type pointer,
                  memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_max(&_pointer, pointer, order);
        }

        __ATTR_ALWAYS_INLINE__ __pointer_type
        fetch_max(__pointer_type pointer,
                  memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_max(&_pointer, pointer, order);
        }
    };

    namespace __atomic_impl {
        // 原子对象 padding 处理的内部辅助函数.

        template <typename _Tp>
        constexpr bool __maybe_has_padding() {
#if !__has_builtin(__builtin_clear_padding)
            return false;
#elif __has_builtin(__has_unique_object_representations)
            return !__has_unique_object_representations(_Tp) &&
                   !is_same<_Tp, float>::value && !is_same<_Tp, double>::value;
#else
            return true;
#endif
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++17-extensions"

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ constexpr _Tp* __clear_padding(
            _Tp& storage) noexcept {
            auto* ptr = std::__addressof(storage);
#if __has_builtin(__builtin_clear_padding)
            if constexpr (__atomic_impl::__maybe_has_padding<_Tp>())
                __builtin_clear_padding(ptr);
#endif
            return ptr;
        }

        template <bool _AtomicRef = false, typename _Tp>
        __ATTR_ALWAYS_INLINE__ bool __compare_exchange(
            _Tp& storage, _Val<_Tp>& expected, _Val<_Tp>& value, bool is_weak,
            memory_order success_order, memory_order failure_order) noexcept {
            __stdlib_assert(__is_valid_cmpexch_failure_order(failure_order));

            using _Vp              = _Val<_Tp>;
            _Tp* const storage_ptr = std::__addressof(storage);

            if constexpr (!__atomic_impl::__maybe_has_padding<_Vp>()) {
                return __atomic_compare_exchange(
                    storage_ptr, std::__addressof(expected),
                    std::__addressof(value), is_weak, int(success_order),
                    int(failure_order));
            } else if constexpr (!_AtomicRef)  // atomic<T>
            {
                // 先清理待写入值中的 padding 位.
                _Vp* const desired_ptr = __atomic_impl::__clear_padding(value);
                // 失败时才允许回写 expected, 因此先保留一份副本.
                _Vp expected_copy      = expected;
                // 再清理期望值副本中的 padding 位.
                _Vp* const expected_ptr =
                    __atomic_impl::__clear_padding(expected_copy);

                // atomic<T> 内部保存的值会在写入前清理 padding,
                // 因此这里可以直接按值表示比较.
                if (__atomic_compare_exchange(
                        storage_ptr, expected_ptr, desired_ptr, is_weak,
                        int(success_order), int(failure_order)))
                    return true;
                // 如果失败源自值表示不同, 需要把期望值副本写回 expected.
                __builtin_memcpy(std::__addressof(expected), expected_ptr,
                                 sizeof(_Vp));
                return false;
            } else  // atomic_ref<T> 且 T 含有 padding 位.
            {
                // 先清理待写入值中的 padding 位.
                _Vp* const desired_ptr = __atomic_impl::__clear_padding(value);

                // 失败时才允许回写 expected, 因此先保留一份副本.
                _Vp expected_copy = expected;
                // 先按“目标值此前已经清理过 padding”这一常见情况构造期望值.
                _Vp* const expected_ptr =
                    __atomic_impl::__clear_padding(expected_copy);

                // atomic_ref 直接引用外部对象, 失败既可能是真正的值不同,
                // 也可能只是 padding 位不同. 这里通过有限次重试把两者区分开.
                while (true) {
                    // 保留本轮比较前的期望值表示.
                    _Vp original_value = expected_copy;

                    if (__atomic_compare_exchange(
                            storage_ptr, expected_ptr, desired_ptr, is_weak,
                            int(success_order), int(failure_order)))
                        return true;

                    // 保存本轮读到的实际值表示.
                    _Vp current_copy = expected_copy;

                    // 忽略 padding 后再比较真实值表示.
                    if (__builtin_memcmp(
                            __atomic_impl::__clear_padding(original_value),
                            __atomic_impl::__clear_padding(current_copy),
                            sizeof(_Vp)))
                    {
                        // 清理 padding 后仍不同, 说明这是一次真实失败.
                        __builtin_memcpy(std::__addressof(expected),
                                         expected_ptr, sizeof(_Vp));
                        return false;
                    }
                }
            }
        }
#pragma GCC diagnostic pop
    }  // namespace __atomic_impl

    // atomic_ref 与浮点原子操作共享的内部辅助函数.
    namespace __atomic_impl {
        // 与 _Val<T> 类似, 但用于 difference_type 参数.
        template <typename _Tp>
        using _Diff = __conditional_t<is_pointer_v<_Tp>, ptrdiff_t, _Val<_Tp>>;

        template <size_t _Size, size_t _Align>
        __ATTR_ALWAYS_INLINE__ bool is_lock_free() noexcept {
            // 传入按最小对齐构造的伪地址, 仅用于查询 lock-free 属性.
            return __atomic_is_lock_free(_Size,
                                         reinterpret_cast<void*>(-_Align));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ void store(_Tp* ptr, _Val<_Tp> value,
                                          memory_order order) noexcept {
            __atomic_store(ptr, __atomic_impl::__clear_padding(value),
                           int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Val<_Tp> load(const _Tp* ptr,
                                              memory_order order) noexcept {
            alignas(_Tp) unsigned char buf[sizeof(_Tp)];
            auto* dest = reinterpret_cast<_Val<_Tp>*>(buf);
            __atomic_load(ptr, dest, int(order));
            return *dest;
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Val<_Tp> exchange(_Tp* ptr, _Val<_Tp> desired,
                                                  memory_order order) noexcept {
            alignas(_Tp) unsigned char buf[sizeof(_Tp)];
            auto* dest = reinterpret_cast<_Val<_Tp>*>(buf);
            __atomic_exchange(ptr, __atomic_impl::__clear_padding(desired),
                              dest, int(order));
            return *dest;
        }

        template <bool _AtomicRef = false, typename _Tp>
        __ATTR_ALWAYS_INLINE__ bool compare_exchange_weak(
            _Tp* ptr, _Val<_Tp>& expected, _Val<_Tp> desired,
            memory_order success_order, memory_order failure_order) noexcept {
            return __atomic_impl::__compare_exchange<_AtomicRef>(
                *ptr, expected, desired, true, success_order, failure_order);
        }

        template <bool _AtomicRef = false, typename _Tp>
        __ATTR_ALWAYS_INLINE__ bool compare_exchange_strong(
            _Tp* ptr, _Val<_Tp>& expected, _Val<_Tp> desired,
            memory_order success_order, memory_order failure_order) noexcept {
            return __atomic_impl::__compare_exchange<_AtomicRef>(
                *ptr, expected, desired, false, success_order, failure_order);
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp fetch_add(_Tp* ptr, _Diff<_Tp> value,
                                             memory_order order) noexcept {
            return __atomic_fetch_add(ptr, value, int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp fetch_sub(_Tp* ptr, _Diff<_Tp> value,
                                             memory_order order) noexcept {
            return __atomic_fetch_sub(ptr, value, int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp fetch_and(_Tp* ptr, _Val<_Tp> value,
                                             memory_order order) noexcept {
            return __atomic_fetch_and(ptr, value, int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp fetch_or(_Tp* ptr, _Val<_Tp> value,
                                            memory_order order) noexcept {
            return __atomic_fetch_or(ptr, value, int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp fetch_xor(_Tp* ptr, _Val<_Tp> value,
                                             memory_order order) noexcept {
            return __atomic_fetch_xor(ptr, value, int(order));
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp __add_fetch(_Tp* ptr,
                                               _Diff<_Tp> value) noexcept {
            return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp __sub_fetch(_Tp* ptr,
                                               _Diff<_Tp> value) noexcept {
            return __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp __and_fetch(_Tp* ptr,
                                               _Val<_Tp> value) noexcept {
            return __atomic_and_fetch(ptr, value, __ATOMIC_SEQ_CST);
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp __or_fetch(_Tp* ptr,
                                              _Val<_Tp> value) noexcept {
            return __atomic_or_fetch(ptr, value, __ATOMIC_SEQ_CST);
        }

        template <typename _Tp>
        __ATTR_ALWAYS_INLINE__ _Tp __xor_fetch(_Tp* ptr,
                                               _Val<_Tp> value) noexcept {
            return __atomic_xor_fetch(ptr, value, __ATOMIC_SEQ_CST);
        }

        template <typename _Tp>
        concept __atomic_fetch_addable =
            requires(_Tp value) { __atomic_fetch_add(&value, value, 0); };

        template <typename _Tp>
        _Tp __fetch_add_flt(_Tp* ptr, _Val<_Tp> value,
                            memory_order order) noexcept {
            if constexpr (__atomic_fetch_addable<_Tp>)
                return __atomic_fetch_add(ptr, value, int(order));
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value + value;
                while (!compare_exchange_weak(ptr, old_value, new_value, order,
                                              memory_order_relaxed))
                    new_value = old_value + value;
                return old_value;
            }
        }

        template <typename _Tp>
        concept __atomic_fetch_subtractable =
            requires(_Tp value) { __atomic_fetch_sub(&value, value, 0); };

        template <typename _Tp>
        _Tp __fetch_sub_flt(_Tp* ptr, _Val<_Tp> value,
                            memory_order order) noexcept {
            if constexpr (__atomic_fetch_subtractable<_Tp>)
                return __atomic_fetch_sub(ptr, value, int(order));
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value - value;
                while (!compare_exchange_weak(ptr, old_value, new_value, order,
                                              memory_order_relaxed))
                    new_value = old_value - value;
                return old_value;
            }
        }

        template <typename _Tp>
        concept __atomic_add_fetchable =
            requires(_Tp value) { __atomic_add_fetch(&value, value, 0); };

        template <typename _Tp>
        _Tp __add_fetch_flt(_Tp* ptr, _Val<_Tp> value) noexcept {
            if constexpr (__atomic_add_fetchable<_Tp>)
                return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value + value;
                while (!compare_exchange_weak(ptr, old_value, new_value,
                                              memory_order_seq_cst,
                                              memory_order_relaxed))
                    new_value = old_value + value;
                return new_value;
            }
        }

        template <typename _Tp>
        concept __atomic_sub_fetchable =
            requires(_Tp value) { __atomic_sub_fetch(&value, value, 0); };

        template <typename _Tp>
        _Tp __sub_fetch_flt(_Tp* ptr, _Val<_Tp> value) noexcept {
            if constexpr (__atomic_sub_fetchable<_Tp>)
                return __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value - value;
                while (!compare_exchange_weak(ptr, old_value, new_value,
                                              memory_order_seq_cst,
                                              memory_order_relaxed))
                    new_value = old_value - value;
                return new_value;
            }
        }

        template <typename _Tp>
        concept __atomic_fetch_minmaxable = requires(_Tp value) {
            __atomic_fetch_min(&value, value, 0);
            __atomic_fetch_max(&value, value, 0);
        };

        template <typename _Tp>
        _Tp __fetch_min(_Tp* ptr, _Val<_Tp> value,
                        memory_order order) noexcept {
            if constexpr (__atomic_fetch_minmaxable<_Tp>)
                return __atomic_fetch_min(ptr, value, int(order));
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value < value ? old_value : value;
                while (!compare_exchange_weak(ptr, old_value, new_value, order,
                                              memory_order_relaxed))
                    new_value = old_value < value ? old_value : value;
                return old_value;
            }
        }

        template <typename _Tp>
        _Tp __fetch_max(_Tp* ptr, _Val<_Tp> value,
                        memory_order order) noexcept {
            if constexpr (__atomic_fetch_minmaxable<_Tp>)
                return __atomic_fetch_max(ptr, value, int(order));
            else {
                _Val<_Tp> old_value = load(ptr, memory_order_relaxed);
                _Val<_Tp> new_value = old_value > value ? old_value : value;
                while (!compare_exchange_weak(ptr, old_value, new_value, order,
                                              memory_order_relaxed))
                    new_value = old_value > value ? old_value : value;
                return old_value;
            }
        }
    }  // namespace __atomic_impl

    // 浮点原子的公共底层实现.
    template <typename _Fp>
    struct __atomic_float {
        static_assert(is_floating_point_v<_Fp>);

        static constexpr size_t _S_alignment = __alignof__(_Fp);

    public:
        using value_type      = _Fp;
        using difference_type = value_type;

        static constexpr bool is_always_lock_free =
            __atomic_always_lock_free(sizeof(_Fp), 0);

        __atomic_float() = default;

        constexpr __atomic_float(_Fp value) : _value(value) {
            if (!is_constant_evaluated())
                __atomic_impl::__clear_padding(_value);
        }

        __atomic_float(const __atomic_float&)                     = delete;
        __atomic_float& operator=(const __atomic_float&)          = delete;
        __atomic_float& operator=(const __atomic_float&) volatile = delete;

        _Fp operator=(_Fp value) volatile noexcept {
            this->store(value);
            return value;
        }

        _Fp operator=(_Fp value) noexcept {
            this->store(value);
            return value;
        }

        bool is_lock_free() const volatile noexcept {
            return __atomic_impl::is_lock_free<sizeof(_Fp), _S_alignment>();
        }

        bool is_lock_free() const noexcept {
            return __atomic_impl::is_lock_free<sizeof(_Fp), _S_alignment>();
        }

        void store(_Fp value, memory_order order =
                                  memory_order_seq_cst) volatile noexcept {
            __atomic_impl::store(&_value, value, order);
        }

        void store(_Fp value,
                   memory_order order = memory_order_seq_cst) noexcept {
            __atomic_impl::store(&_value, value, order);
        }

        _Fp load(memory_order order = memory_order_seq_cst) const
            volatile noexcept {
            return __atomic_impl::load(&_value, order);
        }

        _Fp load(memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::load(&_value, order);
        }

        operator _Fp() const volatile noexcept {
            return this->load();
        }
        operator _Fp() const noexcept {
            return this->load();
        }

        _Fp exchange(_Fp desired, memory_order order =
                                      memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::exchange(&_value, desired, order);
        }

        _Fp exchange(_Fp desired,
                     memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::exchange(&_value, desired, order);
        }

        bool compare_exchange_weak(_Fp& expected, _Fp desired,
                                   memory_order success_order,
                                   memory_order failure_order) noexcept {
            return __atomic_impl::compare_exchange_weak(
                &_value, expected, desired, success_order, failure_order);
        }

        bool compare_exchange_weak(
            _Fp& expected, _Fp desired, memory_order success_order,
            memory_order failure_order) volatile noexcept {
            return __atomic_impl::compare_exchange_weak(
                &_value, expected, desired, success_order, failure_order);
        }

        bool compare_exchange_strong(_Fp& expected, _Fp desired,
                                     memory_order success_order,
                                     memory_order failure_order) noexcept {
            return __atomic_impl::compare_exchange_strong(
                &_value, expected, desired, success_order, failure_order);
        }

        bool compare_exchange_strong(
            _Fp& expected, _Fp desired, memory_order success_order,
            memory_order failure_order) volatile noexcept {
            return __atomic_impl::compare_exchange_strong(
                &_value, expected, desired, success_order, failure_order);
        }

        bool compare_exchange_weak(
            _Fp& expected, _Fp desired,
            memory_order order = memory_order_seq_cst) noexcept {
            return compare_exchange_weak(expected, desired, order,
                                         __cmpexch_failure_order(order));
        }

        bool compare_exchange_weak(
            _Fp& expected, _Fp desired,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return compare_exchange_weak(expected, desired, order,
                                         __cmpexch_failure_order(order));
        }

        bool compare_exchange_strong(
            _Fp& expected, _Fp desired,
            memory_order order = memory_order_seq_cst) noexcept {
            return compare_exchange_strong(expected, desired, order,
                                           __cmpexch_failure_order(order));
        }

        bool compare_exchange_strong(
            _Fp& expected, _Fp desired,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return compare_exchange_strong(expected, desired, order,
                                           __cmpexch_failure_order(order));
        }

        value_type fetch_add(
            value_type value,
            memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_add_flt(&_value, value, order);
        }

        value_type fetch_add(
            value_type value,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_add_flt(&_value, value, order);
        }

        value_type fetch_sub(
            value_type value,
            memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_sub_flt(&_value, value, order);
        }

        value_type fetch_sub(
            value_type value,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_sub_flt(&_value, value, order);
        }

        value_type fetch_min(
            value_type value,
            memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_min(&_value, value, order);
        }

        value_type fetch_min(
            value_type value,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_min(&_value, value, order);
        }

        value_type fetch_max(
            value_type value,
            memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_impl::__fetch_max(&_value, value, order);
        }

        value_type fetch_max(
            value_type value,
            memory_order order = memory_order_seq_cst) volatile noexcept {
            return __atomic_impl::__fetch_max(&_value, value, order);
        }

        value_type operator+=(value_type value) noexcept {
            return __atomic_impl::__add_fetch_flt(&_value, value);
        }

        value_type operator+=(value_type value) volatile noexcept {
            return __atomic_impl::__add_fetch_flt(&_value, value);
        }

        value_type operator-=(value_type value) noexcept {
            return __atomic_impl::__sub_fetch_flt(&_value, value);
        }

        value_type operator-=(value_type value) volatile noexcept {
            return __atomic_impl::__sub_fetch_flt(&_value, value);
        }

    private:
        alignas(_S_alignment) _Fp _value = 0;
    };

    // atomic_ref 的实现分层:
    // 1. const 基类提供只读公共接口
    // 2. 非 const 基类补充可写公共接口
    // 3. 各特化层按整数、浮点、指针补充类型相关操作
    // 4. 最外层 atomic_ref 只负责公开包装与约束校验

    template <typename _Tp>
    struct __atomic_ref_base;

    template <typename _Tp>
    struct __atomic_ref_base<const _Tp> {
    private:
        using _Vt = remove_cv_t<_Tp>;

        static consteval bool _S_is_always_lock_free() {
            if constexpr (is_pointer_v<_Vt>)
                return ATOMIC_POINTER_LOCK_FREE == 2;
            else
                return __atomic_always_lock_free(sizeof(_Vt), 0);
        }

        static consteval int _S_required_alignment() {
            if constexpr (is_floating_point_v<_Vt> || is_pointer_v<_Vt>)
                return __alignof__(_Vt);
            else if constexpr ((sizeof(_Vt) & (sizeof(_Vt) - 1)) ||
                               sizeof(_Vt) > 16)
                return alignof(_Vt);
            else
                // 1/2/4/8/16 字节类型至少按自身大小对齐.
                return (sizeof(_Vt) > alignof(_Vt)) ? sizeof(_Vt)
                                                    : alignof(_Vt);
        }

    public:
        using value_type = _Vt;
        static_assert(is_trivially_copyable_v<value_type>);

        static constexpr bool is_always_lock_free = _S_is_always_lock_free();
        static_assert(is_always_lock_free || !is_volatile_v<_Tp>,
                      "atomic operations on volatile T must be lock-free");

        static constexpr size_t required_alignment = _S_required_alignment();

        __atomic_ref_base()                                    = delete;
        __atomic_ref_base& operator=(const __atomic_ref_base&) = delete;

        explicit __atomic_ref_base(const _Tp* ptr) noexcept
            : _pointer(const_cast<_Tp*>(ptr)) {}

        __atomic_ref_base(const __atomic_ref_base&) noexcept = default;

        operator value_type() const noexcept {
            return this->load();
        }

        bool is_lock_free() const noexcept {
            return __atomic_impl::is_lock_free<sizeof(_Tp),
                                               required_alignment>();
        }

        value_type load(
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::load(_pointer, order);
        }

        __ATTR_ALWAYS_INLINE__ constexpr const _Tp* address() const noexcept {
            return _pointer;
        }

    protected:
        _Tp* _pointer;
    };

    template <typename _Tp>
    struct __atomic_ref_base : __atomic_ref_base<const _Tp> {
        using value_type = typename __atomic_ref_base<const _Tp>::value_type;

        explicit __atomic_ref_base(_Tp* ptr) noexcept
            : __atomic_ref_base<const _Tp>(ptr) {}

        value_type operator=(value_type value) const noexcept {
            this->store(value);
            return value;
        }

        void store(value_type value,
                   memory_order order = memory_order_seq_cst) const noexcept {
            __atomic_impl::store(this->_pointer, value, order);
        }

        value_type exchange(
            value_type desired,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::exchange(this->_pointer, desired, order);
        }

        bool compare_exchange_weak(value_type& expected, value_type desired,
                                   memory_order success_order,
                                   memory_order failure_order) const noexcept {
            return __atomic_impl::compare_exchange_weak<true>(
                this->_pointer, expected, desired, success_order,
                failure_order);
        }

        bool compare_exchange_strong(
            value_type& expected, value_type desired,
            memory_order success_order,
            memory_order failure_order) const noexcept {
            return __atomic_impl::compare_exchange_strong<true>(
                this->_pointer, expected, desired, success_order,
                failure_order);
        }

        bool compare_exchange_weak(
            value_type& expected, value_type desired,
            memory_order order = memory_order_seq_cst) const noexcept {
            return compare_exchange_weak(expected, desired, order,
                                         __cmpexch_failure_order(order));
        }

        bool compare_exchange_strong(
            value_type& expected, value_type desired,
            memory_order order = memory_order_seq_cst) const noexcept {
            return compare_exchange_strong(expected, desired, order,
                                           __cmpexch_failure_order(order));
        }

        __ATTR_ALWAYS_INLINE__ constexpr _Tp* address() const noexcept {
            return this->_pointer;
        }
    };

    template <typename _Tp,
              bool = is_integral_v<_Tp> && !is_same_v<remove_cv_t<_Tp>, bool>,
              bool = is_floating_point_v<_Tp>, bool = is_pointer_v<_Tp>>
    struct __atomic_ref;

    // 非整数、非浮点、非指针类型的 atomic_ref 公共实现.
    template <typename _Tp>
    struct __atomic_ref<_Tp, false, false, false> : __atomic_ref_base<_Tp> {
        using __atomic_ref_base<_Tp>::__atomic_ref_base;
        using __atomic_ref_base<_Tp>::operator=;
    };

    template <typename _Tp>
    struct __atomic_ref<const _Tp, false, false, false>
        : __atomic_ref_base<const _Tp> {
        using __atomic_ref_base<const _Tp>::__atomic_ref_base;
    };

    // 整数类型的 atomic_ref 公共实现.
    template <typename _Tp>
    struct __atomic_ref<_Tp, true, false, false> : __atomic_ref_base<_Tp> {
        using value_type      = typename __atomic_ref_base<_Tp>::value_type;
        using difference_type = value_type;

        using __atomic_ref_base<_Tp>::__atomic_ref_base;
        using __atomic_ref_base<_Tp>::operator=;

        value_type fetch_add(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_add(this->_pointer, value, order);
        }

        value_type fetch_sub(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_sub(this->_pointer, value, order);
        }

        value_type fetch_and(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_and(this->_pointer, value, order);
        }

        value_type fetch_or(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_or(this->_pointer, value, order);
        }

        value_type fetch_xor(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_xor(this->_pointer, value, order);
        }

        value_type fetch_min(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_min(this->_pointer, value, order);
        }

        value_type fetch_max(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_max(this->_pointer, value, order);
        }

        __ATTR_ALWAYS_INLINE__ value_type operator++(int) const noexcept {
            return fetch_add(1);
        }

        __ATTR_ALWAYS_INLINE__ value_type operator--(int) const noexcept {
            return fetch_sub(1);
        }

        value_type operator++() const noexcept {
            return __atomic_impl::__add_fetch(this->_pointer, value_type(1));
        }

        value_type operator--() const noexcept {
            return __atomic_impl::__sub_fetch(this->_pointer, value_type(1));
        }

        value_type operator+=(value_type value) const noexcept {
            return __atomic_impl::__add_fetch(this->_pointer, value);
        }

        value_type operator-=(value_type value) const noexcept {
            return __atomic_impl::__sub_fetch(this->_pointer, value);
        }

        value_type operator&=(value_type value) const noexcept {
            return __atomic_impl::__and_fetch(this->_pointer, value);
        }

        value_type operator|=(value_type value) const noexcept {
            return __atomic_impl::__or_fetch(this->_pointer, value);
        }

        value_type operator^=(value_type value) const noexcept {
            return __atomic_impl::__xor_fetch(this->_pointer, value);
        }
    };

    template <typename _Tp>
    struct __atomic_ref<const _Tp, true, false, false>
        : __atomic_ref_base<const _Tp> {
        using difference_type =
            typename __atomic_ref_base<const _Tp>::value_type;
        using __atomic_ref_base<const _Tp>::__atomic_ref_base;
    };

    // 浮点类型的 atomic_ref 公共实现.
    template <typename _Fp>
    struct __atomic_ref<_Fp, false, true, false> : __atomic_ref_base<_Fp> {
        using value_type      = typename __atomic_ref_base<_Fp>::value_type;
        using difference_type = value_type;

        using __atomic_ref_base<_Fp>::__atomic_ref_base;
        using __atomic_ref_base<_Fp>::operator=;

        value_type fetch_add(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_add_flt(this->_pointer, value, order);
        }

        value_type fetch_sub(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_sub_flt(this->_pointer, value, order);
        }

        value_type fetch_min(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_min(this->_pointer, value, order);
        }

        value_type fetch_max(
            value_type value,
            memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_max(this->_pointer, value, order);
        }

        value_type operator+=(value_type value) const noexcept {
            return __atomic_impl::__add_fetch_flt(this->_pointer, value);
        }

        value_type operator-=(value_type value) const noexcept {
            return __atomic_impl::__sub_fetch_flt(this->_pointer, value);
        }
    };

    template <typename _Fp>
    struct __atomic_ref<const _Fp, false, true, false>
        : __atomic_ref_base<const _Fp> {
        using difference_type =
            typename __atomic_ref_base<const _Fp>::value_type;
        using __atomic_ref_base<const _Fp>::__atomic_ref_base;
    };

    // 指针类型的 atomic_ref 公共实现.
    template <typename _Pt>
    struct __atomic_ref<_Pt, false, false, true> : __atomic_ref_base<_Pt> {
        using value_type      = typename __atomic_ref_base<_Pt>::value_type;
        using difference_type = ptrdiff_t;

        using __atomic_ref_base<_Pt>::__atomic_ref_base;
        using __atomic_ref_base<_Pt>::operator=;
        __ATTR_ALWAYS_INLINE__ value_type
        fetch_add(difference_type offset,
                  memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_add(this->_pointer,
                                            _S_type_size(offset), order);
        }

        __ATTR_ALWAYS_INLINE__ value_type
        fetch_sub(difference_type offset,
                  memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::fetch_sub(this->_pointer,
                                            _S_type_size(offset), order);
        }

        __ATTR_ALWAYS_INLINE__ value_type
        fetch_min(value_type value,
                  memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_min(this->_pointer, value, order);
        }

        __ATTR_ALWAYS_INLINE__ value_type
        fetch_max(value_type value,
                  memory_order order = memory_order_seq_cst) const noexcept {
            return __atomic_impl::__fetch_max(this->_pointer, value, order);
        }

        value_type operator++(int) const noexcept {
            return fetch_add(1);
        }

        value_type operator--(int) const noexcept {
            return fetch_sub(1);
        }

        value_type operator++() const noexcept {
            return __atomic_impl::__add_fetch(this->_pointer, _S_type_size(1));
        }

        value_type operator--() const noexcept {
            return __atomic_impl::__sub_fetch(this->_pointer, _S_type_size(1));
        }

        value_type operator+=(difference_type offset) const noexcept {
            return __atomic_impl::__add_fetch(this->_pointer,
                                              _S_type_size(offset));
        }

        value_type operator-=(difference_type offset) const noexcept {
            return __atomic_impl::__sub_fetch(this->_pointer,
                                              _S_type_size(offset));
        }

    private:
        static constexpr ptrdiff_t _S_type_size(ptrdiff_t offset) noexcept {
            using _Et = remove_pointer_t<value_type>;
            static_assert(is_object_v<_Et>);
            return offset * sizeof(_Et);
        }
    };

    template <typename _Pt>
    struct __atomic_ref<const _Pt, false, false, true>
        : __atomic_ref_base<const _Pt> {
        using difference_type = ptrdiff_t;
        using __atomic_ref_base<const _Pt>::__atomic_ref_base;
    };

    /// @endcond

    /// @} group atomics

}  // namespace std
