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

#include <cap/capdef.h>
#include <cap/permission.h>
#include <kio.h>
#include <sus/list.h>
#include <sus/optional.h>
#include <sus/rtti.h>
#include <sus/types.h>
#include <sustcore/cap_type.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

    bool _accepted;

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
          _permissions(std::move(permissions)),
          _accepted(false) {
        assert(payload != nullptr);
        assert(owner != nullptr);
        _payload->retain();
    }
    virtual ~Capability() {
        // 将自身从继承树中剥离

        // 遍历继承树, 通知其派生能力销毁自身(通过suicide)
        // TODO: 待树的数据结构实现后, 对Capability的继承树进行管理

        // 释放负载对象
        if (_payload != nullptr) {
            _payload->release();
        }
    }

    // you shouldn't copy or move any capability directly
    Capability(const Capability &)            = delete;
    Capability &operator=(const Capability &) = delete;
    Capability(Capability &&)                 = delete;
    Capability &operator=(Capability &&)      = delete;

    void suicide(void) {
        // 反向调用CapHolder相关方法, 通知其销毁自身
        if (_accepted && _owner != nullptr) {
            // doing sth...
        }
    }
    void accept(void) {
        // 说明该能力已被其owner接受, 在suicide时应当通知其owner主动销毁自身
        _accepted = true;
    }
    void discard(void) {
        // 说明该能力已被其owner抛弃, 在suicide时不应当通知其owner销毁自身
        _accepted = false;
    }
    constexpr CapType type(void) const noexcept {
        return Payload::IDENTIFIER;
    }
    constexpr CapHolder *owner(void) const noexcept {
        return _owner;
    }
    constexpr CapIdx idx(void) const noexcept {
        return _idx;
    }

    friend class BasicCalls<Payload>;

protected:
};

class CSpaceCalls;
class CSpaceBase : public RTTIBase<CSpaceBase, CapType> {
protected:
    int _ref_count;
    virtual void on_zero_ref() = 0;
    CapHolder *_holder;
    const size_t _index;

public:
    static constexpr size_t SPACE_SIZE  = 64;
    static constexpr size_t SPACE_COUNT = 1;
    using CCALL                         = CSpaceCalls;
    // constructors & destructors
    constexpr explicit CSpaceBase(CapHolder *holder, size_t index)
        : _ref_count(0), _holder(holder), _index(index){};
    virtual ~CSpaceBase() {}

    static constexpr CapType PAYLOAD_IDENTIFIER = CapType::CAP_SPACE;

    // reference counting
    void retain(void);
    void release(void);

    constexpr int ref_count(void) const noexcept {
        return _ref_count;
    }

    constexpr size_t index(void) const noexcept {
        return _index;
    }

    constexpr CapHolder *holder(void) noexcept {
        return _holder;
    }

    friend class CSpaceCalls;
};
static_assert(PayloadTrait<CSpaceBase>);

template <PayloadTrait Payload>
class CSpace : public CSpaceBase {
public:
    static constexpr size_t SpaceSize  = Payload::SPACE_SIZE;
    static constexpr size_t SpaceCount = Payload::SPACE_COUNT;
    using Cap                          = Capability<Payload>;
    using Universe                     = CUniverse<Payload>;

protected:
    Cap *_slots[SpaceSize];
    Universe *_universe;

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
        assert(slot_idx < SpaceSize);
        assert(_index + slot_idx != 0);  // 0号能力位被保留, 不允许访问
        _slots[slot_idx] = cap;
    }

    // 将能力对象从能力空间中移除
    void __remove(size_t slot_idx) {
        assert(slot_idx < SpaceSize);
        assert(_index + slot_idx != 0);  // 0号能力位被保留, 不允许访问
        _slots[slot_idx] = nullptr;
    }

    void __clear(void) {
        for (int i = 0; i < SpaceSize; i++) {
            if (_slots[i] != nullptr) {
                delete _slots[i];
                _slots[i] = nullptr;
            }
        }
    }

    // constructor & destructor, it should only be called by Universe
    CSpace(CapHolder *holder, Universe *universe, size_t index)
        : CSpaceBase(holder, index), _universe(universe) {
        memset(_slots, 0, sizeof(_slots));
    }
    ~CSpace() {
        __clear();
    }

