/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/cap_type.h>

#include <concepts>
#include <tuple>
#include <utility>

#include "sus/optional.h"

enum class CapErrCode {
    SUCCESS                  = 0,
    INVALID_CAPABILITY       = -1,
    INVALID_INDEX            = -2,
    INSUFFICIENT_PERMISSIONS = -3,
    TYPE_NOT_MATCHED         = -4,
    PAYLOAD_ERROR            = -5,
    UNKNOWN_ERROR            = -255,
};

template <typename T>
using CapOptional = util::Optional<T, CapErrCode, CapErrCode::SUCCESS,
                                   CapErrCode::UNKNOWN_ERROR>;

struct PermissionBits {
    // 基础权限位, 绝大多数能力只需要这64个位
    // 我们规定, 低16位保留给基础能力使用, 剩余48位供派生能力使用
    b64 basic_permissions;
    // 扩展权限位图
    // 例如 FILE 能力, NOTIFICATION 能力,
    // CSPACE能力指向的内核对象实质上包含了一系列对象
    // 对这些对象进行更细粒度的权限控制时, 就需要使用权限位图
    b64 *permission_bitmap;

    enum class Type { NONE = 0, BASIC = 1 };
    const Type type;

    constexpr static size_t to_bitmap_size(Type type) noexcept {
        switch (type) {
            case Type::NONE:
            case Type::BASIC:
            default:          return 0;
        }
    }

    constexpr PermissionBits(b64 basic, b64 *bitmap, Type type)
        : basic_permissions(basic), permission_bitmap(bitmap), type(type) {}
    constexpr PermissionBits(b64 basic, Type type)
        : basic_permissions(basic), permission_bitmap(nullptr), type(type) {
        // assert (to_bitmap_size(type) == 0);
    }
    constexpr ~PermissionBits() {
        if (permission_bitmap != nullptr) {
            delete[] permission_bitmap;
            permission_bitmap = nullptr;
        }
    }

    constexpr PermissionBits(PermissionBits &&other) noexcept
        : permission_bitmap(other.permission_bitmap), type(other.type) {
        other.permission_bitmap = nullptr;
    }
    constexpr PermissionBits &operator=(PermissionBits &&other) noexcept {
        // assert (this.type == other.type);
        if (this != &other) {
            permission_bitmap       = other.permission_bitmap;
            other.permission_bitmap = nullptr;
        }
        return *this;
    }

    PermissionBits(const PermissionBits &other)            = delete;
    PermissionBits &operator=(const PermissionBits &other) = delete;

    constexpr bool imply(const PermissionBits &other) const noexcept {
        if (this->type != other.type) {
            return false;
        }
        // 首先比较 basic_permissions
        if (BITS_IMPLIES(this->basic_permissions, other.basic_permissions) ==
            false)
        {
            return false;
        }
        // 之后再比较 permission_bitmap
        size_t bitmap_size      = to_bitmap_size(this->type);
        bool permission_implied = true;
        /**
         * 为什么采用逐b64比较并将结果进行累加,
         * 而不是在判定到某一位不满足时直接返回false? 在这里, 我的考量是,
         * 权限位图并不会特别巨大(不超过4096字节), 且绝大多数情况下,
         * 权限校验都是成功的. 因此, 提前返回无法节省多少时间,
         * 甚至可能因为分支预测失败而带来额外的开销(虽然概率不大) 而且,
         * 我选择相信编译器.
         * 编译器极有可能会使用SIMD指令来优化这个循环(尽管RISC-V下好像没有,
         * 但似乎存在类似的向量指令集) 同时也能够避免分支预测失败带来的性能损失.
         * 综上所述, 我认为这种实现方式在大多数情况下性能更优.
         * 也许我是错的? 但我确实认为这种方法是效率更加的选择.
         * 也许需要做一个测试, 但还是等到后面说吧.
         * TODO: 进行性能测试, 比较提前返回与否的性能差异
         */
        for (size_t i = 0; i < bitmap_size; i++) {
            permission_implied &= BITS_IMPLIES(this->permission_bitmap[i],
                                               other.permission_bitmap[i]);
        }
        return permission_implied;
    }

    constexpr bool imply(b64 basic_permission) const noexcept {
        return BITS_IMPLIES(this->basic_permissions, basic_permission);
    }
};

template <size_t SPACE_SIZE, size_t SPACE_COUNT, typename... Payloads>
class __CapHolder;

constexpr size_t CAP_SPACE_SIZE  = 1024;
constexpr size_t CAP_SPACE_COUNT = 1024;

template <typename... Payloads>
using _CapHolder = __CapHolder<CAP_SPACE_SIZE, CAP_SPACE_COUNT>;

class PCB;

// 新的需要管理的内核对象应该追加到此处
using CapHolder = _CapHolder<PCB /*, other kernel objects*/>;

template <typename T>
concept HasReferenceCount = requires(T *t) {
    {
        t->retain()
    } -> std::same_as<void>;
    {
        t->release()
    } -> std::same_as<void>;
    {
        t->ref_count()
    } -> std::convertible_to<int>;
};

template <typename T>
concept PayloadTrait = HasReferenceCount<T> && requires {
    {
        T::IDENTIFIER
    } -> std::convertible_to<CapType>;
};

template <PayloadTrait Payload>
class Capability {
protected:
    CapHolder *_owner;
    CapIdx _idx;
    // TODO: Derivation Tree

    // Payload
    Payload *_payload;
    // permissions
    PermissionBits _permissions;

public:
    // permissions
    static constexpr b64 PERMISSION_UNWRAP = 0x1;
    static constexpr b64 PERMISSION_DERIVE = 0x2;

