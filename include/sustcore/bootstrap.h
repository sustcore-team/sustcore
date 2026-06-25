/**
 * @file bootstrap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动缓冲区中使用的 bootstrap 记录结构
 * @version alpha-1.0.0
 * @date 2026-06-05
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

struct bsheader {
    uint32_t size;
    uint32_t type;
};

struct BootstrapRecordView {
    const bsheader *header;
    const void *data;
    size_t data_size;
};

struct BootstrapCapExplainPayloadHead {
    CapIdx cap_idx;
    PayloadType cap_type;
    b64 cap_perm;
};

struct BootstrapCapExplainView {
    CapIdx cap_idx;
    PayloadType cap_type;
    b64 cap_perm;
    const char *cap_desc;
};

struct BootstrapVaddrExplainPayloadHead {
    addr_t vaddr;
};

struct BootstrapVaddrExplainView {
    VirAddr vaddr;
    const char *vaddr_desc;
};

struct BootstrapPathExplainView {
    const char *path_desc;
};

namespace boot {
    constexpr uint32_t TYPE_CAPEXP   = 0x1;
    constexpr uint32_t TYPE_VADDREXP = 0x2;
    constexpr uint32_t TYPE_PATHEXP  = 0x3;
}  // namespace boot

constexpr uint32_t BOOTSTRAP_USER_TYPE_PREFIX = 0xFFFF0000U;

template <uint32_t Type>
struct BootstrapSingleCapRecord {
    bsheader header;
    CapIdx cap;

    explicit constexpr BootstrapSingleCapRecord(CapIdx value) noexcept
        : header{.size = sizeof(BootstrapSingleCapRecord<Type>), .type = Type},
          cap(value) {}
};

[[nodiscard]]
inline constexpr bool bootstrap_is_user_type(uint32_t type) noexcept {
    return (type & BOOTSTRAP_USER_TYPE_PREFIX) == BOOTSTRAP_USER_TYPE_PREFIX;
}

[[nodiscard]]
inline bool bootstrap_make_view(const bsheader *record,
                                BootstrapRecordView &view) noexcept {
    if (record == nullptr || record->size < sizeof(bsheader)) {
        return false;
    }
    view.header    = record;
    view.data      = reinterpret_cast<const char *>(record) + sizeof(bsheader);
    view.data_size = record->size - sizeof(bsheader);
    return true;
}

template <typename Callback>
[[nodiscard]]
inline bool bootstrap_foreach_record(const bsheader *const *bsargv,
                                     size_t bsargc, Callback &&callback) {
    if (bsargc != 0 && bsargv == nullptr) {
        return false;
    }
    for (size_t i = 0; i < bsargc; ++i) {
        BootstrapRecordView view{};
        if (!bootstrap_make_view(bsargv[i], view)) {
            return false;
        }
        callback(view);
    }
    return true;
}

[[nodiscard]]
inline bool bootstrap_parse_single_cap(const BootstrapRecordView &view,
                                       CapIdx &cap) {
    if (view.data_size != sizeof(CapIdx) || view.data == nullptr) {
        return false;
    }
    memcpy(&cap, view.data, sizeof(cap));
    return true;
}

[[nodiscard]]
inline bool bootstrap_parse_cap_explain(const BootstrapRecordView &view,
                                        BootstrapCapExplainView &cap_explain) {
    if (view.data == nullptr ||
        view.data_size < sizeof(BootstrapCapExplainPayloadHead) + 1)
    {
        return false;
    }

    auto *bytes = static_cast<const char *>(view.data);
    auto *head =
        reinterpret_cast<const BootstrapCapExplainPayloadHead *>(bytes);
    cap_explain.cap_idx  = head->cap_idx;
    cap_explain.cap_type = head->cap_type;
    cap_explain.cap_perm = head->cap_perm;
    cap_explain.cap_desc = bytes + sizeof(BootstrapCapExplainPayloadHead);

    for (size_t i = sizeof(BootstrapCapExplainPayloadHead); i < view.data_size;
         ++i)
    {
        if (bytes[i] == '\0') {
            return true;
        }
    }
    return false;
}

[[nodiscard]]
inline bool bootstrap_parse_vaddr_explain(
    const BootstrapRecordView &view, BootstrapVaddrExplainView &vaddr_explain) {
    if (view.data == nullptr ||
        view.data_size < sizeof(BootstrapVaddrExplainPayloadHead) + 1)
    {
        return false;
    }

    auto *bytes = static_cast<const char *>(view.data);
    auto *head =
        reinterpret_cast<const BootstrapVaddrExplainPayloadHead *>(bytes);
    vaddr_explain.vaddr      = VirAddr(head->vaddr);
    vaddr_explain.vaddr_desc = bytes + sizeof(BootstrapVaddrExplainPayloadHead);

    for (size_t i = sizeof(BootstrapVaddrExplainPayloadHead);
         i < view.data_size; ++i)
    {
        if (bytes[i] == '\0') {
            return true;
        }
    }
    return false;
}

[[nodiscard]]
inline bool bootstrap_parse_path_explain(const BootstrapRecordView &view,
                                         BootstrapPathExplainView &path_view) {
    if (view.data == nullptr || view.data_size == 0) {
        return false;
    }

    auto *bytes = static_cast<const char *>(view.data);
    path_view.path_desc = bytes;
    for (size_t i = 0; i < view.data_size; ++i) {
        if (bytes[i] == '\0') {
            return i != 0 && bytes[0] == '#';
        }
    }
    return false;
}

[[nodiscard]]
inline bool bootstrap_find_single_cap(const bsheader *const *bsargv,
                                      size_t bsargc, uint32_t type,
                                      CapIdx &cap) {
    bool found = false;
    bool ok    = bootstrap_foreach_record(
        bsargv, bsargc, [&](const BootstrapRecordView &view) {
            if (found || view.header->type != type) {
                return;
            }
            found = bootstrap_parse_single_cap(view, cap);
        });
    return ok && found;
}
