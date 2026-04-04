/**
 * @file cholder.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <cap/cspace.h>
#include <sus/map.h>

#include <functional>

// 能够持有能力的对象称为能力持有者 (Capability Holder)
// 例如, Process就是一种典型的能力持有者.
// 又如, VFS也是一种能力持有者, 因为它持有着文件系统的能力 (如根目录的能力).
class CHolder {
protected:
    CSpace _space;
    RecvSpace _recv_space;  // 接收空间, 用于接收从其他CHolder迁移过来的能力
    CapIdx _csa_idx;  // 指向自身CSpace访问器(最大)的能力索引
public:
    const size_t cholder_id;
    CHolder(size_t cholder_id);
    ~CHolder();

    constexpr const CSpace &space(void) const {
        return _space;
    }
    constexpr CSpace &space(void) {
        return _space;
    }

    constexpr const RecvSpace &recv_space(void) const {
        return _recv_space;
    }

    constexpr RecvSpace &recv_space(void) {
        return _recv_space;
    }

    Result<Capability *> access(CapIdx idx);

    inline Result<Capability *> csa(void) {
        return access(_csa_idx);
    };

    constexpr CapIdx csa_idx(void) const {
        return _csa_idx;
    }
};

class CHolderManager {
private:
    size_t __cholder_id = 0;
    constexpr size_t __id_alloc() {
        return __cholder_id++;
    }
    util::LinkedMap<size_t, CHolder *> _holders;

public:
    CHolderManager() = default;

    Result<CHolder *> get_holder(size_t id) const {
        return _holders.get(id)
            .transform_error(always(ErrCode::INVALID_INDEX))
            .transform(
                std::mem_fn(&std::reference_wrapper<CHolder *const>::get));
    }

    template <typename... Args>
    Result<CHolder *> create_holder(Args &&...args) {
        size_t id   = __id_alloc();
        auto holder = new CHolder(id, std::forward<Args>(args)...);
        _holders.put(id, holder);
        return holder;
    }
};