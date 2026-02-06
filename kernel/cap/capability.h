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

#include <cap/permission.h>
#include <cap/capdef.h>
#include <kio.h>
#include <sus/optional.h>
#include <sus/types.h>
#include <sustcore/cap_type.h>

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

template <PayloadTrait Payload>
class BasicCalls;

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

    Payload * __payload(void) const noexcept {
        return _payload;
    }
public:
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
    virtual ~Capability() {
        // 将自身从继承树中剥离

        // 遍历继承树, 通知其派生能力销毁自身

        // 释放负载对象
        _payload->release();
    }
    virtual void suicide(void) {
        // 反向调用CapHolder相关方法, 通知其销毁自身
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

    friend class BasicCalls<Payload>;
};

template <typename DrvdSpace>
concept DrvdSpaceTrait = requires() {
    std::is_base_of_v<CSpaceBase, DrvdSpace>;
    {
        DrvdSpace::IDENTIFIER
    } -> std::same_as<const CapType &>;
};

class CSpaceCalls;

class CSpaceBase {
protected:
    int _ref_count;
    virtual void on_zero_ref() = 0;

public:
    // constructors & destructors
    constexpr CSpaceBase() : _ref_count(0){};
    virtual ~CSpaceBase() {}

    static constexpr CapType IDENTIFIER = CapType::CAP_SPACE;

    // reference counting
    void retain(void);
    void release(void);

    constexpr int ref_count(void) const noexcept {
        return _ref_count;
    }

    // 因为 RTTI 被我们禁用
    // 我们通过这种方法模拟 RTTI
    virtual CapType type() = 0;

    template <typename T>
        requires DrvdSpaceTrait<T>
    bool is(void) {
        return type() == T::IDENTIFIER;
    }

