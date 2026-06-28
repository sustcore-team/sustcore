/**
 * @file attr.h
 * @author jeromeyao
 * @brief 属性结构与属性掩码
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

struct AttrSet {
    uint32_t mode;     // 文件类型 + 权限位 (S_IFREG | 0755)
    uint32_t uid;      // owner user id (ext4 disk has 16-bit, expand to 32)
    uint32_t gid;      // owner group id
    uint64_t size;     // 文件大小
    uint64_t inode;    // inode 编号
    uint32_t nlink;    // 硬链接数
    uint64_t atime;    // access time (Unix epoch seconds)
    uint64_t mtime;    // modification time (Unix epoch seconds)
    uint64_t ctime;    // change time (Unix epoch seconds)
    uint32_t blksize;  // 块大小（默认 512）
    uint64_t blocks;   // 块数
};

enum class AttrMask : uint32_t {
    MODE  = 1 << 0,
    UID   = 1 << 1,
    GID   = 1 << 2,
    SIZE  = 1 << 3,
    ATIME = 1 << 4,
    MTIME = 1 << 5,
    CTIME = 1 << 6,
};

constexpr AttrMask operator|(AttrMask a, AttrMask b) {
    return static_cast<AttrMask>(static_cast<uint32_t>(a) |
                                 static_cast<uint32_t>(b));
}

constexpr bool operator&(AttrMask a, AttrMask b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

namespace attr {
    constexpr AttrMask OWNER = AttrMask::UID | AttrMask::GID;
    constexpr AttrMask TIMES = AttrMask::ATIME | AttrMask::MTIME;
}  // namespace attr