public:
    // identifiers
    static constexpr CapType IDENTIFIER = Payload::PAYLOAD_IDENTIFIER;
    virtual CapType type_id() const override {
        return IDENTIFIER;
    }

    // you shouldn't copy or move any CSpace
    CSpace(const CSpace &)            = delete;
    CSpace &operator=(const CSpace &) = delete;
    CSpace(CSpace &&)                 = delete;
    CSpace &operator=(CSpace &&)      = delete;

    // wrap functions
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
    CapOptional<Cap *> lookup(size_t slot_idx) {
        // 0号能力位被保留, 不允许访问
        if (_index + slot_idx == 0)
            return CapErrCode::INVALID_INDEX;
        if (slot_idx >= SpaceSize)
            return CapErrCode::INVALID_INDEX;
        return wrap(_slots[slot_idx]);
    }

    CapOptional<const Cap *> lookup(size_t slot_idx) const {
        // 0号能力位被保留, 不允许访问
        if (_index + slot_idx == 0)
            return CapErrCode::INVALID_INDEX;
        if (slot_idx >= SpaceSize)
            return CapErrCode::INVALID_INDEX;
        return wrap((const Cap *)_slots[slot_idx]);
    }

    friend class CUniverse<Payload>;
    friend class CSpaceCalls;
};

template <PayloadTrait Payload>
class CUniverse {
public:
    static constexpr size_t SpaceSize  = Payload::SPACE_SIZE;
    static constexpr size_t SpaceCount = Payload::SPACE_COUNT;
    using Space                        = CSpace<Payload>;
    using Cap                          = Space::Cap;

protected:
    /**
     * @brief CSpace对象集
     * CUniverse中, 并不是一开始就创建了所有的CSpace对象,
     * 而是采用了按需创建的方式
     * 当某个通过CUniverse提供的space()方法作为Capability对象的Payload被访问时
     * 才会通过__new_space方法创建对应的CSpace对象
     * 并在CUniverse析构时销毁所有已创建的CSpace对象
     * CUniverse对CSpace拥有绝对所有权
     */
    Space *_spaces[SpaceCount];
    CapHolder *_holder;

    void __new_space(size_t space_idx) {
        assert(space_idx < SpaceCount);
        if (_spaces[space_idx] == nullptr) {
            _spaces[space_idx] = new Space(_holder, this, space_idx);
            _spaces[space_idx]->retain();
        }
    }

    void on_zero_ref(Space *sp) {
        // CSpace的声明周期与CUniverse绑定而非与CSpaceCap绑定
        // 因此只在CUniverse被销毁时再销毁CSpace
        // 该函数只是为了说明引用计数达到零时的处理方式,
        // 实际上我们并不需要在此处销毁CSpace
        CAPABILITY::DEBUG("空间%u的引用计数归零", sp->index());
    }

public:
    CUniverse() {
        memset(_spaces, 0, sizeof(_spaces));
    }
    ~CUniverse() {
        for (size_t i = 0; i < SpaceCount; i++) {
            if (_spaces[i] != nullptr) {
                _spaces[i]->release();
                if (_spaces[i]->ref_count() > 0) {
                    CAPABILITY::WARN(
                        "空间%u在销毁时仍存在外部引用(ref_count=%d), 可能存在资源泄漏", i, _spaces[i]->ref_count());
                }
                delete _spaces[i];
            }
        }
    }

    void set_holder(CapHolder *holder) {
        _holder = holder;
    }

    // you shouldn't copy or move any CUniverse
    CUniverse(const CUniverse &)            = delete;
    CUniverse &operator=(const CUniverse &) = delete;
    CUniverse(CUniverse &&)                 = delete;
    CUniverse &operator=(CUniverse &&)      = delete;

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
        if (space_idx >= SpaceCount)
            return CapErrCode::INVALID_INDEX;
        return wrap(_spaces[space_idx]);
    }

    CapOptional<const Space *> lookup_space(size_t space_idx) const {
        if (space_idx >= SpaceCount)
            return CapErrCode::INVALID_INDEX;
        return wrap((const Space *)_spaces[space_idx]);
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

    CapOptional<Space *> space(size_t space_idx) {
        if (space_idx >= SpaceCount)
            return CapErrCode::INVALID_INDEX;
        if (_spaces[space_idx] == nullptr) {
            __new_space(space_idx);
        }
        // 不需要wrap, 因为__new_space保证了_spaces[space_idx]不为nullptr
        return _spaces[space_idx];
    }

    friend class CSpace<Payload>;
};

template <typename... Payloads>
class _CapHolder {
public:
    template <PayloadTrait Payload>
    using Universe = CUniverse<Payload>;
    template <PayloadTrait Payload>
    using Space = Universe<Payload>::Space;

