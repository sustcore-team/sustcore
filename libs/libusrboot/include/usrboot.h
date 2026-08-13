/**
 * @file usrboot.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义内核与 mk-usrboot 共享的 usrboot 文件格式 ABI。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief usrboot 格式使用的固定宽度无符号整数。 */
typedef uint64_t ub_u64;

/** @brief usrboot 文件中保存的 64-bit 用户虚拟地址。 */
typedef uint64_t ub_addr64;

/** @brief 相对于整个 usrboot 文件起点的 64-bit 字节偏移。 */
typedef uint64_t ub_off64;

/** @brief usrboot 文件和内存区域使用的 64-bit 字节长度。 */
typedef uint64_t ub_sz64;

/** @brief little-endian 文件中表示 ASCII `USRBOOT_` 的固定魔数。 */
#define USRBOOT_MAGIC 0x5f544f4f42525355ULL

/**
 * @brief 描述一个由内核装载到用户地址空间的文件段。
 *
 * 文件内容位于 `[off, off + filesz)`；内核将其复制到 `vaddr`，并将
 * `[vaddr + filesz, vaddr + memsz)` 清零。`off` 相对于整个 usrboot 文件起点，
 * 不是相对于 Header 后的 Body 起点。
 */
struct usrboot_segment {
    /** @brief 段在用户地址空间中的起始虚拟地址。 */
    ub_addr64 vaddr;

    /** @brief 段文件内容相对于整个 usrboot 文件起点的字节偏移。 */
    ub_off64 off;

    /** @brief usrboot 文件中实际保存的段内容长度。 */
    ub_sz64 filesz;

    /** @brief 段装载后占用的内存长度，必须不小于 `filesz`。 */
    ub_sz64 memsz;
};

/**
 * @brief usrboot 文件的固定 120-byte Header。
 *
 * Header 后紧跟 Body。Body 按 RX、RW、RO 顺序保存三个段的文件内容；各段仍通过
 * 文件绝对偏移 `usrboot_segment::off` 定位。所有整数均使用 64-bit little-endian 编码。
 */
struct usrboot_header {
    /** @brief 必须等于 `USRBOOT_MAGIC` 的格式魔数。 */
    ub_u64 magic;

    /** @brief Header 之后 Body 的总字节数。 */
    ub_sz64 body_size;

    /** @brief 内核首次进入 usrboot 时使用的用户态入口地址。 */
    ub_addr64 entry;

    /** @brief read-execute 段描述。 */
    struct usrboot_segment seg_rx;

    /** @brief read-write 段描述。 */
    struct usrboot_segment seg_rw;

    /** @brief read-only 段描述。 */
    struct usrboot_segment seg_ro;
};

/** @cond USRBOOT_INTERNAL */
#if defined(__cplusplus)
#define USRBOOT_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define USRBOOT_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

USRBOOT_STATIC_ASSERT(sizeof(ub_u64) == 8, "usrboot integers must be 64-bit");
USRBOOT_STATIC_ASSERT(sizeof(struct usrboot_segment) == 32, "usrboot segment layout changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, vaddr) == 0,
                      "usrboot segment vaddr offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, off) == 8,
                      "usrboot segment off offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, filesz) == 16,
                      "usrboot segment filesz offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, memsz) == 24,
                      "usrboot segment memsz offset changed");

USRBOOT_STATIC_ASSERT(sizeof(struct usrboot_header) == 120, "usrboot header layout changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, magic) == 0,
                      "usrboot header magic offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, body_size) == 8,
                      "usrboot header body_size offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, entry) == 16,
                      "usrboot header entry offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_rx) == 24,
                      "usrboot header RX offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_rw) == 56,
                      "usrboot header RW offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_ro) == 88,
                      "usrboot header RO offset changed");

#undef USRBOOT_STATIC_ASSERT
/** @endcond */
