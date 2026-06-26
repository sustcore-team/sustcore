/**
 * @file vmount.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief mount capability object
 * @version alpha-1.0.0
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <vfs/vfs.h>

namespace cap {
    class VMountObject : public CapObj<::VMount> {
    public:
        explicit VMountObject(util::nonnull<Capability *> cap)
            : CapObj<::VMount>(cap) {}
        ~VMountObject() = default;

        void *operator new(size_t size) = delete;
        void operator delete(void *ptr) = delete;

        [[nodiscard]]
        Result<bool> mount(VDirectory &parent, const char *mntpath,
                           uint64_t attachflags);
        [[nodiscard]]
        Result<bool> umount(uint64_t flags);
        [[nodiscard]]
        Result<CapIdx> root(CHolder &holder);
        [[nodiscard]]
        Result<MountStatus> state() const;
    };
}  // namespace cap
