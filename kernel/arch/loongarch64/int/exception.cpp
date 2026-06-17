/**
 * @file exception.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 trap 入口处理
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/csr.h>
#include <arch/loongarch64/trait.h>
#include <logger.h>
#include <sus/logger.h>

using namespace la64;

namespace exception {
    constexpr umb_t INTERRUPT           = 0;  // 中断
    constexpr umb_t LOAD_PAGE_INVALID   = 1;  // load操作页无效例外
    constexpr umb_t STORE_PAGE_INVALID  = 2;  // store操作页无效例外
    constexpr umb_t FETCH_PAGE_INVALID  = 3;  // 取指操作页无效例外
    constexpr umb_t PAGE_MODIFICATION   = 4;  // 页修改例外
    constexpr umb_t PAGE_NOT_READABLE   = 5;  // 页不可读例外
    constexpr umb_t PAGE_NOT_EXECUTABLE = 6;  // 页不可执行例外
    constexpr umb_t PAGE_PRIVILEGE_VIOLATION = 7;  // 页特权等级不合规则例外
    constexpr umb_t FETCH_ADDRESS_FAULT = 8;          // 取指地址错例外
    constexpr umb_t MEMORY_ACCESS_ADDRESS_FAULT = 9;  // 访存指令地址错例外
    constexpr umb_t ADDRESS_MISALIGNED    = 10;       // 地址非对齐例外
    constexpr umb_t BOUNDARY_CHECK        = 11;       // 边界检查错例外
    constexpr umb_t SYSTEM_CALL           = 12;       // 系统调用例外
    constexpr umb_t BREAKPOINT            = 13;       // 断点例外
    constexpr umb_t INSTRUCTION_NOT_EXIST = 14;       // 指令不存在例外
    constexpr umb_t INSTRUCTION_PRIVILEGE_ERROR = 15;  // 指令特权等级错例外
    constexpr umb_t FLOAT_DISABLED = 16;  // 浮点指令未使能例外
    constexpr umb_t VECTOR_128_DISABLED = 17;  // 128位向量扩展指令未使能例外
    constexpr umb_t VECTOR_256_DISABLED = 18;  // 256位向量扩展指令未使能例外
    constexpr umb_t FLOAT_EXCEPTION        = 19;  // 基础浮点指令例外
    constexpr umb_t VECTOR_FLOAT_EXCEPTION = 20;  // 向量浮点指令例外
    constexpr umb_t WATCHPOINT_FETCH       = 21;  // 取指监测点例外
    constexpr umb_t WATCHPOINT_MEMORY = 22;  // load/store操作监测点例外
    constexpr umb_t BINARY_TRANSLATION_DISABLED =
        23;  // 二进制翻译扩展指令未使能例外
    constexpr umb_t BINARY_TRANSLATION_EXCEPTION = 24;  // 二进制翻译相关例外
    constexpr umb_t GUEST_SENSITIVE_PRIVILEGE_RESOURCE =
        25;                                // 客户机敏感特权资源例外
    constexpr umb_t HYPERVISOR_CALL = 26;  // 虚拟机监控调用例外
    constexpr umb_t GUEST_CSR_SOFTWARE_MODIFICATION =
        27;  // 客户机CSR软件修改例外
    constexpr umb_t GUEST_CSR_HARDWARE_MODIFICATION =
        28;  // 客户机CSR硬件修改例外

    const char *EXCEPTION_NAMES[] = {"中断",
                                     "load操作页无效例外",
                                     "store操作页无效例外",
                                     "取指操作页无效例外",
                                     "页修改例外",
                                     "页不可读例外",
                                     "页不可执行例外",
                                     "页特权等级不合规则例外",
                                     "取指地址错例外",
                                     "访存指令地址错例外",
                                     "地址非对齐例外",
                                     "边界检查错例外",
                                     "系统调用例外",
                                     "断点例外",
                                     "指令不存在例外",
                                     "指令特权等级错例外",
                                     "浮点指令未使能例外",
                                     "128位向量扩展指令未使能例外",
                                     "256位向量扩展指令未使能例外",
                                     "基础浮点指令例外",
                                     "向量浮点指令例外",
                                     "取指监测点例外",
                                     "load/store操作监测点例外",
                                     "二进制翻译扩展指令未使能例外",
                                     "二进制翻译相关例外",
                                     "客户机敏感特权资源例外",
                                     "虚拟机监控调用例外",
                                     "客户机CSR软件修改例外",
                                     "客户机CSR硬件修改例外"};

    /**
     * @brief 获取例外号对应的可读名称。
     *
     * @param cause 例外号（按上述常量定义）。
     * @return const char* 例外名称。
     */
    [[nodiscard]]
    const char *exception_name(umb_t cause) noexcept {
        if (cause < sizeof(EXCEPTION_NAMES) / sizeof(EXCEPTION_NAMES[0])) {
            return EXCEPTION_NAMES[cause];
        }
        return "未知";
    }
}  // namespace exception

extern "C" void isr_entry();

extern "C" void handle_trap(umb_t era, csr_estat_t estat, Context *ctx) {
    loggers::INTERRUPT::INFO("LoongArch64 trap: era=0x%lx estat=0x%lx ctx=%p",
                             era, estat.value, ctx);
    if (ctx == nullptr) {
        loggers::INTERRUPT::INFO("ctx: null");
        return;
    }

    loggers::INTERRUPT::INFO("ctx: sp=0x%lx ra=0x%lx tp=0x%lx fp=0x%lx",
                             ctx->sp(), ctx->ra, ctx->tp, ctx->fp);
    loggers::INTERRUPT::INFO("args: a0=0x%lx a1=0x%lx a2=0x%lx a3=0x%lx",
                             ctx->a0, ctx->a1, ctx->a2, ctx->a3);
    loggers::INTERRUPT::INFO("args: a4=0x%lx a5=0x%lx a6=0x%lx a7=0x%lx",
                             ctx->a4, ctx->a5, ctx->a6, ctx->a7);
    loggers::INTERRUPT::INFO(
        "temp: t0=0x%lx t1=0x%lx t2=0x%lx t3=0x%lx t4=0x%lx", ctx->t0, ctx->t1,
        ctx->t2, ctx->t3, ctx->t4);
    loggers::INTERRUPT::INFO("temp: t5=0x%lx t6=0x%lx t7=0x%lx t8=0x%lx",
                             ctx->t5, ctx->t6, ctx->t7, ctx->t8);
    loggers::INTERRUPT::INFO("saved: u0=0x%lx s0=0x%lx s1=0x%lx s2=0x%lx",
                             ctx->u0, ctx->s0, ctx->s1, ctx->s2);
    loggers::INTERRUPT::INFO(
        "saved: s3=0x%lx s4=0x%lx s5=0x%lx s6=0x%lx s7=0x%lx s8=0x%lx", ctx->s3,
        ctx->s4, ctx->s5, ctx->s6, ctx->s7, ctx->s8);

    while (true);
}

void Interrupt::init() {
    auto isr_addr = reinterpret_cast<umb_t>(&isr_entry);
    LA64_CSR_WRITE(CSR_EENTRY, isr_addr);
    loggers::INTERRUPT::INFO("LoongArch64 isr_entry 已设置: 0x%lx", isr_addr);
}

void Interrupt::sti() {
    LA64_CSR_SET(CSR_CRMD, CRMD_IE);
}

void Interrupt::cli() {
    LA64_CSR_CLEAR(CSR_CRMD, CRMD_IE);
}

bool Interrupt::enabled() {
    return (LA64_CSR_READ(CSR_CRMD) & CRMD_IE) != 0;
}
