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
#include <sus/map.h>
#include <sus/queue.h>
#include <sustcore/capability.h>

// 能力发送记录
struct SendRecord {
public:
    struct SenderInfo {
        size_t sender_id;
        CapIdx cap_idx;
    };

private:
    size_t _timestamp       = 0;
    SenderInfo _sender_info = {};
    size_t _target_id       = 0;

public:
    constexpr SendRecord() = default;
    constexpr SendRecord(size_t timestamp, SenderInfo sender_info,
                         size_t target_id)
        : _timestamp(timestamp),
          _sender_info(sender_info),
          _target_id(target_id) {}
    [[nodiscard]]
    size_t target_id() const {
        return _target_id;
    }
    [[nodiscard]]
    size_t sender_id() const {
        return _sender_info.sender_id;
    }
    [[nodiscard]]
    CapIdx cap_idx() const {
        return _sender_info.cap_idx;
    }
    [[nodiscard]]
    size_t timestamp() const {
        return _timestamp;
    }
};

// 能力接收凭证
struct ReceiveToken {
    size_t sender_id;
    size_t record_idx;
    size_t timestamp;
};

// 能够持有能力的对象称为能力持有者 (Capability Holder)
// 例如, Process就是一种典型的能力持有者.
// 又如, VFS也是一种能力持有者, 因为它持有着文件系统的能力 (如根目录的能力).
class CHolder {
private:
    CSpace _space;
    CapIdx _csa_idx = capidx::make(0, 1);  // 指向自身CSpace访问器(最大)的能力索引
    size_t cholder_id;
    // send record queue, 用于记录该能力持有者发送过的能力
    constexpr static size_t SENDRECORDS = 64;
    SendRecord _send_records[SENDRECORDS];
    size_t _send_record_idx = 0;
public:
    CHolder(size_t cholder_id);
    ~CHolder();

    [[nodiscard]]
    constexpr size_t id() const {
        return cholder_id;
    }

    [[nodiscard]]
    constexpr const CSpace &space() const {
        return _space;
    }
    [[nodiscard]]
    constexpr CSpace &space() {
        return _space;
    }

    [[nodiscard]]
    Result<Capability *> access(CapIdx idx) {
        if (!capidx::valid(idx)) {
            return {unexpect, ErrCode::TYPE_NOT_MATCHED};
        }

        return _space.get(idx);
    }

    [[nodiscard]]
    Result<Capability *> csa() {
        return access(_csa_idx);
    };

    [[nodiscard]]
    constexpr CapIdx csa_idx() const {
        return _csa_idx;
    }

    [[nodiscard]]
    Result<ReceiveToken> send_capability(size_t target_id, CapIdx cap_idx);
    // 尝试从send record queue中查找记录
    // 如果找到一条合法的记录, 则返回对应的能力索引, 并将其从记录中移除
    // 否则, 返回一个错误
    [[nodiscard]]
    Result<CapIdx> try_receive(size_t receiver_id, ReceiveToken token);
};

class CHolderManager {
private:
    size_t __cholder_id = 0;
    constexpr size_t __id_alloc() {
        return __cholder_id++;
    }
    util::LinkedMap<size_t, CHolder *> _holders;
    size_t _timestamp = 1;  // 用于记录发送记录的时间戳, 每次发送能力时递增
public:
    CHolderManager() = default;

    [[nodiscard]]
    Result<CHolder *> get_holder(size_t id) const {
        return _holders.get(id)
            .transform_error(always(ErrCode::OUT_OF_BOUNDARY))
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

    [[nodiscard]]
    size_t timestamp() {
        return _timestamp++;
    }
};