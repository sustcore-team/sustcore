/**
 * @file ctx.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 Trap/调度上下文定义
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/csr.h>
#include <arch/loongarch64/csrnum.h>
#include <arch/loongarch64/ctxlayout.h>
#include <arch/trait.h>
#include <sus/types.h>

#include <cstddef>

namespace la64 {
    struct Context {
        umb_t ra;
        umb_t tp;
        umb_t _sp;
        umb_t a0;
        umb_t a1;
        umb_t a2;
        umb_t a3;
        umb_t a4;
        umb_t a5;
        umb_t a6;
        umb_t a7;
        umb_t t0;
        umb_t t1;
        umb_t t2;
        umb_t t3;
        umb_t t4;
        umb_t t5;
        umb_t t6;
        umb_t t7;
        umb_t t8;
        umb_t u0;
        umb_t fp;
        umb_t s0;
        umb_t s1;
        umb_t s2;
        umb_t s3;
        umb_t s4;
        umb_t s5;
        umb_t s6;
        umb_t s7;
        umb_t s8;
        umb_t era;
        csr_crmd_t crmd;
        csr_prmd_t prmd;
        umb_t estat;
        umb_t _kstack_top;

        [[nodiscard]]
        constexpr umb_t &pc() {
            return era;
        }
        [[nodiscard]]
        constexpr const umb_t &pc() const {
            return era;
        }

        [[nodiscard]]
        constexpr umb_t &sp() {
            return _sp;
        }

        [[nodiscard]]
        constexpr const umb_t &sp() const {
            return _sp;
        }

        [[nodiscard]]
        constexpr umb_t &kstack_top() {
            return _kstack_top;
        }

        [[nodiscard]]
        constexpr const umb_t &kstack_top() const {
            return _kstack_top;
        }

        constexpr void setup_regs(bool smode, bool sie, bool spie) {
            ra         = 0;
            crmd       = {};
            prmd       = {};
            crmd.plv   = PLV_KERNEL;
            crmd.ie    = sie;
            crmd.da    = 0;
            crmd.pg    = 1;
            crmd.datf  = 0b01;
            crmd.datm  = 0b01;
            crmd.we    = 0;
            prmd.pplv  = smode ? PLV_KERNEL : PLV_USER;
            prmd.pie   = spie;
        }

        template <SetupCase setcase>
        constexpr void setup_regs() {
            if constexpr (setcase == SetupCase::UTHREAD_TRAMPOLINE) {
                setup_regs(true, false, true);
            } else if constexpr (setcase == SetupCase::USER_THREAD) {
                setup_regs(false, false, true);
            } else if constexpr (setcase == SetupCase::KTHREAD) {
                setup_regs(true, true, false);
            } else {
                static_assert(setupcase_dependent_false<setcase>,
                              "Invalid case!");
            }
        }

        [[nodiscard]]
        constexpr static size_t size_bytes() noexcept {
            return CTX_SLOT_OFFSET(CTX_SLOT_COUNT);
        }
    };

    static_assert(ContextTrait<Context>);
    static_assert(sizeof(Context) == Context::size_bytes(),
                  "la64::Context layout must match ctxlayout slots");
    static_assert(offsetof(Context, ra) == CTX_SLOT_OFFSET(CTX_RA_SLOT));
    static_assert(offsetof(Context, tp) == CTX_SLOT_OFFSET(CTX_TP_SLOT));
    static_assert(offsetof(Context, _sp) == CTX_SLOT_OFFSET(CTX_SP_SLOT));
    static_assert(offsetof(Context, a0) == CTX_SLOT_OFFSET(CTX_A0_SLOT));
    static_assert(offsetof(Context, a7) == CTX_SLOT_OFFSET(CTX_A7_SLOT));
    static_assert(offsetof(Context, t8) == CTX_SLOT_OFFSET(CTX_T8_SLOT));
    static_assert(offsetof(Context, u0) == CTX_SLOT_OFFSET(CTX_U0_SLOT));
    static_assert(offsetof(Context, fp) == CTX_SLOT_OFFSET(CTX_FP_SLOT));
    static_assert(offsetof(Context, s8) == CTX_SLOT_OFFSET(CTX_S8_SLOT));
    static_assert(offsetof(Context, era) == CTX_SLOT_OFFSET(CTX_ERA_SLOT));
    static_assert(offsetof(Context, crmd) == CTX_SLOT_OFFSET(CTX_CRMD_SLOT));
    static_assert(offsetof(Context, prmd) == CTX_SLOT_OFFSET(CTX_PRMD_SLOT));
    static_assert(offsetof(Context, estat) == CTX_SLOT_OFFSET(CTX_ESTAT_SLOT));
    static_assert(offsetof(Context, _kstack_top) ==
                  CTX_SLOT_OFFSET(CTX_KSTACK_SLOT));
}  // namespace la64
