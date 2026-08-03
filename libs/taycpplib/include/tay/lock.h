/**
 * @file lock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供通用锁所有权和同步值工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/panic.h>
#include <tay/spinlock.h>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace tay {
    struct adopt_lock_t {
        explicit adopt_lock_t() = default;
    };

    struct defer_lock_t {
        explicit defer_lock_t() = default;
    };

    struct try_to_lock_t {
        explicit try_to_lock_t() = default;
    };

    inline constexpr adopt_lock_t adopt_lock{};
    inline constexpr defer_lock_t defer_lock{};
    inline constexpr try_to_lock_t try_to_lock{};

    template <class Lock>
    class lock_guard {
    public:
        explicit lock_guard(Lock& lock) noexcept : lock_(&lock) {
            lock_->lock();
        }

        lock_guard(Lock& lock, adopt_lock_t) noexcept : lock_(&lock) {}

        lock_guard(const lock_guard&)            = delete;
        lock_guard& operator=(const lock_guard&) = delete;
        lock_guard(lock_guard&&)                 = delete;
        lock_guard& operator=(lock_guard&&)      = delete;

        ~lock_guard() noexcept {
            lock_->unlock();
        }

    private:
        Lock* lock_;
    };

    template <class Lock>
    lock_guard(Lock&) -> lock_guard<Lock>;

    template <class Lock>
    class unique_lock {
    public:
        constexpr unique_lock() noexcept = default;

        explicit unique_lock(Lock& lock) : lock_(&lock) {
            this->lock();
        }

        constexpr unique_lock(Lock& lock, defer_lock_t) noexcept : lock_(&lock) {}

        constexpr unique_lock(Lock& lock, adopt_lock_t) noexcept : lock_(&lock), owns_lock_(true) {}

        unique_lock(Lock& lock, try_to_lock_t) : lock_(&lock) {
            owns_lock_ = lock_->try_lock();
        }

        unique_lock(const unique_lock&)            = delete;
        unique_lock& operator=(const unique_lock&) = delete;

        constexpr unique_lock(unique_lock&& other) noexcept
            : lock_(other.lock_), owns_lock_(other.owns_lock_) {
            other.lock_      = nullptr;
            other.owns_lock_ = false;
        }

        constexpr unique_lock& operator=(unique_lock&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            if (owns_lock_) {
                lock_->unlock();
            }
            lock_            = other.lock_;
            owns_lock_       = other.owns_lock_;
            other.lock_      = nullptr;
            other.owns_lock_ = false;
            return *this;
        }

        ~unique_lock() noexcept {
            if (owns_lock_) {
                lock_->unlock();
            }
        }

        void lock() {
            require_lockable();
            lock_->lock();
            owns_lock_ = true;
        }

        [[nodiscard]] bool try_lock() {
            require_lockable();
            owns_lock_ = lock_->try_lock();
            return owns_lock_;
        }

        void unlock() {
            if (!owns_lock_) {
                tay::panic("unique_lock does not own the lock");
            }
            lock_->unlock();
            owns_lock_ = false;
        }

        [[nodiscard]] constexpr bool owns_lock() const noexcept {
            return owns_lock_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return owns_lock();
        }

        [[nodiscard]] constexpr Lock* mutex() const noexcept {
            return lock_;
        }

        [[nodiscard]] constexpr Lock* release() noexcept {
            auto* released = lock_;
            lock_          = nullptr;
            owns_lock_     = false;
            return released;
        }

        constexpr void swap(unique_lock& other) noexcept {
            using std::swap;
            swap(lock_, other.lock_);
            swap(owns_lock_, other.owns_lock_);
        }

    private:
        void require_lockable() const noexcept {
            if (lock_ == nullptr) {
                tay::panic("unique_lock has no associated lock");
            }
            if (owns_lock_) {
                tay::panic("unique_lock already owns the lock");
            }
        }

        Lock* lock_     = nullptr;
        bool owns_lock_ = false;
    };

    template <size_t Order, class Guard>
    struct guard_stage {
        static constexpr size_t order = Order;
        using guard_type              = Guard;
    };

    namespace detail {
        template <class... Ts>
        struct type_list {};

        template <class T>
        inline constexpr bool is_guard_stage_v = false;

        template <size_t Order, class Guard>
        inline constexpr bool is_guard_stage_v<guard_stage<Order, Guard>> = true;

        template <class... Stages>
        inline constexpr bool are_guard_stages_v = (is_guard_stage_v<Stages> && ...);

        template <class... Stages>
        struct orders_are_unique : std::true_type {};

        template <class Stage, class... Rest>
        struct orders_are_unique<Stage, Rest...>
            : std::bool_constant<((Stage::order != Rest::order) && ...) &&
                                 orders_are_unique<Rest...>::value> {};

        template <class Head, class List>
        struct prepend;

        template <class Head, class... Tail>
        struct prepend<Head, type_list<Tail...>> {
            using type = type_list<Head, Tail...>;
        };

        template <class Stage, class List>
        struct insert_guard_stage;

        template <class Stage>
        struct insert_guard_stage<Stage, type_list<>> {
            using type = type_list<Stage>;
        };

        template <class Stage, class Head, class... Tail>
        struct insert_guard_stage<Stage, type_list<Head, Tail...>> {
            using type = std::conditional_t<
                (Stage::order < Head::order), type_list<Stage, Head, Tail...>,
                typename prepend<
                    Head, typename insert_guard_stage<Stage, type_list<Tail...>>::type>::type>;
        };

        template <class Sorted, class... Stages>
        struct sort_guard_stages;

        template <class Sorted>
        struct sort_guard_stages<Sorted> {
            using type = Sorted;
        };

        template <class Sorted, class Stage, class... Rest>
        struct sort_guard_stages<Sorted, Stage, Rest...> {
            using inserted = typename insert_guard_stage<Stage, Sorted>::type;
            using type     = typename sort_guard_stages<inserted, Rest...>::type;
        };

        template <class... Stages>
        using sort_guard_stages_t = typename sort_guard_stages<type_list<>, Stages...>::type;

        template <bool Valid, class... Stages>
        struct context_stage_info {
            static constexpr bool orders_are_unique = true;
            using sorted_stages                     = type_list<>;
        };

        template <class... Stages>
        struct context_stage_info<true, Stages...> {
            static constexpr bool orders_are_unique = detail::orders_are_unique<Stages...>::value;
            using sorted_stages                     = sort_guard_stages_t<Stages...>;
        };

        template <class... Stages>
        class guard_chain;

        template <>
        class guard_chain<> {
        public:
            constexpr guard_chain() noexcept = default;
        };

        template <class Stage, class... Rest>
        class guard_chain<Stage, Rest...> {
            using guard_type = typename Stage::guard_type;

        public:
            constexpr guard_chain() noexcept(
                std::is_nothrow_default_constructible_v<guard_type> &&
                std::is_nothrow_default_constructible_v<guard_chain<Rest...>>)
                : current_{}, rest_{} {}

            guard_chain(const guard_chain&)            = delete;
            guard_chain& operator=(const guard_chain&) = delete;
            guard_chain(guard_chain&&)                 = delete;
            guard_chain& operator=(guard_chain&&)      = delete;

        private:
            [[no_unique_address]] guard_type current_;
            [[no_unique_address]] guard_chain<Rest...> rest_;
        };

        template <class List>
        struct make_guard_chain;

        template <class... Stages>
        struct make_guard_chain<type_list<Stages...>> {
            using type = guard_chain<Stages...>;
        };
    }  // namespace detail

    template <class Lock, class... Stages>
    class context_lock_guard {
        static constexpr bool valid_stages = detail::are_guard_stages_v<Stages...>;
        using stage_info                   = detail::context_stage_info<valid_stages, Stages...>;

        static_assert(valid_stages, "context_lock_guard requires guard_stage<Order, Guard>");
        static_assert(stage_info::orders_are_unique, "context guard orders must be unique");

        using guard_chain_type =
            typename detail::make_guard_chain<typename stage_info::sorted_stages>::type;

    public:
        explicit context_lock_guard(Lock& lock) noexcept : guards_{}, lock_{lock} {}

        context_lock_guard(const context_lock_guard&)            = delete;
        context_lock_guard& operator=(const context_lock_guard&) = delete;
        context_lock_guard(context_lock_guard&&)                 = delete;
        context_lock_guard& operator=(context_lock_guard&&)      = delete;

    private:
        [[no_unique_address]] guard_chain_type guards_;
        lock_guard<Lock> lock_;
    };

    template <class T, class Lock, template <class> class Locker>
    class locked_ref;

    template <class T, class Lock = spinlock, template <class> class Locker = lock_guard>
    class synchronized;

    template <class T, class Lock, template <class> class Locker>
    class locked_ref {
        template <class, class, template <class> class>
        friend class synchronized;

    public:
        locked_ref(const locked_ref&)            = delete;
        locked_ref& operator=(const locked_ref&) = delete;
        locked_ref(locked_ref&&)                 = delete;
        locked_ref& operator=(locked_ref&&)      = delete;

        [[nodiscard]] constexpr T* get() const noexcept {
            return value_;
        }

        [[nodiscard]] constexpr T& operator*() const noexcept {
            return *value_;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept {
            return value_;
        }

    private:
        constexpr locked_ref(Lock& lock, T* value) noexcept : locker_(lock), value_(value) {}

        [[no_unique_address]] Locker<Lock> locker_;
        T* value_;
    };

    template <class T, class Lock, template <class> class Locker>
    class synchronized {
    public:
        template <class... Args>
            requires std::constructible_from<T, Args&&...>
        explicit constexpr synchronized(Args&&... args) noexcept(
            std::is_nothrow_constructible_v<T, Args&&...>)
            : lock_{}, value_(std::forward<Args>(args)...) {}

        synchronized(const synchronized&)            = delete;
        synchronized& operator=(const synchronized&) = delete;
        synchronized(synchronized&&)                 = delete;
        synchronized& operator=(synchronized&&)      = delete;

        [[nodiscard]] locked_ref<T, Lock, Locker> lock() noexcept {
            return locked_ref<T, Lock, Locker>{lock_, &value_};
        }

        [[nodiscard]] locked_ref<const T, Lock, Locker> lock() const noexcept {
            return locked_ref<const T, Lock, Locker>{lock_, &value_};
        }

    private:
        mutable Lock lock_;
        T value_;
    };
}  // namespace tay