    template <PayloadTrait Payload>
    struct MigrateToken {
        CapIdx src_idx;
        _CapHolder *dst_holder;
    };

protected:
    std::tuple<Universe<Payloads>...> _universes;
    std::tuple<util::ArrayList<MigrateToken<Payloads>>...> _migrate_tokens;

    // 在migrate_tokens中寻找空位
    // 如果找到了一个空位, 则返回该空位的索引; 否则返回-1
    template <PayloadTrait Payload>
    size_t migrate_alloc(void) {
        util::ArrayList<MigrateToken<Payload>> &tokens =
            std::get<util::ArrayList<MigrateToken<Payload>>>(_migrate_tokens);
        size_t idx = INT32_MAX;
        for (size_t i = 0; i < tokens.size(); i++) {
            if (tokens[i].src_idx.nullable()) {
                idx = i;
                break;
            }
        }
        return idx;
    }

    // 将token放入migrate_tokens中, 返回其索引
    template <PayloadTrait Payload>
    size_t migrate_put(MigrateToken<Payload> token) {
        util::ArrayList<MigrateToken<Payload>> &tokens =
            std::get<util::ArrayList<MigrateToken<Payload>>>(_migrate_tokens);
        size_t idx = migrate_alloc<Payload>();
        if (idx == INT32_MAX) {
            tokens.push_back(token);
            return tokens.size() - 1;
        }
        tokens[idx] = token;
        return idx;
    }

public:
    _CapHolder() : _universes() {
        // for each the universes and set the holder
        // by folding expressions
        (universe<Payloads>().set_holder(this), ...);
    }
    ~_CapHolder() {}

    // you shouldn't copy or move any Cap Holder
    _CapHolder(const _CapHolder &)            = delete;
    _CapHolder &operator=(const _CapHolder &) = delete;
    _CapHolder(_CapHolder &&other)            = delete;
    _CapHolder &operator=(_CapHolder &&)      = delete;

    template <PayloadTrait Payload>
    Universe<Payload> &universe(void) {
        return std::get<Universe<Payload>>(_universes);
    }

    template <PayloadTrait Payload>
    const Universe<Payload> &universe(void) const {
        return std::get<Universe<Payload>>(_universes);
    }

    template <PayloadTrait Payload>
    CapOptional<Space<Payload> *> lookup_space(size_t space_idx) {
        return universe<Payload>().lookup_space(space_idx);
    }

    template <PayloadTrait Payload>
    CapOptional<const Space<Payload> *> lookup_space(size_t space_idx) const {
        return universe<Payload>().lookup_space(space_idx);
    }

    template <PayloadTrait Payload>
    CapOptional<Space<Payload> *> space(size_t space_idx) {
        return universe<Payload>().space(space_idx);
    }

    template <PayloadTrait Payload>
    CapOptional<const Space<Payload> *> space(size_t space_idx) const {
        return universe<Payload>().space(space_idx);
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

    template <PayloadTrait Payload>
    MigrateToken<Payload> fetch_migrate_token(int token_idx) {
        util::ArrayList<MigrateToken<Payload>> &tokens =
            std::get<util::ArrayList<MigrateToken<Payload>>>(_migrate_tokens);
        if (token_idx < 0 || token_idx >= (int)tokens.size()) {
            return MigrateToken<Payload>{CapIdx(0), nullptr};
        }
        MigrateToken<Payload> tok = tokens[token_idx];
        tokens[token_idx]         = {CapIdx(0), nullptr};  // clear the token
        return tok;
    }

    template <PayloadTrait Payload>
    MigrateToken<Payload> peer_migrate_token(int token_idx) {
        util::ArrayList<MigrateToken<Payload>> &tokens =
            std::get<util::ArrayList<MigrateToken<Payload>>>(_migrate_tokens);
        if (token_idx < 0 || token_idx >= (int)tokens.size()) {
            return MigrateToken<Payload>{CapIdx(0), nullptr};
        }
        MigrateToken<Payload> tok = tokens[token_idx];
        return tok;
    }

    /**
     * @brief 创建能力转移令牌, 由Capability调用,
     * 以生成一个能力转移令牌供其owner进行能力转移操作
     *
     * @tparam Payload 能力负载类型
     * @param src_idx 能力索引
     * @param dst_holder 目标holder
     * @return size_t 能力转移令牌的索引
     */
    template <PayloadTrait Payload>
    size_t create_migrate_token(CapIdx src_idx, _CapHolder *dst_holder) {
        MigrateToken<Payload> token{src_idx, dst_holder};
        return migrate_put<Payload>(token);
    }
};