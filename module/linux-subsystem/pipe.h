/**
 * @file pipe.h
 * @brief Linux pipe compatibility helpers
 */

#pragma once

#include <cstddef>

#include <sustcore/capability.h>

size_t linux_sys_pipe2(int *pipefd, int flags);
bool linux_cap_is_pipe_read_end(CapIdx cap);
bool linux_cap_is_pipe_write_end(CapIdx cap);
size_t linux_pipe_read(CapIdx cap, void *buf, size_t count);
size_t linux_pipe_write(CapIdx cap, const void *buf, size_t len);
