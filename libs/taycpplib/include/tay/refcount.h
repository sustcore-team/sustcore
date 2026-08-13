/**
 * @file refcount.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供原子引用计数、CRTP 生命周期基类和 pin RAII 工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace tay {
    /**
     * @brief 维护独立于对象所有权策略的原子引用数量。
     *
     * 计数器只负责引用数量和最后引用的 release/acquire 同步，不决定对象如何销毁。
     * `acquire()` 可建立第一个引用；`try_acquire()` 只在已有引用时成功，适合防止对象
     * 从零引用状态复活。
     *
     * @tparam Count 无符号整数计数类型。
     */
    template <typename Count = size_t>
        requires(std::is_integral_v<Count> && std::is_unsigned_v<Count> &&
                 !std::is_same_v<std::remove_cv_t<Count>, bool>)
    class reference_counter final {
    public:
        using count_type = Count;

        /** @brief 构造引用数量为零的计数器。 */
        constexpr reference_counter() noexcept = default;

        /**
         * @brief 以指定引用数量构造计数器。
         * @param initial 初始引用数量。
         */
        constexpr explicit reference_counter(count_type initial) noexcept : count_(initial) {}

        reference_counter(const reference_counter &)            = delete;
        reference_counter &operator=(const reference_counter &) = delete;
        reference_counter(reference_counter &&)                 = delete;
        reference_counter &operator=(reference_counter &&)      = delete;

        /**
         * @brief 读取当前引用数量。
         * @return 调用瞬间观察到的引用数量快照。
         */
        [[nodiscard]] count_type count() const noexcept {
            return count_.load(std::memory_order_acquire);
        }

        /** @brief 判断当前是否没有引用。 */
        [[nodiscard]] bool empty() const noexcept {
            return count() == 0;
        }

        /**
         * @brief 无条件增加一个引用。
         * @pre 调用方持有足以证明对象仍存活的外部所有权，且计数尚未达到最大值。
         */
        void acquire() noexcept {
            count_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 仅在对象仍有引用时增加一个引用。
         * @return 成功取得引用返回 true；计数为零或已经饱和时返回 false。
         * @note 成功的 acquire 与发布该对象的操作建立 acquire 顺序。
         */
        [[nodiscard]] bool try_acquire() noexcept {
            auto current = count_.load(std::memory_order_acquire);
            while (current != 0 && current != std::numeric_limits<count_type>::max()) {
                if (count_.compare_exchange_weak(current, static_cast<count_type>(current + 1),
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed))
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 释放一个引用。
         * @return 本次释放使计数从一变为零时返回 true；否则返回 false。
         * @note 最后引用路径执行 acquire fence，使对象销毁观察到此前引用持有者的写入。
         * @pre 调用方持有一个尚未释放的引用。
         */
        [[nodiscard]] bool release() noexcept {
            auto current = count_.load(std::memory_order_relaxed);
            while (current != 0) {
                if (count_.compare_exchange_weak(current, static_cast<count_type>(current - 1),
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed))
                {
                    if (current == 1)
                        std::atomic_thread_fence(std::memory_order_acquire);
                    return current == 1;
                }
            }
            return false;
        }

    private:
        std::atomic<count_type> count_{};
    };

    /**
     * @brief 将 `reference_counter` 与派生对象的零引用回调组合。
     *
     * 派生类型应实现 `on_zero_references() noexcept`。为兼容现有 Tay 调用方，也接受
     * `on_death() noexcept`。基类不执行 `delete this`；销毁、回收或状态迁移完全由派生类
     * 的回调决定。
     *
     * @tparam Derived CRTP 派生类型。
     * @tparam Count 引用计数使用的无符号整数类型。
     */
    template <typename Derived, typename Count = size_t>
    class ref_counted {
    public:
        using count_type = Count;

        ref_counted(const ref_counted &)            = delete;
        ref_counted &operator=(const ref_counted &) = delete;
        ref_counted(ref_counted &&)                 = delete;
        ref_counted &operator=(ref_counted &&)      = delete;

        /** @brief 建立一个由调用方拥有的新引用。 */
        void retain() noexcept {
            references_.acquire();
        }

        /** @brief `retain()` 的兼容名称。 */
        void keep() noexcept {
            retain();
        }

        /**
         * @brief 尝试从非零计数取得引用。
         * @return 对象仍存活且引用建立成功时返回 true。
         */
        [[nodiscard]] bool try_retain() noexcept {
            return references_.try_acquire();
        }

        /**
         * @brief 释放调用方持有的引用，并在最后引用消失时通知派生对象。
         * @pre 调用方持有一个尚未释放的引用。
         */
        void release() noexcept {
            if (!references_.release())
                return;
            auto &derived = *static_cast<Derived *>(this);
            if constexpr (requires { derived.on_zero_references(); }) {
                static_assert(noexcept(derived.on_zero_references()),
                              "zero-reference callback must be noexcept");
                derived.on_zero_references();
            } else {
                static_assert(
                    requires { derived.on_death(); },
                    "ref_counted requires on_zero_references() or on_death()");
                static_assert(noexcept(derived.on_death()), "on_death callback must be noexcept");
                derived.on_death();
            }
        }

        /** @brief 返回当前引用数量快照。 */
        [[nodiscard]] count_type ref_count() const noexcept {
            return references_.count();
        }

        /** @brief 判断对象当前是否至少拥有一个引用。 */
        [[nodiscard]] bool alive() const noexcept {
            return !references_.empty();
        }

    protected:
        /**
         * @brief 以指定引用数量构造 CRTP 基类。
         * @param initial 初始引用数量，通常为零。
         */
        constexpr explicit ref_counted(count_type initial = 0) noexcept : references_(initial) {}
        ~ref_counted() = default;

    private:
        reference_counter<count_type> references_;
    };

    /** @brief `ref_counted` 的旧接口兼容别名。 */
    template <typename Derived>
    using refc = ref_counted<Derived>;

    /**
     * @brief 持有通过 `ref_counted` 管理的侵入式强引用。
     * @tparam T 提供 `retain()` 和 `release()` 的对象类型。
     * @note 指针不负责分配对象；最后引用的行为由对象自身的零引用回调决定。
     */
    template <typename T>
    class refc_ptr final {
    public:
        /** @brief 构造空引用。 */
        constexpr refc_ptr() noexcept = default;

        /**
         * @brief 从借用指针建立一个强引用。
         * @param pointer 可为空；非空时调用 `retain()`。
         */
        explicit refc_ptr(T *pointer) noexcept : pointer_(pointer) {
            retain();
        }

        refc_ptr(const refc_ptr &other) noexcept : pointer_(other.pointer_) {
            retain();
        }

        refc_ptr(refc_ptr &&other) noexcept : pointer_(other.pointer_) {
            other.pointer_ = nullptr;
        }

        refc_ptr &operator=(const refc_ptr &other) noexcept {
            if (this == &other)
                return *this;
            if (other.pointer_ != nullptr)
                other.pointer_->retain();
            reset();
            pointer_ = other.pointer_;
            return *this;
        }

        refc_ptr &operator=(refc_ptr &&other) noexcept {
            if (this == &other)
                return *this;
            reset();
            pointer_       = other.pointer_;
            other.pointer_ = nullptr;
            return *this;
        }

        ~refc_ptr() noexcept {
            reset();
        }

        /** @brief 返回所持有的对象指针，不改变引用数量。 */
        [[nodiscard]] T *get() const noexcept {
            return pointer_;
        }

        /** @brief 判断当前是否持有对象。 */
        [[nodiscard]] explicit operator bool() const noexcept {
            return pointer_ != nullptr;
        }

        /** @brief 返回借用指针的旧接口兼容转换，不改变引用数量。 */
        [[nodiscard]] operator T *() const noexcept {
            return pointer_;
        }

        [[nodiscard]] T *operator->() const noexcept {
            return pointer_;
        }

        [[nodiscard]] T &operator*() const noexcept {
            return *pointer_;
        }

        /** @brief 释放当前强引用并变为空引用。 */
        void reset() noexcept {
            if (pointer_ != nullptr)
                pointer_->release();
            pointer_ = nullptr;
        }

    private:
        void retain() noexcept {
            if (pointer_ != nullptr)
                pointer_->retain();
        }

        T *pointer_ = nullptr;
    };

    /**
     * @brief 以 RAII 管理对象的临时 pin。
     *
     * `try_pin()` 调用对象的同名方法；只有成功后才记录对象。析构或 `reset()` 调用
     * `unpin()`。该类型不可复制但可移动，适合保护并发删除期间的短期对象访问。
     *
     * @tparam T 提供 `bool try_pin() noexcept` 和 `void unpin() noexcept` 的对象类型。
     */
    template <typename T>
    class pin_guard final {
    public:
        /** @brief 构造不持有 pin 的 guard。 */
        constexpr pin_guard() noexcept = default;

        pin_guard(const pin_guard &)            = delete;
        pin_guard &operator=(const pin_guard &) = delete;

        pin_guard(pin_guard &&other) noexcept : pointer_(other.pointer_) {
            other.pointer_ = nullptr;
        }

        pin_guard &operator=(pin_guard &&other) noexcept {
            if (this == &other)
                return *this;
            reset();
            pointer_       = other.pointer_;
            other.pointer_ = nullptr;
            return *this;
        }

        ~pin_guard() noexcept {
            reset();
        }

        /**
         * @brief 尝试取得对象 pin。
         * @param object 目标对象；其生命周期在调用期间必须有效。
         * @return 成功时返回持有对象的 guard，失败时返回空 guard。
         */
        [[nodiscard]] static pin_guard try_pin(T &object) noexcept {
            static_assert(noexcept(object.try_pin()), "try_pin must be noexcept");
            static_assert(noexcept(object.unpin()), "unpin must be noexcept");
            return object.try_pin() ? pin_guard(&object) : pin_guard{};
        }

        /** @brief 返回被 pin 的对象指针；空 guard 返回 nullptr。 */
        [[nodiscard]] T *get() const noexcept {
            return pointer_;
        }

        /** @brief 判断 guard 当前是否持有 pin。 */
        [[nodiscard]] explicit operator bool() const noexcept {
            return pointer_ != nullptr;
        }

        [[nodiscard]] T *operator->() const noexcept {
            return pointer_;
        }

        [[nodiscard]] T &operator*() const noexcept {
            return *pointer_;
        }

        /** @brief 释放当前 pin；空 guard 上调用没有效果。 */
        void reset() noexcept {
            if (pointer_ != nullptr)
                pointer_->unpin();
            pointer_ = nullptr;
        }

    private:
        explicit constexpr pin_guard(T *pointer) noexcept : pointer_(pointer) {}

        T *pointer_ = nullptr;
    };
}  // namespace tay
