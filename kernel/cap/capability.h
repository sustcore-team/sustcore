/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Defintion
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <mem/alloc.h>
#include <perm/permission.h>
#include <sus/nonnull.h>
#include <sus/owner.h>
#include <sus/raii.h>
#include <sus/refcount.h>
#include <sus/rtti.h>
#include <sus/tree.h>
#include <sustcore/capability.h>

#include <new>

// forward declarations

namespace cap {
    class Payload;
    class Capability;
    class CGroup;
    class CHolder;

    // 载荷基类
    // 所有的载荷都必须继承自该基类
    class Payload : public RTTIBase<Payload, PayloadType> {
        // 引用计数, 用于实现共享载荷的自动内存管理
        size_t _refcount = 0;

    public:
        Payload()          = default;
        // virtual destructor
        virtual ~Payload() = default;

        // RTTI机制
        // 说明自己的类型ID
        [[nodiscard]]
        PayloadType type_id() const override = 0;

        virtual void destruct() {
            delete this;
        }

        void keep() {
            _refcount++;
        }

        void release() {
            assert(_refcount > 0);
            _refcount--;
            if (_refcount == 0) {
                destruct();
            }
        }

        [[nodiscard]]
        size_t ref_count() const {
            return _refcount;
        }
    };

    template <PayloadType _IDENTIFIER>
    class _PayloadHelper : public Payload {
    public:
        constexpr static PayloadType IDENTIFIER = _IDENTIFIER;
        [[nodiscard]]
        PayloadType type_id() const override {
            return IDENTIFIER;
        }
    };

    class Capability {
    private:
        // 载荷
        Payload *_payload;
        // 权限
        b64 _perm;

    public:
        Capability(Payload *payload, b64 perm)
            : _payload(payload), _perm(perm) {
            assert(_payload != nullptr);
            _payload->keep();
        }

        Capability(const Capability &other)
            : _payload(other._payload), _perm(other._perm) {
            assert(_payload != nullptr);
            _payload->keep();
        }

        Capability &operator=(const Capability &other) = delete;

        ~Capability() {
            if (_payload != nullptr) {
                _payload->release();
            }
        }

        [[nodiscard]]
        Payload *payload() const {
            return _payload;
        }

        template <typename T>
        [[nodiscard]]
        T *payload_as() const {
            return _payload->as<T>();
        }

        [[nodiscard]]
        b64 perm() const {
            return _perm;
        }

        [[nodiscard]]
        Capability *clone() const {
            return new Capability(*this);
        }

        [[nodiscard]]
        Result<void> downgrade(b64 new_perm) {
            if (!perm::imply(_perm, new_perm)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            _perm = new_perm;
            void_return();
        }

        [[nodiscard]]
        bool imply(b64 required) const noexcept {
            return perm::imply(_perm, required);
        }

        void *operator new(size_t size);
        void operator delete(void *ptr);
    };

    template <typename PayloadType>
    class CapObj {
    protected:
        Capability *_cap;
        PayloadType *_obj;

        CapObj(util::nonnull<Capability *> cap)
            : _cap(cap), _obj(_cap->payload_as<PayloadType>()) {
            assert(_obj != nullptr);
        }

        [[nodiscard]]
        bool implies(b64 perm) const noexcept {
            return _cap->imply(perm);
        }

        [[nodiscard]]
        bool imply(b64 basic_permission) const noexcept {
            return perm::imply(_cap->perm(), basic_permission);
        }

    public:
        [[nodiscard]]
        Capability *cap() const {
            return _cap;
        }

        [[nodiscard]]
        PayloadType *obj() const {
            return _obj;
        }
    };

    class CGroup {
    private:
        Capability *caps[CGROUP_SLOTS];

    public:
        CGroup() {
            memset(caps, 0, sizeof(caps));
        }

        ~CGroup() {
            for (auto &cap : caps) {
                if (cap != nullptr) {
                    delete cap;
                    cap = nullptr;
                }
            }
        }

