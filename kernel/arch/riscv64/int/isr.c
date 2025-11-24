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

#include <arch/riscv64/int/isr.h>
#include <arch/riscv64/int/trap.h>
#include <basec/logger.h>
#include <sbi/sbi.h>

enum {
    EXCEPTION_INST_MISALIGNED    = 0,   // 指令地址不对齐
    EXCEPTION_INST_ACCESS_FAULT  = 1,   // 指令访问错误
    EXCEPTION_ILLEGAL_INST       = 2,   // 非法指令
    EXCEPTION_BREAKPOINT         = 3,   // 断点
    EXCEPTION_LOAD_MISALIGNED    = 4,   // 加载地址不对齐
    EXCEPTION_LOAD_ACCESS_FAULT  = 5,   // 加载访问错误
    EXCEPTION_STORE_MISALIGNED   = 6,   // 存储地址不对齐
    EXCEPTION_STORE_ACCESS_FAULT = 7,   // 存储访问错误
    EXCEPTION_ECALL_U            = 8,   // 用户模式环境调用
    EXCEPTION_ECALL_S            = 9,   // 监管模式环境调用
    EXCEPTION_RESERVED_0         = 10,  // 保留
    EXCEPTION_RESERVED_1         = 11,  // 保留
    EXCEPTION_INST_PAGE_FAULT    = 12,  // 取指页错误
    EXCEPTION_LOAD_PAGE_FAULT    = 13,  // 加载页错误
    EXCEPTION_RESERVED_2         = 14,  // 保留
    EXCEPTION_STORE_PAGE_FAULT   = 15,  // 存储页错误
    EXCEPTION_RESERVED_3         = 16,  // 保留
    EXCEPTION_RESERVED_4         = 17,  // 保留
    EXCEPTION_SOFTWARE_CHECK     = 18,  // 软件检查异常
    EXCEPTION_HARDWARE_EEROR     = 19   // 硬件错误
};

void general_exception(csr_scause_t scause, umb_t sepc, umb_t stval,
                       InterruptContextRegisterList *reglist_ptr) {
    // 异常信息
    const char *exception_msg[] = {"指令地址不对齐",
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

    // 输出异常类型
    if (scause.cause < sizeof(exception_msg) / sizeof(exception_msg[0])) {
        log_info("发生异常! 类型: %s (%lu)", exception_msg[scause.cause],
                 scause.cause);
    } else {
        log_info("发生异常! 类型: 未知 (%lu)", scause.cause);
    }

    // 输出寄存器状态
    log_info("scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx", scause.value, sepc,
             stval);
    log_info("reglist_ptr: 0x%lx", reglist_ptr);

    // log_info("寄存器状态:");
    // log_info("x 0: 0000000000000000 ; x 1: %016lx ; x 2: %016lx ; x 3:
    // %016lx",
    //          reglist_ptr->regs[0], reglist_ptr->regs[1],
    //          reglist_ptr->regs[2]);

    // for (int i = 1; i < 8; i++) {
    //     log_info("x%2d: %016lx ; x%2d: %016lx ; x%2d: %016lx ; x%2d: %016lx",
    //              i * 4, reglist_ptr->regs[i * 4 - 1], i * 4 + 1,
    //              reglist_ptr->regs[i * 4 + 0], i * 4 + 2,
    //              reglist_ptr->regs[i * 4 + 1], i * 4 + 3,
    //              reglist_ptr->regs[i * 4 + 2]);
    // }

    // log_info("sstatus: 0x%lx", reglist_ptr->sstatus.value);

    // 输出异常发生特权级
    if (reglist_ptr->sstatus.spp) {
        log_info("异常发生在S-Mode");
    } else {
        log_info("异常发生在U-Mode");
    }

    switch (scause.cause) {
        case EXCEPTION_ILLEGAL_INST:
            illegal_instruction_handler(scause, sepc, stval, reglist_ptr);
            break;
        case EXCEPTION_INST_PAGE_FAULT:
        case EXCEPTION_LOAD_PAGE_FAULT:
        case EXCEPTION_STORE_PAGE_FAULT:
            paging_handler(scause, sepc, stval, reglist_ptr);
            break;
        default:
            log_error("无对应解决方案: 0x%lx", scause.cause);
            while (true);
    }
}

void illegal_instruction_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                                 InterruptContextRegisterList *reglist_ptr) {
    log_info("非法指令处理程序: sepc=0x%lx, stval=0x%lx", sepc, stval);

    // 我们可以通过该指令自定义kernel服务
    dword ins = *((dword *)sepc);
    log_info("指令内容: 0x%08x", ins);
    // 这是一个任意的非法指令
    // 被我们选中用于模拟真实指令
    if (ins == 0x000000FF) {
        log_info("自定义Kernel服务: Hello, World!");
    } else if (ins == 0x00FF00FF) {
        log_info("自定义Kernel服务: 计算t0的t1次方, 结果存储到t0中");
        int t0 = reglist_ptr->regs[5 - 1];  // x5 = t0
        int t1 = reglist_ptr->regs[6 - 1];  // x6 = t1
        log_info("计算参数: t0=%d, t1=%d", t0, t1);
        int result = 1;
        for (int i = 0; i < t1; i++) {
            result *= t0;
        }
        reglist_ptr->regs[5 - 1] = result;  // x5 = t0
        log_info("计算完成!");
    } else {
        log_error("非kernel自定义指令: 0x%08x", ins);
    }

    reglist_ptr->sepc += 4;  // 跳过该指令
}

void paging_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                    InterruptContextRegisterList *reglist_ptr) {
    log_info("页异常处理程序: scause=0x%lx, sepc=0x%lx, stval=0x%lx",
             scause.value, sepc, stval);

    log_info("异常页地址: 0x%016lx", stval);
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
                   InterruptContextRegisterList *reglist_ptr) {
    log_info("定时器中断处理程序: scause=0x%lx, sepc=0x%lx, stval=0x%lx",
             scause.value, sepc, stval);

    csr_t clock, clock_ms, clock_ns;

    clock    = csr_get_time();
    clock_ms = clock / (timer_info.freq / 1000);
    clock_ns = clock / (timer_info.freq / 1000000) - clock_ms * 1000;
    log_info("进入handler的时间: %ld.%03ld ms", clock_ms, clock_ns);

    // 接下来应当执行时钟中断相关处理
    // 1. 执行进程调度
    // 2. 更新系统时间
    // 3. 重新设置下一次时钟中断
    sbi_legacy_set_timer(csr_get_time() + timer_info.increasment);

    clock    = csr_get_time();
    clock_ms = clock / (timer_info.freq / 1000);
    clock_ns = clock / (timer_info.freq / 1000000) - clock_ms * 1000;
    log_info("离开handler的CPU时间: %ld.%03ld ms", clock_ms, clock_ns);
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