/**
 * @file context.h
 * @brief RISC-V64 调度上下文布局与切换 ABI
 */

#pragma once

#include <arch/context_trait.h>
#include <arch/riscv64/context_offsets.h>

#include <cstddef>

namespace riscv64::hal {
    struct alignas(16) Context final {
        static constexpr size_t SIZE_BYTES = RV_CTX_SIZE;

        addr_t _ra = 0;
        addr_t _sp = 0;
        xlen_t s0  = 0;
        xlen_t s1  = 0;
        xlen_t s2  = 0;
        xlen_t s3  = 0;
        xlen_t s4  = 0;
        xlen_t s5  = 0;
        xlen_t s6  = 0;
        xlen_t s7  = 0;
        xlen_t s8  = 0;
        xlen_t s9  = 0;
        xlen_t s10 = 0;
        xlen_t s11 = 0;

        [[nodiscard]] constexpr addr_t &ra() noexcept {
            return _ra;
        }
        [[nodiscard]] constexpr const addr_t &ra() const noexcept {
            return _ra;
        }
        [[nodiscard]] constexpr addr_t &sp() noexcept {
            return _sp;
        }
        [[nodiscard]] constexpr const addr_t &sp() const noexcept {
            return _sp;
        }
    };

#define RV_ASSERT_CONTEXT_FIELD(member, reg) \
    static_assert(offsetof(Context, member) == RV_CTX_##reg)
    RV_ASSERT_CONTEXT_FIELD(_ra, RA);
    RV_ASSERT_CONTEXT_FIELD(_sp, SP);
    RV_ASSERT_CONTEXT_FIELD(s0, S0);
    RV_ASSERT_CONTEXT_FIELD(s1, S1);
    RV_ASSERT_CONTEXT_FIELD(s2, S2);
    RV_ASSERT_CONTEXT_FIELD(s3, S3);
    RV_ASSERT_CONTEXT_FIELD(s4, S4);
    RV_ASSERT_CONTEXT_FIELD(s5, S5);
    RV_ASSERT_CONTEXT_FIELD(s6, S6);
    RV_ASSERT_CONTEXT_FIELD(s7, S7);
    RV_ASSERT_CONTEXT_FIELD(s8, S8);
    RV_ASSERT_CONTEXT_FIELD(s9, S9);
    RV_ASSERT_CONTEXT_FIELD(s10, S10);
    RV_ASSERT_CONTEXT_FIELD(s11, S11);
#undef RV_ASSERT_CONTEXT_FIELD

    static_assert(ContextTrait<Context>);
    static_assert(sizeof(Context) == RV_CTX_SIZE);
    static_assert(alignof(Context) == 16);

    extern "C" void __switch_to(Context *previous, Context *next) noexcept;
}  // namespace riscv64::hal
