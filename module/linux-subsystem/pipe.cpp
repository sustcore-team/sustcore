/**
 * @file pipe.cpp
 * @brief Linux pipe compatibility helpers
 */

#include <errno.h>
#include <fdtable.h>
#include <logger.h>
#include <pipe.h>
#include <syscall.h>

#include <cstring>

namespace {
    constexpr int LINUX_O_NONBLOCK = 00004000;
    constexpr int LINUX_O_CLOEXEC  = 02000000;

    [[nodiscard]]
    size_t pipe_error_to_linux(ErrCode err) noexcept {
        switch (err) {
            case ErrCode::WOULD_BLOCK:              return -EAGAIN;
            case ErrCode::BROKEN_PIPE:              return -EPIPE;
            case ErrCode::INSUFFICIENT_PERMISSIONS: return -EBADF;
            case ErrCode::TYPE_NOT_MATCHED:         return -EBADF;
            case ErrCode::INVALID_PARAM:            return -EBADF;
            case ErrCode::NULLPTR:                  return -EFAULT;
            case ErrCode::OUT_OF_MEMORY:
            case ErrCode::ALLOCATION_FAILED:        return -ENOMEM;
            case ErrCode::NO_FREE_SLOT:             return -EMFILE;
            default:                                return -EIO;
        }
    }

    [[nodiscard]]
    bool cap_type_is(CapIdx cap, PayloadType type) {
        if (cap == cap::null || cap == cap::error) {
            return false;
        }
        CapInfo info{};
        auto info_res = sys_cap_lookup(cap, &info).to_result();
        return info_res.has_value() && info.type == type;
    }
}  // namespace

bool linux_cap_is_pipe_read_end(CapIdx cap) {
    return cap_type_is(cap, PayloadType::PIPE_READ_END);
}

bool linux_cap_is_pipe_write_end(CapIdx cap) {
    return cap_type_is(cap, PayloadType::PIPE_WRITE_END);
}

size_t linux_pipe_read(CapIdx cap, void *buf, size_t count) {
    auto read_res = sys_pipe_read(cap, buf, count).to_result();
    if (!read_res.has_value()) {
        return pipe_error_to_linux(read_res.error());
    }
    return read_res.value();
}

size_t linux_pipe_write(CapIdx cap, const void *buf, size_t len) {
    auto write_res = sys_pipe_write(cap, buf, len).to_result();
    if (!write_res.has_value()) {
        return pipe_error_to_linux(write_res.error());
    }
    return write_res.value();
}

size_t linux_sys_pipe2(int *pipefd, int flags) {
    if (pipefd == nullptr) {
        return -EFAULT;
    }
    if ((flags & LINUX_O_CLOEXEC) != 0) {
        return -EINVAL;
    }
    if ((flags & ~LINUX_O_NONBLOCK) != 0) {
        return -EINVAL;
    }

    PipeCreateRet caps{};
    auto create_res =
        sys_pipe_create((flags & LINUX_O_NONBLOCK) != 0, &caps).to_result();
    if (!create_res.has_value()) {
        return pipe_error_to_linux(create_res.error());
    }

    int read_fd = alloc_fd(caps.read_cap);
    if (read_fd < 0) {
        sys_cap_remove(caps.read_cap);
        sys_cap_remove(caps.write_cap);
        return -EMFILE;
    }
    int write_fd = alloc_fd(caps.write_cap);
    if (write_fd < 0) {
        free_fd(read_fd);
        sys_cap_remove(caps.write_cap);
        return -EMFILE;
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return 0;
}
