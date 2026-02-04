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

#include <concepts>
#include <type_traits>
#include "sus/list.h"

class PCB;
class Capability;
class IPermission;

enum class PermType {
    BASIC = 0,
};

template <typename T>
concept DrvdPermTrait = requires {
    std::is_base_of_v<IPermission, T>;
    {
        T::IDENTIFIER
    } -> std::convertible_to<PermType>;
};

class IPermission {
protected:
    PermType _type;

public:
    constexpr IPermission(PermType type) : _type(type) {}
    virtual ~IPermission() {}
    constexpr PermType type(void) const noexcept {
        return _type;
    }
    /**
     * @brief 检查权限
     *
     * 我们保证此时perm是一个nonnull且类型与当前对象相同的权限对象
     *
     * @param perm 权限对象
     * @return true 当前权限允许该操作
     * @return false 当前权限不允许该操作
     */
    virtual bool _check_permission(IPermission *perm) = 0;
    bool check_permission(IPermission *perm) {
        if (perm == nullptr) {
            return false;
        }
        if (perm->_type != this->_type) {
            return false;
        }
        return _check_permission(perm);
    }
    // 我们禁用了RTTI, 但是仍然可以通过这种方法对RTTI进行模拟
    template <typename DrvdPerm>
        requires DrvdPermTrait<DrvdPerm>
    inline bool is() const noexcept {
        return this->type() == DrvdPerm::IDENTIFIER;
    }
    template <typename DrvdPerm>
        requires DrvdPermTrait<DrvdPerm>
    inline static DrvdPerm *cast(IPermission *perm) {
        if (perm->is<DrvdPerm>()) {
            return reinterpret_cast<DrvdPerm *>(perm);
        }
        return nullptr;
    }
    template <typename DrvdPerm>
        requires DrvdPermTrait<DrvdPerm>
    inline static const DrvdPerm *cast(const IPermission *perm) {
        if (perm->is<DrvdPerm>()) {
            return reinterpret_cast<const DrvdPerm *>(perm);
        }
        return nullptr;
    }
    template <typename DrvdPerm>
        requires DrvdPermTrait<DrvdPerm>
    inline DrvdPerm *as() {
        if (is<DrvdPerm>()) {
            return reinterpret_cast<DrvdPerm *>(this);
        }
        return nullptr;
    }
    template <typename DrvdPerm>
        requires DrvdPermTrait<DrvdPerm>
    inline const DrvdPerm *as() const {
        if (is<DrvdPerm>()) {
            return reinterpret_cast<const DrvdPerm *>(this);
        }
        return nullptr;
    }
};

template <PermType T>
class B64Permission : public IPermission {
protected:
    b64 perm_bits;

public:
    constexpr static PermType IDENTIFIER = T;
    constexpr B64Permission(b64 perm_bits = 0) : IPermission(T) {}
    virtual ~B64Permission() = default;
    virtual bool _check_permission(IPermission *perm) override {
        auto bperm = IPermission::cast<B64Permission<T>>(perm);
        return BITS_IMPLIES(this->perm_bits, bperm->perm_bits);
    }
};

class BasicPermission : public B64Permission<PermType::BASIC> {
public:
    constexpr static b64 PERM_UNWRAP  = 0x1;
    constexpr static b64 PERM_DERIVE  = 0x2;
    constexpr static b64 PERM_MIGRATE = 0x4;
};

class CSpaceCapability;
class PCBCapability;
class TCBCapability;
class EndpointCapability;
class NotificationCapability;

union CapIdx {
    struct {
        b32 slot : 32;
        b32 space : 32;
    };
    b64 raw;
};

enum class CapType {
    NONE         = 0,
    CSPACE       = 1,
    PCB          = 2,
    TCB          = 3,
    ENDPOINT     = 4,
    NOTIFICATION = 5,
};

template <typename T>
concept DrvdCapTrait = requires {
    std::is_base_of_v<Capability, T>;
    {
        T::IDENTIFIER
    } -> std::convertible_to<CapType>;
};

class Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::NONE;
protected:
    CapType _type;
    PCB *_owner;
    CapIdx _idx;
    // TODO: Derivation Tree
    // permissions
    util::ArrayList<IPermission *> _permissions;
public:
    Capability(CapType type, PCB *owner, CapIdx idx, BasicPermission perm);
    virtual ~Capability();
    virtual void suicide(void) {
        // 反向调用PCB相关方法, 通知其销毁自身
    }
    constexpr CapType type(void) const noexcept {
        return _type;
    }
    constexpr PCB *owner(void) const noexcept {
        return _owner;
    }
    constexpr CapIdx idx(void) const noexcept {
        return _idx;
    }
    // 我们禁用了RTTI, 但是仍然可以通过这种方法对RTTI进行模拟
    template <typename DrvdCap>
        requires DrvdCapTrait<DrvdCap>
    inline bool is() const noexcept {
        return this->type() == DrvdCap::IDENTIFIER;
    }
    template <typename DrvdCap>
        requires DrvdCapTrait<DrvdCap>
    inline static DrvdCap *cast(Capability *cap) {
        if (cap->is<DrvdCap>()) {
            return reinterpret_cast<DrvdCap *>(cap);
        }
        return nullptr;
    }
    template <typename DrvdCap>
        requires DrvdCapTrait<DrvdCap>
    inline static const DrvdCap *cast(const Capability *cap) {
        if (cap->is<DrvdCap>()) {
            return reinterpret_cast<const DrvdCap *>(cap);
        }
        return nullptr;
    }
    template <typename DrvdCap>
        requires DrvdCapTrait<DrvdCap>
    inline DrvdCap *as() {
        if (is<DrvdCap>()) {
            return reinterpret_cast<DrvdCap *>(this);
        }
        return nullptr;
    }
    template <typename DrvdCap>
        requires DrvdCapTrait<DrvdCap>
    inline const DrvdCap *as() const {
        if (is<DrvdCap>()) {
            return reinterpret_cast<const DrvdCap *>(this);
        }
        return nullptr;
    }
};

class CSpaceCapability : public Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::CSPACE;
};

class PCBCapability : public Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::PCB;
};
static_assert(DrvdCapTrait<PCBCapability>);

class TCBCapability : public Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::TCB;
};
static_assert(DrvdCapTrait<TCBCapability>);

class EndpointCapability : public Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::ENDPOINT;
};
static_assert(DrvdCapTrait<EndpointCapability>);

class NotificationCapability : public Capability {
public:
    constexpr static CapType IDENTIFIER = CapType::NOTIFICATION;
};
static_assert(DrvdCapTrait<NotificationCapability>);