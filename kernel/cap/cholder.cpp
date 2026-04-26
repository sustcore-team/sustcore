/**
 * @file cholder.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <env.h>
#include <object/csa.h>
#include <sustcore/capability.h>

namespace key {
    struct cholder : public env::key::chman {};
}  // namespace key

CHolder::CHolder(size_t cholder_id)
    : _space(this), cholder_id(cholder_id) {
    auto ret = _space.create<CSpaceAccessor>(_csa_idx, &_space);
    assert(ret.has_value());
}

CHolder::~CHolder() {}

Result<ReceiveToken> CHolder::send_capability(size_t target_id,
                                              CapIdx cap_idx) {
    // make sure it's a valid capability index
    auto access_res = access(cap_idx);
    propagate(access_res);

    auto chman = env::inst().chman(key::cholder());

    size_t timestamp = chman->timestamp();

    // add a send record to the queue
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    _send_records[_send_record_idx] = SendRecord(
        timestamp, {.sender_id = cholder_id, .cap_idx = cap_idx}, target_id);
    const size_t record_idx = _send_record_idx;
    _send_record_idx        = (_send_record_idx + 1) % SENDRECORDS;

    return ReceiveToken{.sender_id  = cholder_id,
                        .record_idx = record_idx,
                        .timestamp  = timestamp};
}

Result<CapIdx> CHolder::try_receive(size_t receiver_id, ReceiveToken token) {
    // 测试token本身是否合法
    if (token.sender_id != cholder_id) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }
    if (token.record_idx >= SENDRECORDS) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }

    // 从发送记录中获取对应的记录
    const SendRecord &record = _send_records[token.record_idx];

    // 测试记录是否合法
    if (record.timestamp() != token.timestamp) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }
    if (record.sender_id() != token.sender_id) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }
    if (record.target_id() != receiver_id) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }
    // 还需要try access
    auto access_res = access(record.cap_idx());
    if (!access_res.has_value()) {
        unexpect_return(ErrCode::INVALID_TOKEN);
    }

    // token有效, timestamp与sender_id都匹配, 记录中的cap_idx也合法
    CapIdx cap_idx = record.cap_idx();

    // 将记录从发送记录中移除
    SendRecord &_record = _send_records[token.record_idx];
    _record = SendRecord();

    return cap_idx;
}