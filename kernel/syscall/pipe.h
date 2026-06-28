/**
 * @file pipe.h
 * @brief Pipe syscalls
 */

#pragma once

#include <sustcore/capability.h>
#include <syscall/uaccess.h>

#include <cstddef>

namespace syscall {
    struct PipeCreateRet {
        CapIdx read_cap;
        CapIdx write_cap;
    };

    [[nodiscard]]
    Result<void> pipe_create(bool nonblock, UBuffer &&out_buf);

    [[nodiscard]]
    Result<size_t> pipe_read(CapIdx read_cap, UBuffer &&buf, size_t len);

    [[nodiscard]]
    Result<size_t> pipe_write(CapIdx write_cap, UBuffer &&buf, size_t len);
}  // namespace syscall
