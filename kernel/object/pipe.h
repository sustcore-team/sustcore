/**
 * @file pipe.h
 * @brief Anonymous pipe capability objects
 */

#pragma once

#include <cap/capability.h>
#include <object/perm.h>
#include <spinlock.h>
#include <sus/refcount.h>
#include <sus/ringbuf.h>
#include <sustcore/capability.h>
#include <task/wait.h>

#include <cstddef>

namespace cap {
    constexpr size_t PIPE_DEFAULT_CAPACITY = 4096;

    struct PipePayload : public util::refc<PipePayload> {
        util::RingBuffer<byte> buffer;
        size_t capacity;
        size_t read_ends;
        size_t write_ends;
        SpinLocker lock;
        wait::wd_t readable_wd;
        wait::wd_t writable_wd;

        explicit PipePayload(size_t capacity = PIPE_DEFAULT_CAPACITY);

        void on_death();
        [[nodiscard]]
        bool readable_ready() const noexcept;
        [[nodiscard]]
        bool writable_ready() const noexcept;
    };

    struct PipeReadEndPayload
        : public _PayloadHelper<PayloadType::PIPE_READ_END> {
        util::refc_ptr<PipePayload> pipe;
        bool nonblock;
        bool closed;

        explicit PipeReadEndPayload(PipePayload *pipe, bool nonblock);
        ~PipeReadEndPayload() override;
        void destruct() override;

        [[nodiscard]]
        Payload *clone_payload() override;
    };

    struct PipeWriteEndPayload
        : public _PayloadHelper<PayloadType::PIPE_WRITE_END> {
        util::refc_ptr<PipePayload> pipe;
        bool nonblock;
        bool closed;

        explicit PipeWriteEndPayload(PipePayload *pipe, bool nonblock);
        ~PipeWriteEndPayload() override;
        void destruct() override;

        [[nodiscard]]
        Payload *clone_payload() override;
    };

    class PipeReadEndObject : public CapObj<PipeReadEndPayload> {
    public:
        explicit PipeReadEndObject(util::nonnull<Capability *> cap)
            : CapObj<PipeReadEndPayload>(cap) {}

        [[nodiscard]]
        Result<size_t> read(void *buf, size_t len);
    };

    class PipeWriteEndObject : public CapObj<PipeWriteEndPayload> {
    public:
        explicit PipeWriteEndObject(util::nonnull<Capability *> cap)
            : CapObj<PipeWriteEndPayload>(cap) {}

        [[nodiscard]]
        Result<size_t> write(const void *buf, size_t len);
    };
}  // namespace cap
