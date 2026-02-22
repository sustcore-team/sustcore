/**
 * @file exception.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/int/isr.h>
#include <arch/riscv64/trait.h>
#include <kio.h>
#include <sus/logger.h>
#include <sus/types.h>
#include <new>

extern "C" void handle_trap(csr_scause_t scause, umb_t sepc, umb_t stval,
                            Riscv64Context *ctx) {
    if (scause.interrupt) {
        if (scause.cause == 5) {
            Handlers::timer(scause, sepc, stval, ctx);
        }
    } else {
        // 异常
        Handlers::exception(scause, sepc, stval, ctx);
    }
}

extern "C" void isr_entry(void);

void Riscv64Interrupt::init(void) {
    // 重置 sscratch 寄存器
    csr_set_sscratch(0);
    csr_stvec_t stvec;
    stvec.ivt_addr = (umb_t)isr_entry;
    // 采用direct模式
    stvec.mode     = 0b00;
    if (stvec.value & 0x3) {
        INTERRUPT::ERROR("错误: stvec地址未对齐!");
        return;
    }
    INTERRUPT::DEBUG("isr_entry 地址: 0x%lx", (umb_t)isr_entry);

    // 写入stvec寄存器
    csr_set_stvec(stvec);
}

void Riscv64Interrupt::sti(void) {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sie           = 1;
    csr_set_sstatus(sstatus);
}

void Riscv64Interrupt::cli(void) {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sie           = 0;
    csr_set_sstatus(sstatus);
}

void Riscv64Context::switch_to(void *kstack) {
    csr_set_sscratch((umb_t)kstack);
}