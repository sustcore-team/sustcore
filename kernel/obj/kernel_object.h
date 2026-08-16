/**
 * @file kernel_object.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KernelObject 对象头、基本强引用、pin 和无 RTTI 的对象操作表。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cap/error.h>
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
    class ObjectRef;

    template <typename T>
    concept CapabilityObject = requires {
        {
            T::TYPE
        } -> std::convertible_to<ObjectType>;
    };
    /** @brief 内核对象从可解析到最终回收的单向生命周期状态。 */
    enum class ObjectState : u8_t {
        ALIVE,
        RETIRING,
        DEAD,
    };

    /** @brief 内核对象在本次启动期间使用的唯一标识。 */
    struct ObjectId {
        u64_t value = 0;
    };

    struct ObjectOps;

    /**
     * @brief 所有 Capability 对象共有的非虚对象头。
     *
     * 对象强引用数量由 `KernelObject` 的 CRTP 基类保存；对象头只保存 kernel pin 和
     * 类型擦除销毁入口。pin 不授予对象所有权，只延迟已经进入 `RETIRING` 的对象回收。
     */
    struct ObjectHeader {
        ObjectType type = ObjectType::NONE;
        u16_t flags     = 0;
        ObjectId id{};
        tay::reference_counter<u32_t> kernel_pins{};
        std::atomic<ObjectState> state{ObjectState::ALIVE};
        const ObjectOps *ops = nullptr;
    };

    /**
     * @brief 提供 ObjectRef 强引用和 kernel pin 双层生命周期的对象基类。
     *
     * 每个长期持有者通过 `ObjectRef` 保存一个强引用，Capability slot 只是其中一种持有者。
     * 最后一个强引用消失时对象进入
     * `RETIRING`；已经线性化成功的 kernel pin 可以继续访问对象，最后一个 pin 释放后
     * 才通过 `ObjectOps` 销毁。该基类不使用 RTTI 或虚函数。
     *
     */
    class KernelObject : public tay::ref_counted<KernelObject, u32_t> {
    public:
        [[nodiscard]] ObjectType object_type() const noexcept {
            return header_.type;
        }

        [[nodiscard]] ObjectId object_id() const noexcept {
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

        [[nodiscard]] u32_t object_refs() const noexcept {
            return ref_count();
        }

        [[nodiscard]] u32_t kernel_pins() const noexcept {
            return header_.kernel_pins.count();
        }

        [[nodiscard]] ObjectState state() const noexcept {
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
        KernelObject(ObjectType type, const ObjectOps *ops) noexcept;
        ~KernelObject() = default;

    private:
        template <typename T>
        friend class ObjectRef;

        using tay::ref_counted<KernelObject, u32_t>::retain;
        using tay::ref_counted<KernelObject, u32_t>::release;
        using tay::ref_counted<KernelObject, u32_t>::try_retain;

        void try_retire() noexcept;

        ObjectHeader header_{};
    };

    /** @brief KernelObject 的静态类型擦除操作表。 */
    struct ObjectOps {
        void (*destroy)(KernelObject &) noexcept = nullptr;
    };

    /**
     * @brief 为具体对象类型生成唯一的静态销毁操作表。
     * @tparam Derived 由 `new` 创建并直接派生自 KernelObject 层级的具体对象类型。
     */
    template <typename Derived>
    struct ObjectOpsHolder final {
        static_assert(std::is_base_of_v<KernelObject, Derived>);
        inline static const ObjectOps OPS{.destroy = [](KernelObject &object) noexcept {
            delete static_cast<Derived *>(&object);
        }};
    };

    /**
     * @brief 返回具体对象类型共享的静态操作表。
     * @tparam Derived 具体对象类型。
     */
    template <typename Derived>
    [[nodiscard]] const ObjectOps *object_ops() noexcept {
        return &ObjectOpsHolder<Derived>::OPS;
    }

    /**
     * @brief 将具体类型及其 ABI `ObjectType` 绑定到 KernelObject。
     * @tparam Derived 最终对象类型，用于选择正确的销毁函数。
     * @tparam Type 对象写入 Capability 的稳定 ABI 类型。
     */
    template <typename Derived, ObjectType Type>
    class TypedKernelObject : public KernelObject {
    protected:
        TypedKernelObject() noexcept : KernelObject(Type, object_ops<Derived>()) {}
    };

    /**
     * @brief 保存一次 resolve 的对象 pin、权限和 badge 快照。
     *
     * 对象 pin 保证 Capability 删除后已完成的 resolve 仍可访问对象，但不保证对应 token
     * 继续有效。该类型不可复制；移动会转移唯一的 unpin 责任。
     */
    class CapPin final {
    public:
        CapPin() = delete;

        /**
         * @brief 接管一个成功取得的 Tay pin。
         * @param pin 要转移的对象 pin。
         * @param rights resolve 线性化时的权限快照。
         * @param badge resolve 线性化时的 badge 快照。
         * @pre `pin` 必须非空。
         */
        CapPin(tay::pin_guard<KernelObject> &&pin, u64_t rights, u64_t badge) noexcept
            : pin_(std::move(pin)), rights_(rights), badge_(badge) {}
        CapPin(const CapPin &)                = delete;
        CapPin &operator=(const CapPin &)     = delete;
        CapPin(CapPin &&) noexcept            = default;
        CapPin &operator=(CapPin &&) noexcept = default;
        ~CapPin() noexcept                    = default;

        [[nodiscard]] KernelObject *get() const noexcept {
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
        tay::pin_guard<KernelObject> pin_{};
        u64_t rights_ = 0;
        u64_t badge_  = 0;
    };

    /**
     * @brief 不携带 authority 的 KernelObject 长期强引用。
     *
     * ObjectRef 只延长对象生命周期，不能替代 Capability 的类型、权限和 badge 校验。
     * 它用于 Capability slot、ProcessManager、Scheduler 和内核对象之间的稳定引用。
     * 类型只保存一个对象指针；复制取得新引用，移动转移引用，析构自动释放引用。
     */
    template <typename T>
    class ObjectRef final {
    public:
        constexpr ObjectRef() noexcept = default;

        explicit ObjectRef(T &object) noexcept : object_(&object) {
            object_->retain();
        }

        ObjectRef(const ObjectRef &other) noexcept : object_(other.object_) {
            if (object_ != nullptr)
                object_->retain();
        }

        ObjectRef &operator=(const ObjectRef &other) noexcept {
            if (this == &other)
                return *this;
            reset_to(other.object_);
            return *this;
        }

        constexpr ObjectRef(ObjectRef &&other) noexcept : object_(other.release()) {}

        ObjectRef &operator=(ObjectRef &&other) noexcept {
            if (this != &other) {
                clear();
                object_ = other.release();
            }
            return *this;
        }

        ~ObjectRef() noexcept {
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

    static_assert(sizeof(ObjectRef<KernelObject>) == sizeof(void (*)()));
    static_assert(alignof(ObjectRef<KernelObject>) == alignof(void (*)()));

    /**
     * @brief 内核对象之间传递的长期授权引用。
     *
     * CapabilityRef 在 ObjectRef 的生命周期保证之上保存权限和 badge 快照。它只应由
     * 已完成 Capability 校验的内核路径构造，不能替代用户可见的 CapToken resolve。
     */
    template <CapabilityObject T>
    class CapabilityRef final {
    public:
        constexpr CapabilityRef() noexcept = default;

        CapabilityRef(const ObjectRef<T> &object, u64_t rights, u64_t badge = 0) noexcept
            : object_(object), rights_(rights), badge_(badge) {}

        CapabilityRef(T &object, u64_t rights, u64_t badge = 0) noexcept
            : object_(object), rights_(rights), badge_(badge) {}

        CapabilityRef(const CapabilityRef &) noexcept            = default;
        CapabilityRef &operator=(const CapabilityRef &) noexcept = default;
        CapabilityRef(CapabilityRef &&) noexcept                 = default;
        CapabilityRef &operator=(CapabilityRef &&) noexcept      = default;

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
        ObjectRef<T> object_{};
        u64_t rights_ = 0;
        u64_t badge_  = 0;
    };
}  // namespace cap
