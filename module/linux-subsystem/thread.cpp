/**
 * @file thread.cpp
 * @author theflysong
 * @brief linux subsystem 线程辅助接口实现
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <errno.h>
#include <logger.h>
#include <prm.h>
#include <prog.h>
#include <std/stdio.h>
#include <syscall.h>
#include <thread.h>

#include <cstddef>

namespace {
    constexpr size_t LINUXSS_THREAD_STACK_SIZE = 1UL << 20;

    [[nodiscard]]
    void *linuxss_alloc_stack(size_t size) {
        size_t old_brk = __linuxss_ss_brk;
        size_t new_brk = old_brk + size;
        if (new_brk < old_brk) {
            return nullptr;
        }
        size_t actual_brk = linuxss_brk(new_brk);
        if (actual_brk != new_brk) {
            return nullptr;
        }
        return reinterpret_cast<void *>(old_brk);
    }
}  // namespace

CapIdx linuxss_create_thread(addr_t entrypoint) {
    if (entrypoint == 0) {
        loggers::LXRT::ERROR("create thread failed: entrypoint is null");
        return cap::error;
    }

    void *stack_base = linuxss_alloc_stack(LINUXSS_THREAD_STACK_SIZE);
    if (stack_base == nullptr) {
        loggers::LXRT::ERROR("create thread failed: stack alloc failed");
        return cap::error;
    }

    auto *stack_top = reinterpret_cast<void *>(
        reinterpret_cast<addr_t>(stack_base) + LINUXSS_THREAD_STACK_SIZE);
    CapIdx tcb_cap = sys_pcb_create_thread(__prog_pcb_cap,
                                           reinterpret_cast<void (*)()>(
                                               entrypoint),
                                           stack_base,
                                           LINUXSS_THREAD_STACK_SIZE);
    if (tcb_cap == cap::null || tcb_cap == cap::error) {
        loggers::LXSC::ERROR("create thread syscall failed: entry=%p",
                             reinterpret_cast<void *>(entrypoint));
        return cap::error;
    }

    loggers::LXRT::DEBUG("create thread placeholder entry=%p stack=[%p,%p)",
                        reinterpret_cast<void *>(entrypoint), stack_base,
                        stack_top);
    // 当前占位实现不回收线程栈，后续真正支持 clone 线程后再补生命周期管理。
    return tcb_cap;
}
