/**
 * @file context.h
 * @brief LoongArch64 调度上下文布局与切换 ABI
 */

#pragma once

#include <arch/context_trait.h>
#include <arch/loongarch64/ctx_offsets.h>

#include <cstddef>

namespace loongarch64::hal {
    struct alignas(16) Context final {
        static constexpr size_t SIZE_BYTES = LA_CTX_SIZE;

        addr_t _ra = 0;
        addr_t _sp = 0;
        xlen_t fp  = 0;
        xlen_t s0  = 0;
        xlen_t s1  = 0;
        xlen_t s2  = 0;
        xlen_t s3  = 0;
        xlen_t s4  = 0;
        xlen_t s5  = 0;
        xlen_t s6  = 0;
        xlen_t s7  = 0;
        xlen_t s8  = 0;

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

#define LA_ASSERT_CONTEXT_FIELD(member, reg) \
    static_assert(offsetof(Context, member) == LA_CTX_##reg)
    LA_ASSERT_CONTEXT_FIELD(_ra, RA);
    LA_ASSERT_CONTEXT_FIELD(_sp, SP);
    LA_ASSERT_CONTEXT_FIELD(fp, FP);
    LA_ASSERT_CONTEXT_FIELD(s0, S0);
    LA_ASSERT_CONTEXT_FIELD(s1, S1);
    LA_ASSERT_CONTEXT_FIELD(s2, S2);
    LA_ASSERT_CONTEXT_FIELD(s3, S3);
    LA_ASSERT_CONTEXT_FIELD(s4, S4);
    LA_ASSERT_CONTEXT_FIELD(s5, S5);
    LA_ASSERT_CONTEXT_FIELD(s6, S6);
    LA_ASSERT_CONTEXT_FIELD(s7, S7);
    LA_ASSERT_CONTEXT_FIELD(s8, S8);
#undef LA_ASSERT_CONTEXT_FIELD

    static_assert(ContextTrait<Context>);
    static_assert(sizeof(Context) == LA_CTX_SIZE);
    static_assert(alignof(Context) == 16);

    extern "C" void __switch_to(Context *previous, Context *next) noexcept;
}  // namespace loongarch64::hal
