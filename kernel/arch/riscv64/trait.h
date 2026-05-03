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
#include <arch/trait.h>
#include <sus/types.h>
#include <syscall/syscall.h>

// 该功能必须通过宏来实现, 以保证其一定会内联到原代码片段中
#define RELOAD_SP() asm volatile("la sp, boot_stack_top\n");

class Riscv64Serial {
public:
    static void serial_write_char(char ch);
    static void serial_write_string(size_t len, const char *str);
};

static_assert(SerialTrait<Riscv64Serial>);

class Riscv64Initialization {
public:
    static void pre_init(void);
    static void post_init(void);
};

static_assert(InitializationTrait<Riscv64Initialization>);

class Riscv64MemoryLayout {
public:
    /**
     * @brief 检测内存布局
     *
     * @param regions 内存区域数组
     * @param cnt 数组大小
     * @return 内存区域数量, < 0说明数组大小不足
     */
    static Result<void> detect();
};

static_assert(MemoryLayoutTrait<Riscv64MemoryLayout>);

struct Riscv64Context {
    umb_t regs[31];
    umb_t sepc;
    csr_sstatus_t sstatus;
    umb_t kstack_sp;

    constexpr umb_t &pc() {
        return this->sepc;
    }

    constexpr static size_t X1_BASE = 0;
    constexpr static size_t RA_BASE = 1;
    // x10 = a0
    constexpr static size_t A0_BASE = X1_BASE + 9;

    constexpr umb_t &sp() {
        return this->regs[X1_BASE + 1];  // x2 = sp
    }

    static void switch_to(void *kstack);
    constexpr static size_t CONTEXT_OFFSET = 0;
    inline static Riscv64Context *from_kstack(void *kstack_top) {
        auto kstack_addr = reinterpret_cast<size_t>(kstack_top);
        return reinterpret_cast<Riscv64Context *>(kstack_addr - CONTEXT_OFFSET -
                                                  sizeof(Riscv64Context));
    }

    constexpr void setup_regs(bool smode) {
        this->regs[RA_BASE]      = 0;  // ra设置为0
        this->sstatus.spp  = smode;  // 代码运行在 S/U-Mode
        this->sstatus.spie = 1;  // 用户进程应该开启中断
    }

    constexpr void write_ret(const syscall::RetPack &pack)
    {
        regs[A0_BASE] = pack.ret0;
        regs[A0_BASE + 1] = pack.ret1;
    }

    constexpr void read_args(syscall::ArgPack& pack) const
    {
        pack.syscall_number = regs[A0_BASE + 7]; // a7: syscall number
        pack.capidx = regs[A0_BASE + 6];

        pack.args[0] = regs[A0_BASE + 0];
        pack.args[1] = regs[A0_BASE + 1];
        pack.args[2] = regs[A0_BASE + 2];
        pack.args[3] = regs[A0_BASE + 3];
        pack.args[4] = regs[A0_BASE + 4];
    }

    [[nodiscard]]
    constexpr syscall::ArgPack read_args() const
    {
        syscall::ArgPack pack{};
        read_args(pack);
        return pack;
    }
};

static_assert(ContextTrait<Riscv64Context>);


struct Riscv64Interrupt {
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
};

static_assert(InterruptTrait<Riscv64Interrupt>);

struct Riscv64WPFault {
    int reserved;
};
static_assert(WPFaultTrait<Riscv64WPFault>);

#include <arch/riscv64/mem/sv39.h>