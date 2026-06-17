/**
 * @file ctx.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V Trap 上下文定义
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/callconv.h>
#include <arch/riscv64/csr.h>
#include <arch/riscv64/ctxlayout.h>
#include <arch/trait.h>
#include <sus/types.h>

#include <cstddef>

namespace rv64 {
    struct Context {
        umb_t ra;
        umb_t _sp;
        umb_t gp;
        umb_t _tp;
        umb_t t0;
        umb_t t1;
        umb_t t2;
        umb_t s0;
        umb_t s1;
        umb_t a0;
        umb_t a1;
        umb_t a2;
        umb_t a3;
        umb_t a4;
        umb_t a5;
        umb_t a6;
        umb_t a7;
        umb_t s2;
        umb_t s3;
        umb_t s4;
        umb_t s5;
        umb_t s6;
        umb_t s7;
        umb_t s8;
        umb_t s9;
        umb_t s10;
        umb_t s11;
        umb_t t3;
        umb_t t4;
        umb_t t5;
        umb_t t6;
        umb_t sepc;
        csr_sstatus_t sstatus;
        umb_t kstack_sp;

        constexpr umb_t &pc() {
            return sepc;
        }

        constexpr umb_t &sp() {
            return _sp;
        }

        [[nodiscard]]
        constexpr const umb_t &sp() const {
            return _sp;
        }

        constexpr umb_t &kstack_top() {
            return kstack_sp;
        }

        constexpr umb_t &tp() {
            return _tp;
        }

        [[nodiscard]]
        constexpr const umb_t &tp() const {
            return _tp;
        }

        [[nodiscard]]
        constexpr static size_t size_bytes() noexcept {
            return CTX_SLOT_OFFSET(CTX_SLOT_COUNT);
        }

        constexpr void setup_regs(bool smode, bool sie, bool spie) {
            ra           = 0;
            sstatus.spp  = smode;
            sstatus.sie  = sie;
            sstatus.spie = spie;
        }
    };

    static_assert(ContextTrait<Context>);
    static_assert(sizeof(Context) == Context::size_bytes(),
                  "rv64::Context layout must match ctxlayout slots");
    static_assert(offsetof(Context, ra) == CTX_SLOT_OFFSET(CTX_RA_SLOT));
    static_assert(offsetof(Context, _sp) == CTX_SLOT_OFFSET(CTX_SP_SLOT));
    static_assert(offsetof(Context, gp) == CTX_SLOT_OFFSET(CTX_GP_SLOT));
    static_assert(offsetof(Context, _tp) == CTX_SLOT_OFFSET(CTX_TP_SLOT));
    static_assert(offsetof(Context, a0) == CTX_SLOT_OFFSET(CTX_A0_SLOT));
    static_assert(offsetof(Context, a7) == CTX_SLOT_OFFSET(CTX_A7_SLOT));
    static_assert(offsetof(Context, sepc) == CTX_SLOT_OFFSET(CTX_SEPC_SLOT));
    static_assert(offsetof(Context, sstatus) ==
                  CTX_SLOT_OFFSET(CTX_SSTATUS_SLOT));
    static_assert(offsetof(Context, kstack_sp) ==
                  CTX_SLOT_OFFSET(CTX_KSTACK_SP_SLOT));

    constexpr void write_ret(Context &ctx, const syscall::RetPack &pack) {
        ctx.a0 = pack.ret0;
        ctx.a1 = pack.ret1;
    }

    constexpr void read_args(const Context &ctx, syscall::ArgPack &pack) {
        pack.syscall_number = ctx.a7;
        pack.args[0]        = ctx.a0;
        pack.args[1]        = ctx.a1;
        pack.args[2]        = ctx.a2;
        pack.args[3]        = ctx.a3;
        pack.args[4]        = ctx.a4;
        pack.args[5]        = ctx.a5;
        pack.args[6]        = ctx.a6;
    }

    [[nodiscard]]
    constexpr syscall::ArgPack read_args(const Context &ctx) {
        syscall::ArgPack pack{};
        read_args(ctx, pack);
        return pack;
    }
}  // namespace rv64
