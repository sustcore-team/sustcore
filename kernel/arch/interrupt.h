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
#include <memory/virtual/page_flags.h>
#include <tay/bits.h>

#include <cstddef>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /**
     * @brief RAII 保存当前本地中断状态，并在作用域内关闭中断。
     * @note 析构恢复构造前状态，支持同一 CPU 上的嵌套 guard。
     */
    class interrupt_guard final {
    public:
        interrupt_guard() noexcept;
        ~interrupt_guard() noexcept;

        interrupt_guard(const interrupt_guard &)            = delete;
        interrupt_guard &operator=(const interrupt_guard &) = delete;
        interrupt_guard(interrupt_guard &&)                 = delete;
        interrupt_guard &operator=(interrupt_guard &&)      = delete;

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
    void disable_interrupts() noexcept;

    /** @brief 打开当前 CPU 的可屏蔽中断。 */
    void enable_interrupts() noexcept;

    /** @brief 查询当前 CPU 是否允许可屏蔽中断。 */
    [[nodiscard]] bool interrupts_enabled() noexcept;

    /** @brief 安装启动期 panic-only 异常向量。 */
    void install_early_exception_vectors() noexcept;

    /** @brief 安装能够返回并接入通用 dispatcher 的运行期异常向量。 */
    void install_runtime_exception_vectors() noexcept;

    /** @brief 将架构 TrapFrame 中的 cause/status 解码为 TrapInfo。 */
    [[nodiscard]] TrapInfo decode_trap(const TrapFrame &frame) noexcept;

    /** @brief 判断 trap 是否来自用户态上下文。 */
    [[nodiscard]] bool from_user(const TrapFrame &frame) noexcept;

    /** @brief 读取 TrapFrame 保存的异常返回程序计数器。 */
    [[nodiscard]] addr_t program_counter(const TrapFrame &frame) noexcept;

    /** @brief 修改 TrapFrame 的异常返回程序计数器。 */
    void set_program_counter(TrapFrame &frame, addr_t value) noexcept;

    /** @brief 判断同步异常是否为用户态系统调用。 */
    [[nodiscard]] bool is_user_syscall(const TrapInfo &info) noexcept;

    /** @brief 判断同步异常是否为页故障。 */
    [[nodiscard]] bool is_page_fault(const TrapInfo &info) noexcept;

    /** @brief 读取系统调用号和前两个参数。 */
    [[nodiscard]] xlen_t syscall_number(const TrapFrame &frame) noexcept;
    [[nodiscard]] xlen_t syscall_argument(const TrapFrame &frame, size_t index) noexcept;

    /** @brief 写入系统调用返回值并前移到下一条用户指令。 */
    void set_syscall_result(TrapFrame &frame, xlen_t value) noexcept;
    void advance_syscall(TrapFrame &frame) noexcept;

    /** @brief 返回用于通用 fatal 诊断的架构异常名称和可选 subcode。 */
    [[nodiscard]] const char *trap_name(const TrapInfo &info) noexcept;
    [[nodiscard]] xlen_t trap_subcode(const TrapInfo &info) noexcept;

    /** @brief 构造能够返回用户态的初始 TrapFrame。 */
    void initialize_user_frame(TrapFrame &frame, addr_t entry, addr_t stack_pointer,
                               addr_t argument) noexcept;

    /** @brief 从内核栈恢复 TrapFrame 并首次返回用户态。 */
    [[noreturn]] void enter_user(TrapFrame &frame, addr_t kernel_stack_top) noexcept;

    /** @brief 等待当前 CPU 的下一次可处理事件。 */
    void wait_for_interrupt() noexcept;
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
