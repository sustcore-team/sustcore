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

        void keep() {
            _refcount++;
        }

        void release() {
            assert(_refcount > 0);
            _refcount--;
            if (_refcount == 0) {
                delete this;
            }
        }
    };

    template <PayloadType _IDENTIFIER>
    class _PayloadHelper : public Payload {
    public:
        constexpr static PayloadType IDENTIFIER = IDENTIFIER;
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
        PermissionBits _perm;

    public:
        Capability(Payload *payload, PermissionBits &&perm)
            : _payload(payload), _perm(std::move(perm)) {
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

        [[nodiscard]]
        PermissionBits perm() const {
            return _perm;
        }

        [[nodiscard]]
        Capability *clone() const {
            return new Capability(*this);
        }

        [[nodiscard]]
        Result<void> downgrade(const PermissionBits &new_perm) {
            return _perm.downgrade(new_perm);
        }

        [[nodiscard]]
        bool imply(const PermissionBits &other) const noexcept {
            return _perm.imply(other);
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
            return caps[slot(idx)];
        }

        Result<void> set(CapIdx idx, Capability *cap) {
            if (caps[slot(idx)] != nullptr) {
                delete caps[slot(idx)];
            }
            caps[slot(idx)] = cap;
            void_return();
        }

        Result<void> remove(CapIdx idx) {
            return set(idx, nullptr);
        }
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
            CGroup *grp = groups[group(idx)];
            if (grp == nullptr) {
                return nullptr;
            }
            return grp->get(idx);
        }

        Result<void> set(CapIdx idx, Capability *cap) {
            if (groups[group(idx)] == nullptr) {
                groups[group(idx)] = new CGroup();
            }
            return groups[group(idx)]->set(idx, cap);
        }

        Result<void> remove(CapIdx idx) {
            return set(idx, nullptr);
        }

        [[nodiscard]]
        Result<CapIdx> lookup_freeslot() const {
            for (size_t i = 0; i < CSPACE_SIZE; i++) {
                if (groups[i] == nullptr) {
                    // 该组还未分配, 整组都是空闲的
                    return make(i, 0);
                } else {
                    // 该组已经分配, 需要检查组内的每个槽位
                    for (size_t j = 0; j < CGROUP_SLOTS; j++) {
                        if (groups[i]->get(make(i, j)) == nullptr) {
                            // 该槽位空闲
                            return make(i, j);
                        }
                    }
                }
            }
            unexpect_return(ErrCode::NO_FREE_SLOT);
        }
    };
}  // namespace cap