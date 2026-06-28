/**
 * @file pipe.cpp
 * @brief Anonymous pipe capability objects
 */

#include <logger.h>
#include <object/pipe.h>
#include <spinlock.h>
#include <sustcore/errcode.h>
#include <task/wait.h>

#include <algorithm>
#include <cassert>
#include <cstring>

namespace cap {
    namespace {
        [[nodiscard]]
        size_t pipe_storage_capacity(size_t usable_capacity) noexcept {
            return usable_capacity + 1;
        }

        [[nodiscard]]
        Result<void> wake_all_ignoring_empty(wait::wd_t wd,
                                             SpinLocker &lock) {
            auto wake_res = wait::locked_wake_all(wd, lock);
            if (!wake_res.has_value()) {
                propagate_return(wake_res);
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> close_read_end(PipeReadEndPayload *end) {
            if (end == nullptr || !end->pipe) {
                void_return();
            }

            PipePayload *pipe = end->pipe.get();
            bool should_wake = false;
            {
                GuardedLock guard(pipe->lock);
                if (end->closed) {
                    void_return();
                }
                end->closed = true;
                assert(pipe->read_ends > 0);
                --pipe->read_ends;
                should_wake = true;
            }
            if (should_wake) {
                auto wake_res = wake_all_ignoring_empty(pipe->writable_wd,
                                                        pipe->lock);
                propagate(wake_res);
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> close_write_end(PipeWriteEndPayload *end) {
            if (end == nullptr || !end->pipe) {
                void_return();
            }

            PipePayload *pipe = end->pipe.get();
            bool should_wake = false;
            {
                GuardedLock guard(pipe->lock);
                if (end->closed) {
                    void_return();
                }
                end->closed = true;
                assert(pipe->write_ends > 0);
                --pipe->write_ends;
                should_wake = true;
            }
            if (should_wake) {
                auto wake_res = wake_all_ignoring_empty(pipe->readable_wd,
                                                        pipe->lock);
                propagate(wake_res);
            }
            void_return();
        }
    }  // namespace

    PipePayload::PipePayload(size_t usable_capacity)
        : buffer(pipe_storage_capacity(usable_capacity)),
          capacity(usable_capacity),
          read_ends(0),
          write_ends(0),
          lock(),
          readable_wd(wait::alloc_reason()),
          writable_wd(wait::alloc_reason()) {}

    void PipePayload::on_death() {
        delete this;
    }

    bool PipePayload::readable_ready() const noexcept {
        return !buffer.empty() || write_ends == 0;
    }

    bool PipePayload::writable_ready() const noexcept {
        return !buffer.full() || read_ends == 0;
    }

    PipeReadEndPayload::PipeReadEndPayload(PipePayload *pipe, bool nonblock)
        : pipe(pipe), nonblock(nonblock), closed(false) {
        assert(pipe != nullptr);
        GuardedLock guard(pipe->lock);
        ++pipe->read_ends;
    }

    PipeReadEndPayload::~PipeReadEndPayload() {
        auto close_res = close_read_end(this);
        if (!close_res.has_value()) {
            loggers::CAPABILITY::ERROR("pipe read end close failed: err=%s",
                                       to_cstring(close_res.error()));
        }
    }

    void PipeReadEndPayload::destruct() {
        delete this;
    }

    Payload *PipeReadEndPayload::clone_payload() {
        return this;
    }

    PipeWriteEndPayload::PipeWriteEndPayload(PipePayload *pipe, bool nonblock)
        : pipe(pipe), nonblock(nonblock), closed(false) {
        assert(pipe != nullptr);
        GuardedLock guard(pipe->lock);
        ++pipe->write_ends;
    }

    PipeWriteEndPayload::~PipeWriteEndPayload() {
        auto close_res = close_write_end(this);
        if (!close_res.has_value()) {
            loggers::CAPABILITY::ERROR("pipe write end close failed: err=%s",
                                       to_cstring(close_res.error()));
        }
    }

    void PipeWriteEndPayload::destruct() {
        delete this;
    }

    Payload *PipeWriteEndPayload::clone_payload() {
        return this;
    }

    Result<size_t> PipeReadEndObject::read(void *buf, size_t len) {
        if (!imply(perm::pipe::READ)) {
            loggers::CAPABILITY::ERROR("Pipe READ权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (buf == nullptr && len != 0) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (len == 0) {
            return size_t(0);
        }
        if (!_obj->pipe || _obj->closed) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *out  = static_cast<byte *>(buf);
        auto *pipe = _obj->pipe.get();
        while (true) {
            size_t done = 0;
            {
                GuardedLock guard(pipe->lock);
                if (!pipe->buffer.empty()) {
                    while (done < len && !pipe->buffer.empty()) {
                        out[done++] = pipe->buffer.front();
                        auto pop_res = pipe->buffer.pop();
                        if (!pop_res.has_value()) {
                            unexpect_return(ErrCode::IO_ERROR);
                        }
                    }
                } else if (pipe->write_ends == 0) {
                    return size_t(0);
                } else if (_obj->nonblock) {
                    unexpect_return(ErrCode::WOULD_BLOCK);
                }
            }

            if (done != 0) {
                auto wake_res = wake_all_ignoring_empty(pipe->writable_wd,
                                                        pipe->lock);
                propagate(wake_res);
                return done;
            }

            auto wait_res = locked_wait_event(
                pipe->readable_wd, pipe->lock, pipe->readable_ready());
            propagate(wait_res);
        }
    }

    Result<size_t> PipeWriteEndObject::write(const void *buf, size_t len) {
        if (!imply(perm::pipe::WRITE)) {
            loggers::CAPABILITY::ERROR("Pipe WRITE权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (buf == nullptr && len != 0) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (len == 0) {
            return size_t(0);
        }
        if (!_obj->pipe || _obj->closed) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const auto *in = static_cast<const byte *>(buf);
        auto *pipe     = _obj->pipe.get();
        size_t written = 0;
        while (written < len) {
            bool made_progress = false;
            {
                GuardedLock guard(pipe->lock);
                if (pipe->read_ends == 0) {
                    unexpect_return(ErrCode::BROKEN_PIPE);
                }
                if (!pipe->buffer.full()) {
                    while (written < len && !pipe->buffer.full()) {
                        auto push_res = pipe->buffer.push(in[written]);
                        if (!push_res.has_value()) {
                            unexpect_return(ErrCode::IO_ERROR);
                        }
                        ++written;
                        made_progress = true;
                    }
                } else if (_obj->nonblock) {
                    if (written != 0) {
                        return written;
                    }
                    unexpect_return(ErrCode::WOULD_BLOCK);
                }
            }

            if (made_progress) {
                auto wake_res = wake_all_ignoring_empty(pipe->readable_wd,
                                                        pipe->lock);
                propagate(wake_res);
                return written;
            }

            auto wait_res = locked_wait_event(
                pipe->writable_wd, pipe->lock, pipe->writable_ready());
            propagate(wait_res);
        }
        return written;
    }
}  // namespace cap
