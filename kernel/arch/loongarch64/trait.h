/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 龙芯架构Trait
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/callconv.h>
#include <arch/loongarch64/ctx.h>
#include <arch/loongarch64/ctxlayout.h>
#include <arch/trait.h>
#include <sus/types.h>
#include <task/startup.h>

#include <cstddef>

namespace la64 {
    class EarlySerial {
    public:
        static void serial_write_char(char ch);
        static void serial_write_string(size_t len, const char *str);
    };

    static_assert(EarlySerialTrait<EarlySerial>);

    class Initialization {
    public:
        static void pre_init(void);
        static void post_init(void);
        static Result<void> init_clock();
    };

    static_assert(InitializationTrait<Initialization>);

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

    struct Interrupt {
        static void init(void);
        static void sti(void);
        static void cli(void);
        static bool enabled();
    };

    static_assert(InterruptTrait<Interrupt>);

    struct Idle {
        static void idle();
    };

    static_assert(IdleTrait<Idle>);

    class PageMan;
}  // namespace la64
