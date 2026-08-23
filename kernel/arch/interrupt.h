/**
 * @file interrupt.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构陷阱与通用中断接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>
#include <cpu/local.h>
#include <memory/virtual/flags.h>
#include <tay/bits.h>

#include <cstddef>

extern "C" void kernel_preemption_checkpoint() noexcept;

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    [[nodiscard]] bool irq_enabled() noexcept;

    inline void preempt_checkpoint() noexcept {
        if (!cpu::preempt_disabled() && irq_enabled() && cpu::take_resched())
            kernel_preemption_checkpoint();
    }

    /**
     * @brief 保护当前 CPU 的调度抢占深度，不修改本地 IRQ 状态。
     * @note 最外层退出在中断已开启且存在 deferred 请求时触发一次统一 checkpoint；硬中断
     *       上下文只保留请求，切换仍由可安全的调度路径执行。
     */
    class preempt_guard final {
    public:
        preempt_guard() noexcept {
            cpu::preempt_enter();
        }
        ~preempt_guard() noexcept {
            if (!cpu::preempt_leave())
                __builtin_trap();
            preempt_checkpoint();
        }

        preempt_guard(const preempt_guard &)            = delete;
        preempt_guard &operator=(const preempt_guard &) = delete;
        preempt_guard(preempt_guard &&)                 = delete;
        preempt_guard &operator=(preempt_guard &&)      = delete;
    };

    /**
     * @brief RAII 保存当前本地中断状态，并在作用域内关闭中断。
     * @note 析构恢复构造前状态，支持同一 CPU 上的嵌套 guard。
     */
    class irq_guard final {
    public:
        irq_guard() noexcept;
        ~irq_guard() noexcept;

        irq_guard(const irq_guard &)            = delete;
        irq_guard &operator=(const irq_guard &) = delete;
        irq_guard(irq_guard &&)                 = delete;
        irq_guard &operator=(irq_guard &&)      = delete;

    private:
        xlen_t previous_ = 0;
    };

    struct TrapFrame;

    /** @brief 架构 trap 解码后的通用原因分类。 */
    enum class TrapKind : u8_t {
        SYNCHRONOUS,
        TIMER,
        SOFTWARE,
        EXTERNAL,
    };

    /** @brief 从架构 TrapFrame 提取、供通用 dispatcher 使用的 trap 元数据。 */
    struct TrapInfo {
        TrapKind kind;
        xlen_t raw_cause;
        xlen_t code;
        addr_t bad_address;
        bool user;
        memory::FaultAccess access = memory::FaultAccess::NONE;
    };

    /** @brief 关闭当前 CPU 的可屏蔽中断。 */
    void cli() noexcept;

    /** @brief 打开当前 CPU 的可屏蔽中断。 */
    void sti() noexcept;

    /** @brief 安装启动期 panic-only 异常向量。 */
    void bind_cpu_local() noexcept;
    void set_early_vectors() noexcept;

    /** @brief 安装能够返回并接入通用 dispatcher 的运行期异常向量。 */
    void set_trap_vectors() noexcept;

    /** @brief 将架构 TrapFrame 中的 cause/status 解码为 TrapInfo。 */
    [[nodiscard]] TrapInfo decode_trap(const TrapFrame &frame) noexcept;

    /** @brief 判断 trap 是否来自用户态上下文。 */
    [[nodiscard]] bool from_user(const TrapFrame &frame) noexcept;

    /** @brief 读取 TrapFrame 保存的异常返回程序计数器。 */
    [[nodiscard]] addr_t pc(const TrapFrame &frame) noexcept;

    /** @brief 修改 TrapFrame 的异常返回程序计数器。 */
    void set_pc(TrapFrame &frame, addr_t value) noexcept;

    /** @brief 判断同步异常是否为用户态系统调用。 */
    [[nodiscard]] bool is_user_syscall(const TrapInfo &info) noexcept;

    /** @brief 判断同步异常是否为页故障。 */
    [[nodiscard]] bool is_page_fault(const TrapInfo &info) noexcept;

    /** @brief 读取系统调用号和前两个参数。 */
    [[nodiscard]] xlen_t syscall_nr(const TrapFrame &frame) noexcept;
    [[nodiscard]] xlen_t syscall_arg(const TrapFrame &frame, size_t index) noexcept;

    /** @brief 写入系统调用返回值并前移到下一条用户指令。 */
    void set_syscall_ret(TrapFrame &frame, xlen_t value) noexcept;
    void advance_syscall(TrapFrame &frame) noexcept;

    /** @brief 返回用于通用 fatal 诊断的架构异常名称和可选 subcode。 */
    [[nodiscard]] const char *trap_name(const TrapInfo &info) noexcept;
    [[nodiscard]] xlen_t trap_subcode(const TrapInfo &info) noexcept;

    /** @brief 构造能够返回用户态的初始 TrapFrame。 */
    void init_user_frame(TrapFrame &frame, addr_t entry, addr_t stack_pointer,
                         addr_t argument) noexcept;

    /** @brief 从内核栈恢复 TrapFrame 并首次返回用户态。 */
    [[noreturn]] void enter_user(TrapFrame &frame, addr_t kernel_stack_top) noexcept;

    /** @brief 等待当前 CPU 的下一次可处理事件。 */
    void wfi() noexcept;
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
