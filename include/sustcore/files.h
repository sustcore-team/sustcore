/**
 * @file files.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 文件操作相关枚举与宏
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

// used for vfs open function
// read means you're capable to read the file,
// write means you're capable to write the file,
// execute means you're capable to execute the file (if it's executable)
// execute can be combined with read when an executable image must be lazily read.
// write/create cannot be combined with execute.
// you need to open an executable file with execute flag,
// and when calling create_process and execve,
// you need to open the executable file with execute flag at first
// to get a vfile capability, and then pass the capability to create_process or
// execve.
#include <cstddef>
#include <cstdint>

namespace flags {
    // open flags
    using oflg_t               = size_t;
    constexpr oflg_t O_READ    = 0b0001;
    constexpr oflg_t O_WRITE   = 0b0010;
    constexpr oflg_t O_EXECUTE = 0b0100;
    constexpr oflg_t O_CREAT   = 0b1000;

    // posix permission flags
    using ppflg_t                     = size_t;
    constexpr ppflg_t PP_READ_OWNER   = 0b000000001;
    constexpr ppflg_t PP_WRITE_OWNER  = 0b000000010;
    constexpr ppflg_t PP_EXEC_OWNER   = 0b000000100;
    constexpr ppflg_t PP_READ_GROUP   = 0b000001000;
    constexpr ppflg_t PP_WRITE_GROUP  = 0b000010000;
    constexpr ppflg_t PP_EXEC_GROUP   = 0b000100000;
    constexpr ppflg_t PP_READ_OTHERS  = 0b001000000;
    constexpr ppflg_t PP_WRITE_OTHERS = 0b010000000;
    constexpr ppflg_t PP_EXEC_OTHERS  = 0b100000000;

    constexpr ppflg_t PP_RWX_OWNER =
        PP_READ_OWNER | PP_WRITE_OWNER | PP_EXEC_OWNER;
    constexpr ppflg_t PP_RW_OWNER = PP_READ_OWNER | PP_WRITE_OWNER;
};  // namespace flags


struct dir_entry_header {
    size_t next_offset;
};

enum class EntryType : uint64_t {
    FILE    = 0,
    DIR     = 1,
    SYMLINK = 2,
};

enum class MountStatus : uint64_t {
    MOUNTED  = 0,
    UMOUNTED = 1,
    INVALID  = 2,
};

struct NodeMeta {
    EntryType type;
    size_t size;
    size_t inode;
    size_t links;
    size_t devno;
};

struct VFSPageCacheStats {
    size_t hits;
    size_t misses;
    size_t invalidations;
    size_t writebacks;
    size_t evictions;
    size_t cached_pages;
    size_t max_pages;
    size_t backing_reads;
    size_t backing_writes;
};

struct VFSStatFS {
    uint64_t f_type;
    uint64_t f_blocks;
};

static_assert(sizeof(dir_entry_header) == sizeof(size_t),
              "dir_entry_header must match next_offset size");

constexpr size_t DIR_ENTRY_END = 0xFFFFFFFFFFFFFFFFULL;