    template <typename T>
        requires DrvdSpaceTrait<T>
    static T *cast(CSpaceBase *base) {
        if (base->is<T>()) {
            return reinterpret_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdSpaceTrait<T>
    static const T *cast(CSpaceBase *base) {
        if (base->is<T>()) {
            return reinterpret_cast<T *>(base);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdSpaceTrait<T>
    T *as() {
        if (is<T>()) {
            return reinterpret_cast<T *>(this);
        }
        return nullptr;
    }

    template <typename T>
        requires DrvdSpaceTrait<T>
    const T *as() const {
        if (is<T>()) {
            return reinterpret_cast<T *>(this);
        }
        return nullptr;
    }

    friend class CSpaceCalls;
};

template <PayloadTrait Payload, size_t SPACE_SIZE, size_t SPACE_COUNT>
class __CSpace : public CSpaceBase {
public:
    using Cap      = Capability<Payload>;
    using Universe = __CUniverse<Payload, SPACE_SIZE, SPACE_COUNT>;

protected:
    Cap *_slots[SPACE_SIZE];
    Universe *_universe;
    const size_t _index;

    virtual void on_zero_ref() override {
        _universe->on_zero_ref(this);
    }

    /**
     * @brief 将能力对象插入到能力空间中
     *
     * @param slot_idx 能力位索引
     * @param cap 能力对象指针(不应为nullptr)
     */
    void __insert(size_t slot_idx, Cap *cap) {
        // assert(0 <= slot_idx && slot_idx < SPACE_SIZE);
        // assert (_index + slot_idx != 0);  // 0号能力位被保留, 不允许访问
        _slots[slot_idx] = cap;
    }

    // 将能力对象从能力空间中移除
    void __remove(size_t slot_idx) {
        // assert(0 <= slot_idx && slot_idx < SPACE_SIZE);
        // assert (_index + slot_idx != 0);  // 0号能力位被保留, 不允许访问
        _slots[slot_idx] = nullptr;
    }

    // constructor & destructor, it should only be called by Universe
    __CSpace(Universe *universe, size_t index)
        : CSpaceBase(), _universe(universe), _index(index) {
        memset(_slots, 0, sizeof(_slots));
    }
    ~__CSpace() {
        for (int i = 0; i < SPACE_SIZE; i++) {
            if (_slots[i] != nullptr) {
                delete _slots[i];
            }
        }
    }
public:
    // identifiers
    static constexpr CapType IDENTIFIER = Payload::IDENTIFIER;
    virtual CapType type() override {
        return IDENTIFIER;
    }

    constexpr size_t index(void) const noexcept {
        return _index;
    }

    // you shouldn't move or copy any CSpace
    __CSpace(const __CSpace &)            = delete;
    __CSpace &operator=(const __CSpace &) = delete;
    __CSpace(__CSpace &&)                 = delete;
    __CSpace &operator=(__CSpace &&)      = delete;

    template <typename T>
    static constexpr CapOptional<T *> wrap(T *pointer) {
        if (pointer == nullptr) {
            return CapErrCode::INVALID_INDEX;
        }
        return pointer;
    }

    template <typename T>
    static constexpr CapOptional<const T *> wrap(const T *pointer) {
        if (pointer == nullptr) {
            return CapErrCode::INVALID_INDEX;
        }
        return pointer;
    }

    // lookup
    CapOptional<const Cap *> lookup(size_t slot_idx) const {
        // 0号能力位被保留, 不允许访问
        if (_index + slot_idx == 0)
            return CapErrCode::INVALID_INDEX;
        if (0 > slot_idx || slot_idx >= SPACE_SIZE)
            return CapErrCode::INVALID_INDEX;
        return wrap(_slots[slot_idx]);
    }

    CapOptional<Cap *> lookup(size_t slot_idx) {
        // 0号能力位被保留, 不允许访问
        if (_index + slot_idx == 0)
            return CapErrCode::INVALID_INDEX;
        if (0 > slot_idx || slot_idx >= SPACE_SIZE)
            return CapErrCode::INVALID_INDEX;
        return wrap(_slots[slot_idx]);
    }

    friend class __CUniverse<Payload, SPACE_SIZE, SPACE_COUNT>;
    friend class CSpaceCalls;
};

template <PayloadTrait Payload, size_t SPACE_SIZE, size_t SPACE_COUNT>
class __CUniverse {
public:
    using Space = __CSpace<Payload, SPACE_SIZE, SPACE_COUNT>;
    using Cap   = Space::Cap;

protected:
    Space *_spaces[SPACE_COUNT];

    void __new_space(size_t space_idx) {
        // assert (0 <= space && space < SPACE_COUNT);
        if (_spaces[space_idx] == nullptr) {
            _spaces[space_idx] = new Space(space_idx, this);
            _spaces[space_idx]->retain();
        }
    }

    void on_zero_ref(Space *sp) {
        // do nothing, just keep the space alive
        CAPABILITY::DEBUG("Space at %d ref_count reached zero.", sp->index());
    }

public:
    __CUniverse() {
        memset(_spaces, 0, sizeof(_spaces));
    }
    ~__CUniverse() {
        for (int i = 0; i < SPACE_COUNT; i++) {
            if (_spaces[i] != nullptr) {
                delete _spaces[i];
            }
        }
    }

    template <typename T>
    static constexpr CapOptional<T *> wrap(T *pointer) {
        if (pointer == nullptr) {
            return CapErrCode::INVALID_INDEX;
        }
        return pointer;
    }

    template <typename T>
    static constexpr CapOptional<const T *> wrap(const T *pointer) {
        if (pointer == nullptr) {
            return CapErrCode::INVALID_INDEX;
        }
        return pointer;
    }

    CapOptional<Space *> lookup_space(size_t space_idx) {
        if (0 > space_idx || space_idx >= SPACE_COUNT)
            return CapErrCode::INVALID_INDEX;
        return wrap(_spaces[space_idx]);
    }

    CapOptional<const Space *> lookup_space(size_t space_idx) const {
        if (0 > space_idx || space_idx >= SPACE_COUNT)
            return CapErrCode::INVALID_INDEX;
        return wrap(_spaces[space_idx]);
    }

    CapOptional<Cap *> lookup(size_t space_idx, size_t slot_idx) {
        CapOptional<Space *> space = lookup_space(space_idx);
        return space.and_then_opt(
            [slot_idx](Space *sp) { return sp->lookup(slot_idx); });
    }

    CapOptional<const Cap *> lookup(size_t space_idx, size_t slot_idx) const {
        CapOptional<const Space *> space = lookup_space(space_idx);
        return space.and_then_opt(
            [slot_idx](const Space *sp) { return sp->lookup(slot_idx); });
    }

    CapOptional<Cap *> lookup(CapIdx idx) {
        return lookup(idx.space, idx.slot);
    }

    CapOptional<const Cap *> lookup(CapIdx idx) const {
        return lookup(idx.space, idx.slot);
    }

    friend class __CSpace<Payload, SPACE_SIZE, SPACE_COUNT>;
};

template <size_t SPACE_SIZE, size_t SPACE_COUNT, typename... Payloads>
class __CapHolder {
public:
    template <PayloadTrait Payload>
    using Universe = __CUniverse<Payload, SPACE_SIZE, SPACE_COUNT>;

protected:
    std::tuple<Universe<Payloads>...> _universes;

public:
    __CapHolder() {}
    ~__CapHolder() {}
    template <PayloadTrait Payload>
    Universe<Payload> &universe(void) {
        return std::get<Universe<Payload>>(_universes);
    }

    template <PayloadTrait Payload>
    const Universe<Payload> &universe(void) const {
        return std::get<Universe<Payload>>(_universes);
    }

    template <PayloadTrait Payload>
    CapOptional<Capability<Payload> *> lookup(CapIdx idx) {
        Universe<Payload> &univ = universe<Payload>();
        return univ.lookup(idx);
    }

    template <PayloadTrait Payload>
    CapOptional<const Capability<Payload> *> lookup(CapIdx idx) const {
        const Universe<Payload> &univ = universe<Payload>();
        return univ.lookup(idx);
    }
};

template <PayloadTrait Payload>
using CUniverse = CapHolder::Universe<Payload>;
template <PayloadTrait Payload>
using CSpace = CUniverse<Payload>::Space;