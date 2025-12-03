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
#include <arch/riscv64/int/exception.h>
#include <arch/riscv64/int/isr.h>
#include <basec/logger.h>
#include <stddef.h>
#include <task/scheduler.h>

#if IVT_MODE == VECTORED
/**
 * @brief IVT表
 * 其中每一项为四字节, 存放一个指令
 * 我们均采取为跳转指令的形式
 * 即 j offset
 */
SECTION(.ivt)
dword IVT[IVT_ENTRIES] = {};

/**
 * @brief 生成跳转指令
 *
 * @param offset 跳转偏移量
 * @return dword 跳转指令
 */
static dword emit_j_ins(const dword offset) {
    if (offset & 0x3) {
        // 偏移量必须是4字节对齐的
        return 0;
    }

    const dword j_opcode = 0x6F;
    const dword imm20    = (offset >> 20) & 0x1;
    const dword imm10_1  = (offset >> 1) & 0x3FF;
    const dword imm11    = (offset >> 11) & 0x1;
    const dword imm19_12 = (offset >> 12) & 0xFF;

    const dword imm =
        (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12);

    return imm | j_opcode;
}

/**
 * @brief 生成IVT表项
 *
 * @param isr_func ISR例程
 * @param idx 表项索引
 * @return dword 表项内容
 */
static dword emit_ivt_entry(void (*isr_func)(void), int idx) {
    qword q_off = (qword)isr_func - (qword)IVT - (idx * sizeof(dword));
    return emit_j_ins((dword)q_off);
}
#endif

/**
 * @brief RISCV CPU中断标志位
 * 用于区分是中断还是异常
 * scause & RISCV_CPU_INTERRUPT_MASK = 0, 则为异常
 * 否则为中断
 */
#define RISCV_CPU_INTERRUPT_MASK (1ull << 63)

#if IVT_MODE == VECTORED

/**
 * @brief 默认ISR服务程序(Vectored模式)
 *
 */
ISR_SERVICE_ATTRIBUTE
void default_isr(void) {
    ISR_SERVICE_START(default_isr, 128);

    if (scause.interrupt) {
        log_info("默认处理程序: 这是一个中断(Vectored模式)");
    } else {
        log_info("默认处理程序: 这是一个异常(Vectored模式)");
    }

    ISR_SERVICE_END(default_isr);
}

/**
 * @brief 通用ISR服务程序(Vectored模式)
 *
 */
ISR_SERVICE_ATTRIBUTE
void general_isr(void) {
    ISR_SERVICE_START(general_isr, 128);

    if (scause.interrupt) {
        log_info("通用处理程序: 这是一个中断(Vectored模式)");
    } else {
        general_exception(scause, sepc, stval, reglist_ptr);
    }

    ISR_SERVICE_END(general_isr);
}

ISR_SERVICE_ATTRIBUTE
void timer_isr(void) {
    ISR_SERVICE_START(timer_isr, 128);

    if (cur_proc != nullptr) {
        schedule(&reglist_ptr);
    }

    timer_handler(scause, sepc, stval, reglist_ptr);

    ISR_SERVICE_END(timer_isr);
}

/**
 * @brief 测试例程
 *
 */
ISR_SERVICE_ATTRIBUTE
void test(void) {
    ISR_SERVICE_START(test, 128);

    if (scause.interrupt) {
        log_info("这是一个TEST:中断:TIMER");
    } else {
        log_info("这是一个TEST:异常:TIMER");
        general_exception(scause, sepc, stval, reglist_ptr);
    }

    ISR_SERVICE_END(test);
}

#elif IVT_MODE == DIRECT
/**
 * @brief ISR主服务程序(Direct模式)
 *
 * 根据scause将服务分发给不同例程
 *
 */
ISR_SERVICE_ATTRIBUTE
void primary_isr(void) {
    ISR_SERVICE_START(primary_isr, 128);

    if (scause & RISCV_CPU_INTERRUPT_MASK) {
        log_info("这是一个中断(Direct模式)");
        if (scause.cause == 5) {
            // 计时器中断
            timer_handler(scause, sepc, stval, reglist_ptr);
        } else {
            log_info("无对应中断解决方案: 0x%lx", scause.cause);
        }
    } else {
        general_exception(scause, sepc, stval, reglist_ptr);
    }

    ISR_SERVICE_END(primary_isr);
}
#endif

void init_ivt(void) {
#if IVT_MODE == VECTORED
    // 设置IVT表地址
    umb_t ivt_addr = (umb_t)IVT;
    if (ivt_addr & 0x3) {
        log_error("错误: IVT地址未对齐!");
        return;
    }
    csr_stvec_t stvec;
    stvec.ivt_addr = ivt_addr;
    // 设置为vectored模式
    stvec.mode     = 0b01;

    // 初始化IVT表
    for (int i = 0; i < IVT_ENTRIES; i++) {
        IVT[i] = emit_ivt_entry(default_isr, i);
    }

    IVT[0] = emit_ivt_entry(general_isr, 0);
    IVT[5] = emit_ivt_entry(timer_isr, 5);

    // 输出调试信息
    log_debug("IVT 地址: 0x%lx", (umb_t)IVT);
#elif IVT_MODE == DIRECT
    csr_stvec_t stvec;
    stvec.ivt_addr = (umb_t)primary_isr;
    // 采用direct模式
    stvec.mode     = 0b00;
    if (stvec & 0x3) {
        log_error("错误: stvec地址未对齐!");
        return;
    }
    log_debug("primary_isr 地址: 0x%lx", (umb_t)primary_isr);
#endif

    // 写入stvec寄存器
    csr_set_stvec(stvec);
}

void sti(void) {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sie           = 1;
    csr_set_sstatus(sstatus);
}