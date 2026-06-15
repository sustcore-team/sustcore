/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Riscv64架构Trait
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/csr.h>
#include <arch/riscv64/ctxlayout.h>
#include <arch/trait.h>
#include <sus/types.h>
#include <syscall/packs.h>
#include <task/startup.h>

namespace rv64 {
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
        umb_t regs[CTX_SEPC_SLOT];
        umb_t sepc;
        csr_sstatus_t sstatus;
        umb_t kstack_sp;

        constexpr umb_t &pc() {
            return this->sepc;
        }

        constexpr static size_t X1_BASE = CTX_X1_SLOT;
        constexpr static size_t RA_BASE = CTX_RA_SLOT;
        constexpr static size_t TP_BASE = CTX_TP_SLOT;
        constexpr static size_t A0_BASE = CTX_A0_SLOT;
        constexpr static size_t S0_BASE = CTX_S0_SLOT;

        constexpr umb_t &sp() {
            return this->regs[CTX_SP_SLOT];
        }

    /**
     * @brief 访问保存的 tp 寄存器.
     *
     * @return umb_t& 保存的 tp 值
     */
        constexpr umb_t &tp() {
            return this->regs[TP_BASE];
        }

    /**
     * @brief 访问保存的只读 tp 寄存器.
     *
     * @return const umb_t& 保存的 tp 值
     */
        [[nodiscard]]
        constexpr const umb_t &tp() const {
            return this->regs[TP_BASE];
        }

    /**
     * @brief 获取 trap 上下文在栈上的总字节数.
     *
     * @return size_t 上下文大小
     */
        [[nodiscard]]
        constexpr static size_t size_bytes() noexcept {
            return CTX_SLOT_OFFSET(CTX_SLOT_COUNT);
        }

        constexpr void setup_regs(bool smode, bool sie, bool spie) {
            this->regs[RA_BASE] = 0;      // ra设置为0
            this->sstatus.spp   = smode;  // 根据 smode 设置 spp 位
            this->sstatus.sie   = sie;      // 应该开启中断
            this->sstatus.spie  = spie;      // 应该开启中断
        }

        constexpr void write_ret(const syscall::RetPack &pack) {
            regs[A0_BASE]     = pack.ret0;
            regs[A0_BASE + 1] = pack.ret1;
        }

        constexpr void read_args(syscall::ArgPack &pack) const {
            pack.syscall_number = regs[A0_BASE + 7];  // a7: syscall number
            pack.capidx         = regs[A0_BASE + 0];

            pack.args[0] = regs[A0_BASE + 1];
            pack.args[1] = regs[A0_BASE + 2];
            pack.args[2] = regs[A0_BASE + 3];
            pack.args[3] = regs[A0_BASE + 4];
            pack.args[4] = regs[A0_BASE + 5];
            pack.args[5] = regs[A0_BASE + 6];
        }

        [[nodiscard]]
        constexpr syscall::ArgPack read_args() const {
            syscall::ArgPack pack{};
            read_args(pack);
            return pack;
        }
    };

    static_assert(ContextTrait<Context>);
    static_assert(sizeof(Context) == Context::size_bytes(),
                  "rv64::Context layout must match ctxlayout slots");

    struct Interrupt {
        /**
         * @brief 初始化IVT
         *
         */
        static void init(void);

        /**
         * @brief 启用中断
         *
         */
        static void sti(void);

        /**
         * @brief 关闭中断
         *
         */
        static void cli(void);

        static bool enabled() {
            csr_sstatus_t sstatus = csr_get_sstatus();
            return sstatus.sie;
        }
    };

    static_assert(InterruptTrait<Interrupt>);

    struct Idle {
        static void idle();
    };
    static_assert(IdleTrait<Idle>);
}  // namespace rv64

#include <arch/riscv64/mem/pageman.h>
