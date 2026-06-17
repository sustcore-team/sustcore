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
        umb_t estat;
        umb_t _kstack_top;

        constexpr umb_t &pc() {
            return era;
        }

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

        constexpr void setup_regs(bool smode, bool sie, bool spie) {
            (void)smode;
            (void)sie;
            (void)spie;
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
    static_assert(offsetof(Context, estat) == CTX_SLOT_OFFSET(CTX_ESTAT_SLOT));
}  // namespace la64