    /**
     * @brief 构造能力对象
     *
     * @param owner 能力持有者
     * @param idx 能力索引
     * @param payload 负载对象(不应为nullptr)
     * @param permissions 权限位图
     */
    Capability(CapHolder *owner, CapIdx idx, Payload *payload,
               PermissionBits &&permissions)
        : _owner(owner),
          _idx(idx),
          _payload(payload),
          _permissions(std::move(permissions)) {
        // assert (payload != nullptr);
        _payload->retain();
    }
    ~Capability() {
        // 将自身从继承树中剥离

        // 遍历继承树, 通知其派生能力销毁自身

        // 释放负载对象
        _payload->release();
    }
    virtual void suicide(void) {
        // 反向调用PCB相关方法, 通知其销毁自身
    }
    constexpr CapType type(void) const noexcept {
        return Payload::CapType;
    }
    constexpr CapHolder *owner(void) const noexcept {
        return _owner;
    }
    constexpr CapIdx idx(void) const noexcept {
        return _idx;
    }

    CapOptional<Payload *> payload(void) const noexcept {
        // UNWRAP 权限校验
        if (!_permissions.imply(PERMISSION_UNWRAP)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }
        return _payload;
    }
};

class CapSpaceBase {
    virtual CapType type() = 0;

    template <typename T>
    bool is(void) {
        return type() == T::IDENTIFIER;
    }

    template <typename T>
    static T *cast(CapSpaceBase *base) {
        if (base->is<T>()) {
            return reinterpret_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
    static const T *cast(CapSpaceBase *base) {
        if (base->is<T>()) {
            return reinterpret_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
    T *as() {
        if (is<T>()) {
            return reinterpret_cast<T *>(this);
        }
        return nullptr;
    }

    template <typename T>
    const T *as() const {
        if (is<T>()) {
            return reinterpret_cast<T *>(this);
        }
        return nullptr;
    }
};

template <PayloadTrait Payload, size_t SPACE_SIZE>
class CapSpace : public CapSpaceBase {
public:
    using Cap = Capability<Payload>;

protected:
    Cap *_slots[SPACE_SIZE];

public:
    static constexpr CapType IDENTIFIER = Payload::IDENTIFIER;
    virtual CapType type() override {
        return IDENTIFIER;
    }
    CapSpace() {}
    ~CapSpace() {
        for (int i = 0; i < SPACE_SIZE; i++) {
            if (_slots[i] != nullptr) {
                delete _slots[i];
            }
        }
    }
    CapOptional<const Cap *> lookup(size_t slot) const {
        if (0 > slot || slot >= SPACE_SIZE)
            return CapErrCode::INVALID_INDEX;
        return _slots[slot];
    }

    CapOptional<Cap *> lookup(size_t slot) {
        if (0 > slot || slot >= SPACE_SIZE)
            return CapErrCode::INVALID_INDEX;
        return _slots[slot];
    }
};

template <PayloadTrait Payload, size_t SPACE_SIZE, size_t SPACE_COUNT>
class CapUniverse {
public:
    using Space = CapSpace<Payload, SPACE_SIZE>;
    using Cap  = Space::Cap;

protected:
    Space *_spaces[SPACE_COUNT];

public:
    CapUniverse() {}
    ~CapUniverse() {
        for (int i = 0; i < SPACE_COUNT; i++) {
            if (_spaces[i] != nullptr) {
                delete _spaces[i];
            }
        }
    }

    CapOptional<Space *> lookup_space(size_t space) {
        if (0 > space || space >= SPACE_COUNT)
            return CapErrCode::INVALID_INDEX;
        return _spaces[space];
    }

    CapOptional<const Space *> lookup_space(size_t space) const {
        if (0 > space || space >= SPACE_COUNT)
            return CapErrCode::INVALID_INDEX;
        return _spaces[space];
    }

    CapOptional<Cap *> lookup(size_t space, size_t slot) {
        CapOptional<Space *> cap_space = lookup_space(space);
        return cap_space.map([slot](Space *sp) { return sp->lookup(slot); });
    }

    CapOptional<const Cap *> lookup(size_t space, size_t slot) const {
        CapOptional<const Space *> cap_space = lookup_space(space);
        return cap_space.map([slot](const Space *sp) { return sp->lookup(slot); });
    }

    CapOptional<Cap *> lookup(CapIdx idx) {
        return lookup(idx.space, idx.slot);
    }

    CapOptional<const Cap *> lookup(CapIdx idx) const {
        return lookup(idx.space, idx.slot);
    }
};

template <size_t SPACE_SIZE, size_t SPACE_COUNT, typename... Payloads>
class __CapHolder {
protected:
    template <typename Payload>
    using Universe = CapUniverse<Payload, SPACE_SIZE, SPACE_COUNT>;

    std::tuple<Universe<Payloads>...> _universes;

public:
    __CapHolder() {}
    ~__CapHolder() {}
    template <typename Payload>
    Universe<Payload> &universe(void) {
        return std::get<Universe<Payload>>(_universes);
    }

    template <typename Payload>
    const Universe<Payload> &universe(void) const {
        return std::get<Universe<Payload>>(_universes);
    }

    template <typename Payload>
    CapOptional<Capability<Payload> *> lookup(CapIdx idx) {
        Universe<Payload> &univ = universe<Payload>();
        return univ.lookup(idx);
    }

    template <typename Payload>
    CapOptional<const Capability<Payload> *> lookup(CapIdx idx) {
        const Universe<Payload> &univ = universe<Payload>();
        return univ.lookup(idx);
    }
};