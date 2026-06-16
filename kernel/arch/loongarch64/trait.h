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

#include <arch/loongarch64/ctxlayout.h>
#include <arch/trait.h>
#include <sus/types.h>
#include <syscall/packs.h>
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
    };

    static_assert(InitializationTrait<Initialization>);

    struct Context {
        umb_t _pc = 0;
        umb_t _sp = 0;
        umb_t kstack_sp = 0;
        umb_t regs[32]{};

        constexpr umb_t &pc() {
            return _pc;
        }

        constexpr umb_t &sp() {
            return _sp;
        }

        constexpr static size_t RA_BASE = 1;
        constexpr static size_t TP_BASE = 2;
        constexpr static size_t A0_BASE = 4;
        constexpr static size_t S0_BASE = 8;

        constexpr void setup_regs(bool smode, bool sie, bool spie) {
            (void)smode;
            (void)sie;
            (void)spie;
        }

        constexpr void write_ret(const syscall::RetPack &pack) {
            regs[A0_BASE]     = pack.ret0;
            regs[A0_BASE + 1] = pack.ret1;
        }

        constexpr void read_args(syscall::ArgPack &pack) const {
            pack.capidx         = regs[A0_BASE];
            pack.args[0]        = regs[A0_BASE + 1];
            pack.args[1]        = regs[A0_BASE + 2];
            pack.args[2]        = regs[A0_BASE + 3];
            pack.args[3]        = regs[A0_BASE + 4];
            pack.args[4]        = regs[A0_BASE + 5];
            pack.args[5]        = regs[A0_BASE + 6];
            pack.syscall_number = regs[A0_BASE + 7];
        }

        [[nodiscard]]
        constexpr syscall::ArgPack read_args() const {
            syscall::ArgPack pack{};
            read_args(pack);
            return pack;
        }
    };

    static_assert(ContextTrait<Context>);

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
