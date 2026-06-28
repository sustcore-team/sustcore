/**
 * @file fchownat.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief fchownat POSIX wrapper
 * @version alpha-1.0.0
 * @date 2026-06-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <syscall.h>

extern "C" int fchownat(int dirfd, const char *pathname,
                        uint32_t owner, uint32_t group, int flags) {
    SysRet<void> ret = sys_vfs_fchownat(static_cast<CapIdx>(dirfd),
                                        owner, group,
                                        static_cast<uint32_t>(flags),
                                        pathname);
    if (ret.is_error()) {
        return -1;
    }
    return 0;
}