        [[nodiscard]]
        Capability *get(CapIdx idx) const {
            return caps[cap::slot(idx)];
        }

        Result<void> set(CapIdx idx, Capability *cap) {
            if (caps[cap::slot(idx)] != nullptr) {
                delete caps[cap::slot(idx)];
            }
            caps[cap::slot(idx)] = cap;
            void_return();
        }

        [[nodiscard]]
        Capability *take(CapIdx idx) {
            Capability *cap      = caps[cap::slot(idx)];
            caps[cap::slot(idx)] = nullptr;
            return cap;
        }

        Result<void> remove(CapIdx idx) {
            return set(idx, nullptr);
        }

        void *operator new(size_t size);
        void operator delete(void *ptr);
    };

    // CGroup 是一组 Capability 的集合, CSpace 是一组 CGroup 的集合
    // 通过双层索引来管理 Capability
    // 在这种设计中, CSpace最多可以容纳 CSPACE_SIZE * CGROUP_SLOTS 个 Capability
    // 但是不需要一开始就分配出 CSPACE_SIZE * CGROUP_SLOTS 个指针来管理这些
    // Capability
    class CSpace {
    private:
        CGroup *groups[CSPACE_SIZE];

    public:
        CSpace() {
            memset(groups, 0, sizeof(groups));
        }

        ~CSpace() {
            for (auto &group : groups) {
                if (group != nullptr) {
                    delete group;
                    group = nullptr;
                }
            }
        }

        [[nodiscard]]
        Capability *get(CapIdx idx) const {
            CGroup *grp = groups[cap::group(idx)];
            if (grp == nullptr) {
                return nullptr;
            }
            return grp->get(idx);
        }

        Result<void> set(CapIdx idx, Capability *cap) {
            if (groups[cap::group(idx)] == nullptr) {
                groups[cap::group(idx)] = new CGroup();
            }
            return groups[cap::group(idx)]->set(idx, cap);
        }

        Result<void> remove(CapIdx idx) {
            return set(idx, nullptr);
        }

        [[nodiscard]]
        Capability *take(CapIdx idx) {
            CGroup *grp = groups[cap::group(idx)];
            if (grp == nullptr) {
                return nullptr;
            }
            return grp->take(idx);
        }

        Result<void> move(CapIdx target_idx, CapIdx src_idx) {
            if (target_idx == src_idx) {
                void_return();
            }

            Capability *cap = get(src_idx);
            if (cap == nullptr) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto set_res = set(target_idx, cap);
            propagate(set_res);

            Capability *removed_cap = take(src_idx);
            assert(removed_cap == cap);
            void_return();
        }

        [[nodiscard]]
        Result<CapIdx> lookup_freeslot() const {
            for (size_t i = 0; i < CSPACE_SIZE; i++) {
                if (groups[i] == nullptr) {
                    // 该组还未分配, 整组都是空闲的
                    return cap::make(i, 0);
                } else {
                    // 该组已经分配, 需要检查组内的每个槽位
                    for (size_t j = 0; j < CGROUP_SLOTS; j++) {
                        if (groups[i]->get(cap::make(i, j)) == nullptr) {
                            // 该槽位空闲
                            return cap::make(i, j);
                        }
                    }
                }
            }
            unexpect_return(ErrCode::NO_FREE_SLOT);
        }

        template <typename _Fp>
        void foreach(_Fp f) const {
            for (size_t i = 0; i < CSPACE_SIZE; i++) {
                if (groups[i] == nullptr) {
                    continue;
                }
                for (size_t j = 0; j < CGROUP_SLOTS; j++) {
                    CapIdx idx      = cap::make(i, j);
                    Capability *cap = groups[i]->get(idx);
                    if (cap != nullptr) {
                        f(idx, cap);
                    }
                }
            }
        }

        void clear() {
            for (auto &group : groups) {
                if (group != nullptr) {
                    delete group;
                    group = nullptr;
                }
            }
        }
    };

    void init_kop();
}  // namespace cap
