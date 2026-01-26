/**
 * @file isr.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断处理例程
 * @version alpha-1.0.0
 * @date 2025-11-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/configuration.h>
#include <arch/riscv64/int/isr.h>
#include <arch/riscv64/int/trap.h>
#include <basecpp/logger.h>
#include <sbi/sbi.h>
#include <kio.h>

namespace Exceptions {
    constexpr umb_t INST_MISALIGNED    = 0;   // 指令地址不对齐
    constexpr umb_t INST_ACCESS_FAULT  = 1;   // 指令访问错误
    constexpr umb_t ILLEGAL_INST       = 2;   // 非法指令
    constexpr umb_t BREAKPOINT         = 3;   // 断点
    constexpr umb_t LOAD_MISALIGNED    = 4;   // 加载地址
    constexpr umb_t LOAD_ACCESS_FAULT  = 5;   // 加载访问错误
    constexpr umb_t STORE_MISALIGNED   = 6;   // 存储
    constexpr umb_t STORE_ACCESS_FAULT = 7;   // 存储访问错误
    constexpr umb_t ECALL_U            = 8;   // 用户模式环境调用
    constexpr umb_t ECALL_S            = 9;   // 监管模式环境调用
    constexpr umb_t RESERVED_0         = 10;  // 保留
    constexpr umb_t RESERVED_1         = 11;  // 保留
    constexpr umb_t INST_PAGE_FAULT    = 12;  // 取指页
    constexpr umb_t LOAD_PAGE_FAULT    = 13;  // 加载页错误
    constexpr umb_t RESERVED_2         = 14;  // 保留
    constexpr umb_t STORE_PAGE_FAULT   = 15;  // 存储页
    constexpr umb_t RESERVED_3         = 16;  // 保留
    constexpr umb_t RESERVED_4         = 17;  // 保留
    constexpr umb_t SOFTWARE_CHECK     = 18;  // 软件检查异常
    constexpr umb_t HARDWARE_EEROR     = 19;  // 硬件错误

    const char *MSG[] = {"指令地址不对齐",
                               "指令访问错误",
                               "非法指令",
                               "断点",
                               "加载地址不对齐",
                               "加载访问错误",
                               "存储地址不对齐",
                               "存储访问错误",
                               "用户模式环境调用",
                               "监管模式环境调用",
                               "保留",
                               "保留",
                               "取指页错误",
                               "加载页错误",
                               "保留",
                               "存储页错误",
                               "保留",
                               "保留",
                               "软件检查异常",
                               "硬件错误"};
};

void general_exception(csr_scause_t scause, umb_t sepc, umb_t stval,
                       ArchContext *ctx) {
    switch (scause.cause) {
        case Exceptions::ECALL_U: {
            break;
        }
        case Exceptions::ILLEGAL_INST:
            illegal_instruction_handler(scause, sepc, stval, ctx);
            break;
        case Exceptions::INST_PAGE_FAULT:
        case Exceptions::LOAD_PAGE_FAULT:
        case Exceptions::STORE_PAGE_FAULT:
            paging_handler(scause, sepc, stval, ctx);
            break;
        default:
            // 输出异常类型
            if (scause.cause < sizeof(Exceptions::MSG) / sizeof(Exceptions::MSG[0]))
            {
                INTERRUPT.ERROR("发生异常! 类型: %s (%lu)",
                          Exceptions::MSG[scause.cause], scause.cause);
            } else {
                INTERRUPT.ERROR("发生异常! 类型: 未知 (%lu)", scause.cause);
            }

            // 输出寄存器状态
            INTERRUPT.ERROR("scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx", scause.value,
                      sepc, stval);
            INTERRUPT.ERROR("ctx: 0x%lx", ctx);

            // 输出异常发生特权级
            if (ctx->sstatus.spp) {
                INTERRUPT.ERROR("异常发生在S-Mode");
            } else {
                INTERRUPT.ERROR("异常发生在U-Mode");
            }
            INTERRUPT.ERROR("无对应解决方案: 0x%lx", scause.cause);
            while (true);
    }
}

void illegal_instruction_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                                 ArchContext *ctx) {
    INTERRUPT.DEBUG("发生异常! 类型: %s (%lu)", Exceptions::MSG[scause.cause],
              scause.cause);
    INTERRUPT.INFO("非法指令处理程序: sepc=0x%lx, stval=0x%lx", sepc, stval);
    if (ctx->sstatus.spp) {
        INTERRUPT.DEBUG("异常发生在S-Mode");
    } else {
        INTERRUPT.DEBUG("异常发生在U-Mode");
    }

    // 我们可以通过该指令自定义kernel服务
    dword ins = *((dword *)sepc);
    INTERRUPT.INFO("指令内容: 0x%08x", ins);
    // 这是一个任意的非法指令
    // 被我们选中用于模拟真实指令
    if (ins == 0x000000FF) {
        INTERRUPT.INFO("自定义Kernel服务: Hello, World!");
    } else if (ins == 0x00FF00FF) {
        INTERRUPT.INFO("自定义Kernel服务: 计算t0的t1次方, 结果存储到t0中");
        int t0 = ctx->regs[5 - 1];  // x5 = t0
        int t1 = ctx->regs[6 - 1];  // x6 = t1
        INTERRUPT.INFO("计算参数: t0=%d, t1=%d", t0, t1);
        int result = 1;
        for (int i = 0; i < t1; i++) {
            result *= t0;
        }
        ctx->regs[5 - 1] = result;  // x5 = t0
        INTERRUPT.INFO("计算完成!");
    } else {
        INTERRUPT.ERROR("非kernel自定义指令: 0x%08x", ins);
    }

    ctx->sepc += 4;  // 跳过该指令
}

void paging_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                    ArchContext *ctx) {
    INTERRUPT.DEBUG("发生异常! 类型: %s (%lu)", Exceptions::MSG[scause.cause],
              scause.cause);
    INTERRUPT.INFO("页异常处理程序: scause=0x%lx, sepc=0x%lx, stval=0x%lx",
             scause.value, sepc, stval);

    INTERRUPT.INFO("异常页地址: 0x%016lx", stval);

    if (ctx->sstatus.spp) {
        INTERRUPT.DEBUG("异常发生在S-Mode");
    } else {
        INTERRUPT.DEBUG("异常发生在U-Mode");
    }
    while (true);

    // 接下来应该执行页异常相关处理
    // 1. 检查地址是否合法
    // 2. 如果是缺页, 则分配物理页并映射
    // 2.1 如果是缺页, 且该页属于虚拟内存, 则从磁盘中调入
    // 2.2 如果是写保护错误, 则检查是否为写时复制
    // 2.3 更新页表项和TLB
    // 3. 如果是权限错误, 则终止相关进程
}

struct {
    umb_t freq;
    umb_t expected_freq;
    umb_t increasment;
} timer_info;

void timer_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                   ArchContext *ctx) {
    // 重新设置下一次时钟中断
    sbi_legacy_set_timer(csr_get_time() + timer_info.increasment);
}

void init_timer(umb_t freq, umb_t expected_freq) {
    // 设置计时器频率(freq:Hz, expected_freq:mHz(10^-3 Hz))
    umb_t increasment        = freq / expected_freq * 1000;
    timer_info.freq          = freq;
    timer_info.expected_freq = expected_freq;
    timer_info.increasment   = increasment;

    // 之后稳定触发
    sbi_legacy_set_timer(csr_get_time() + increasment);

    // 启用S-Mode计时器中断
    csr_sie_t sie = csr_get_sie();
    sie.stie      = 1;
    csr_set_sie(sie);
}