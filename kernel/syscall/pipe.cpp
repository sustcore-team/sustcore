/**
 * @file pipe.cpp
 * @brief Pipe syscalls
 */

#include <cap/cholder.h>
#include <logger.h>
#include <object/perm.h>
#include <object/pipe.h>
#include <guard.h>
#include <sus/raii.h>
#include <sustcore/errcode.h>
#include <syscall/pipe.h>
#include <task/scheduler.h>

#include <cassert>
#include <cstring>

namespace syscall {
    namespace {
        [[nodiscard]]
        Result<task::TCB *> running_tcb() noexcept {
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current == nullptr || current->task == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return current;
        }

        [[nodiscard]]
        Result<cap::CHolder *> current_holder() noexcept {
            auto current_res = running_tcb();
            propagate(current_res);
            auto *holder = current_res.value()->task->cholder;
            if (holder == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return holder;
        }

        [[nodiscard]]
        Result<cap::Capability *> lookup_current_cap(CapIdx idx) {
            auto holder_res = current_holder();
            propagate(holder_res);
            return holder_res.value()->lookup(idx);
        }
    }  // namespace

    Result<void> pipe_create(bool nonblock, UBuffer &&out_buf) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto *holder = holder_res.value();

        auto *pipe = new cap::PipePayload();
        if (pipe == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        pipe->keep();
        auto pipe_guard = util::Guard([pipe]() { pipe->release(); });

        auto read_payload =
            util::owner(new cap::PipeReadEndPayload(pipe, nonblock));
        if (read_payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto read_payload_guard = delete_guard(read_payload);
        auto read_cap_res = holder->insert_to_free(
            read_payload, perm::pipe::READ | perm::basic::CLONE);
        propagate(read_cap_res);
        read_payload_guard.release();
        CapIdx read_cap = read_cap_res.value();
        auto read_guard = util::Guard([holder, read_cap]() {
            auto remove_res = holder->remove(read_cap);
            assert(remove_res.has_value());
        });

        auto write_payload =
            util::owner(new cap::PipeWriteEndPayload(pipe, nonblock));
        if (write_payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto write_payload_guard = delete_guard(write_payload);
        auto write_cap_res = holder->insert_to_free(
            write_payload, perm::pipe::WRITE | perm::basic::CLONE);
        propagate(write_cap_res);
        write_payload_guard.release();
        CapIdx write_cap = write_cap_res.value();
        auto write_guard = util::Guard([holder, write_cap]() {
            auto remove_res = holder->remove(write_cap);
            assert(remove_res.has_value());
        });

        PipeCreateRet ret{
            .read_cap  = read_cap,
            .write_cap = write_cap,
        };
        memcpy(out_buf.kbuf(), &ret, sizeof(ret));
        auto commit_res = out_buf.commit_to_user();
        propagate(commit_res);

        write_guard.release();
        read_guard.release();
        void_return();
    }

    Result<size_t> pipe_read(CapIdx read_cap, UBuffer &&buf, size_t len) {
        auto cap_res = lookup_current_cap(read_cap);
        propagate(cap_res);
        auto *cap = cap_res.value();
        if (cap->payload()->type_id() != PayloadType::PIPE_READ_END) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::PipeReadEndObject obj(util::nnullforce(cap));
        auto read_res = obj.read(buf.kbuf(), len);
        propagate(read_res);
        auto commit_res = buf.commit_to_user(read_res.value());
        propagate(commit_res);
        return read_res.value();
    }

    Result<size_t> pipe_write(CapIdx write_cap, UBuffer &&buf, size_t len) {
        auto cap_res = lookup_current_cap(write_cap);
        propagate(cap_res);
        auto *cap = cap_res.value();
        if (cap->payload()->type_id() != PayloadType::PIPE_WRITE_END) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::PipeWriteEndObject obj(util::nnullforce(cap));
        return obj.write(buf.kbuf(), len);
    }
}  // namespace syscall
