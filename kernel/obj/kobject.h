/**
 * @file kobject.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KObject 对象头、基本强引用、pin 和无 RTTI 的对象操作表。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error/cap.h>
#include <sustcore/capability.h>
#include <tay/counter.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/refcount.h>

#include <atomic>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace cap {
    template <typename T>
    class KObjectRef;

    template <typename T>
    concept CTarget = requires {
        {
            T::TYPE
        } -> std::convertible_to<ObjectType>;
    };
    /** @brief 内核对象从可解析到最终回收的单向生命周期状态。 */
    enum class KObjectState : u8_t {
        ALIVE,
        RETIRING,
        DEAD,
    };

    /** @brief 内核对象在本次启动期间使用的唯一标识。 */
    struct KObjectId {
        u64_t value = 0;
    };

    struct KObjectOps;

    /**
     * @brief 所有 Capability 对象共有的非虚对象头。
     *
     * 对象强引用数量由 `KObject` 的 CRTP 基类保存；对象头只保存 kernel pin 和
     * 类型擦除销毁入口。pin 不授予对象所有权，只延迟已经进入 `RETIRING` 的对象回收。
     */
    struct KObjectHeader {
        ObjectType type = ObjectType::NONE;
        u16_t flags     = 0;
        KObjectId id{};
        tay::reference_counter<u32_t> pins{};
        std::atomic<KObjectState> state{KObjectState::ALIVE};
        const KObjectOps *ops = nullptr;
    };

    /**
     * @brief 提供 KObjectRef 强引用和 kernel pin 双层生命周期的对象基类。
     *
     * 每个长期持有者通过 `KObjectRef` 保存一个强引用，Capability slot 只是其中一种持有者。
     * 最后一个强引用消失时对象进入
     * `RETIRING`；已经线性化成功的 kernel pin 可以继续访问对象，最后一个 pin 释放后
     * 才通过 `KObjectOps` 销毁。该基类不使用 RTTI 或虚函数。
     *
     */
    class KObject : public tay::ref_counted<KObject, u32_t> {
    public:
        [[nodiscard]] ObjectType type() const noexcept {
            return header_.type;
        }

        [[nodiscard]] KObjectId object_id() const noexcept {
            return header_.id;
        }

        /**
         * @brief 尝试取得一次短期 kernel pin。
         * @return 对象仍处于 `ALIVE` 且 pin 已建立时返回 true。
         * @pre 调用方在本函数返回前持有对象强引用，或持有保护该引用的 CNode 锁。
         */
        [[nodiscard]] bool try_pin() noexcept;

        /**
         * @brief 释放调用方持有的一次 kernel pin。
         * @pre 调用方持有一个尚未释放的 pin。
         */
        void unpin() noexcept;

        [[nodiscard]] u32_t strong_refs() const noexcept {
            return ref_count();
        }

        [[nodiscard]] u32_t pins() const noexcept {
            return header_.pins.count();
        }

        [[nodiscard]] KObjectState state() const noexcept {
            return header_.state.load(std::memory_order_acquire);
        }

        /** @brief 接收 CRTP 基类的最后引用通知并启动延迟回收。 */
        void on_zero_references() noexcept;

    protected:
        /**
         * @brief 构造具有稳定类型和销毁操作表的对象头。
         * @param type 对象的 Capability ABI 类型。
         * @param ops 静态生命周期内有效的销毁操作表。
         */
        KObject(ObjectType type, const KObjectOps *ops) noexcept;
        ~KObject() = default;

    private:
        template <typename T>
        friend class KObjectRef;

        using tay::ref_counted<KObject, u32_t>::retain;
        using tay::ref_counted<KObject, u32_t>::release;
        using tay::ref_counted<KObject, u32_t>::try_retain;

        void try_retire() noexcept;

        KObjectHeader header_{};
    };

    /** @brief KObject 的静态类型擦除操作表。 */
    struct KObjectOps {
        void (*destroy)(KObject &) noexcept = nullptr;
    };

    /**
     * @brief 为具体对象类型生成唯一的静态销毁操作表。
     * @tparam Derived 由 `new` 创建并直接派生自 KObject 层级的具体对象类型。
     */
    template <typename Derived>
    struct KObjectOpsFor final {
        static_assert(std::is_base_of_v<KObject, Derived>);
        inline static const KObjectOps OPS{
            .destroy = [](KObject &object) noexcept { delete static_cast<Derived *>(&object); }};
    };

    /**
     * @brief 返回具体对象类型共享的静态操作表。
     * @tparam Derived 具体对象类型。
     */
    template <typename Derived>
    [[nodiscard]] const KObjectOps *kobject_ops() noexcept {
        return &KObjectOpsFor<Derived>::OPS;
    }

    /**
     * @brief 将具体类型及其 ABI `ObjectType` 绑定到 KObject。
     * @tparam Derived 最终对象类型，用于选择正确的销毁函数。
     * @tparam Type 对象写入 Capability 的稳定 ABI 类型。
     */
    template <typename Derived, ObjectType Type>
    class TypedKObject : public KObject {
    protected:
        TypedKObject() noexcept : KObject(Type, kobject_ops<Derived>()) {}
    };

    /**
     * @brief 保存一次 resolve 的对象 pin、权限和 badge 快照。
     *
     * 对象 pin 保证 Capability 删除后已完成的 resolve 仍可访问对象，但不保证对应 token
     * 继续有效。该类型不可复制；移动会转移唯一的 unpin 责任。
     */
    class CPin final {
    public:
        CPin() = delete;

        /**
         * @brief 接管一个成功取得的 Tay pin。
         * @param pin 要转移的对象 pin。
         * @param rights resolve 线性化时的权限快照。
         * @param badge resolve 线性化时的 badge 快照。
         * @pre `pin` 必须非空。
         */
        CPin(tay::pin_guard<KObject> &&pin, u64_t rights, u64_t badge) noexcept
            : pin_(std::move(pin)), rights_(rights), badge_(badge) {}
        CPin(const CPin &)                = delete;
        CPin &operator=(const CPin &)     = delete;
        CPin(CPin &&) noexcept            = default;
        CPin &operator=(CPin &&) noexcept = default;
        ~CPin() noexcept                  = default;

        [[nodiscard]] KObject *get() const noexcept {
            return pin_.get();
        }

        [[nodiscard]] u64_t rights() const noexcept {
            return rights_;
        }

        [[nodiscard]] u64_t badge() const noexcept {
            return badge_;
        }

        /** @brief 提前释放对象 pin；权限和 badge 快照保持不变。 */
        void reset() noexcept {
            pin_.reset();
        }

    private:
        tay::pin_guard<KObject> pin_{};
        u64_t rights_ = 0;
        u64_t badge_  = 0;
    };

    /**
     * @brief 不携带 authority 的 KObject 长期强引用。
     *
     * KObjectRef 只延长对象生命周期，不能替代 Capability 的类型、权限和 badge 校验。
     * 它用于 Capability slot、ProcTable、Scheduler 和内核对象之间的稳定引用。
     * 类型只保存一个对象指针；复制取得新引用，移动转移引用，析构自动释放引用。
     */
    template <typename T>
    class KObjectRef final {
    public:
        constexpr KObjectRef() noexcept = default;

        explicit KObjectRef(T &object) noexcept : object_(&object) {
            object_->retain();
        }

        KObjectRef(const KObjectRef &other) noexcept : object_(other.object_) {
            if (object_ != nullptr)
                object_->retain();
        }

        KObjectRef &operator=(const KObjectRef &other) noexcept {
            if (this == &other)
                return *this;
            reset_to(other.object_);
            return *this;
        }

        constexpr KObjectRef(KObjectRef &&other) noexcept : object_(other.release()) {}

        KObjectRef &operator=(KObjectRef &&other) noexcept {
            if (this != &other) {
                clear();
                object_ = other.release();
            }
            return *this;
        }

        ~KObjectRef() noexcept {
            clear();
        }

        [[nodiscard]] constexpr T *get() const noexcept {
            return object_;
        }

        [[nodiscard]] constexpr T &operator*() const noexcept {
            return *object_;
        }

        [[nodiscard]] constexpr T *operator->() const noexcept {
            return object_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return object_ != nullptr;
        }

        void clear() noexcept {
            auto *object = release();
            if (object != nullptr)
                object->release();
        }

        /** @brief 释放当前长期引用并变为空引用。 */
        void reset() noexcept {
            clear();
        }

    private:
        [[nodiscard]] constexpr T *release() noexcept {
            auto *object = object_;
            object_      = nullptr;
            return object;
        }

        void reset_to(T *object) noexcept {
            if (object == object_)
                return;
            if (object != nullptr)
                object->retain();
            clear();
            object_ = object;
        }

        T *object_ = nullptr;
    };

    static_assert(sizeof(KObjectRef<KObject>) == sizeof(void (*)()));
    static_assert(alignof(KObjectRef<KObject>) == alignof(void (*)()));

    /**
     * @brief 内核对象之间传递的长期授权引用。
     *
     * CRef 在 KObjectRef 的生命周期保证之上保存权限和 badge 快照。它只应由
     * 已完成 Capability 校验的内核路径构造，不能替代用户可见的 CToken resolve。
     */
    template <CTarget T>
    class CRef final {
    public:
        constexpr CRef() noexcept = default;

        CRef(const KObjectRef<T> &object, u64_t rights, u64_t badge = 0) noexcept
            : object_(object), rights_(rights), badge_(badge) {}

        CRef(T &object, u64_t rights, u64_t badge = 0) noexcept
            : object_(object), rights_(rights), badge_(badge) {}

        CRef(const CRef &) noexcept            = default;
        CRef &operator=(const CRef &) noexcept = default;
        CRef(CRef &&) noexcept                 = default;
        CRef &operator=(CRef &&) noexcept      = default;

        [[nodiscard]] T *object() const noexcept {
            return object_.get();
        }

        [[nodiscard]] u64_t rights() const noexcept {
            return rights_;
        }

        [[nodiscard]] u64_t badge() const noexcept {
            return badge_;
        }

        [[nodiscard]] bool allows(u64_t required) const noexcept {
            return object_ && (rights_ & required) == required;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(object_);
        }

        void reset() noexcept {
            object_.reset();
            rights_ = 0;
            badge_  = 0;
        }

    private:
        KObjectRef<T> object_{};
        u64_t rights_ = 0;
        u64_t badge_  = 0;
    };
}  // namespace cap
