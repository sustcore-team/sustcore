/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ec_write 用户指针校验、按页预取和早期控制台输出。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#if defined(__ARCH_RISCV64__)
#include <arch/csr.h>
#endif
#include <log.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>
#include <syscall.h>

#include <cstdint>
#include <limits>

namespace kernel::syscall {
    namespace {
#if defined(__ARCH_RISCV64__)
        /**
         * @brief 临时允许 S-mode 读取 U-mode 页。
         *
         * RISC-V 的 SUM 位默认关闭，内核完成地址空间缺页处理后仍不能
         * 直接解引用用户指针。guard 只覆盖 syscall 的受检读取区间，退出
         * 时恢复进入 syscall 前的状态，避免把权限泄漏到后续内核代码。
         */
        class user_access_guard final {
        public:
            user_access_guard() noexcept
                : previous_(riscv64::hal::csr::set_bits<riscv64::hal::csr::CSR::SSTATUS>(xlen_t{1}
                                                                                         << 18)) {}

            ~user_access_guard() noexcept {
                constexpr xlen_t SUM = xlen_t{1} << 18;
                if ((previous_ & SUM) == 0)
                    (void)riscv64::hal::csr::clear_bits<riscv64::hal::csr::CSR::SSTATUS>(SUM);
            }

            user_access_guard(const user_access_guard &)            = delete;
            user_access_guard &operator=(const user_access_guard &) = delete;

        private:
            xlen_t previous_ = 0;
        };
#else
        class user_access_guard final {
        public:
            user_access_guard() noexcept                            = default;
            ~user_access_guard() noexcept                           = default;
            user_access_guard(const user_access_guard &)            = delete;
            user_access_guard &operator=(const user_access_guard &) = delete;
        };
#endif
    }  // namespace

    tay::expected<size_t, tay::error_code> ec_write(const char *data, size_t length) noexcept {
        if (length == 0)
            return size_t{0};
        if (data == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto *thread = scheduler::instance().current();
        if (thread == nullptr || thread->process().kernel() ||
            thread->process().address_space() == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        const addr_t start = reinterpret_cast<addr_t>(data);
        if (length > UINT64_MAX - start)
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        const addr_t end = start + length;
        if (start >= KPA_START || end > KPA_START)
            return tay::Err(tay::error_code::OUT_OF_RANGE);

        auto *space = thread->process().address_space();
        for (addr_t page = page_align_down(start); page < end; page += PAGE_SIZE) {
            auto address = VirAddr::try_from(page);
            if (!address)
                return tay::Err(address.error());
            auto mapped = space->handle_page_fault(*address, memory::FaultAccess::READ);
            if (!mapped)
                return tay::Err(mapped.error());
            if (page > UINT64_MAX - PAGE_SIZE)
                break;
        }

        user_access_guard access_guard;
        for (size_t index = 0; index < length; ++index) kernel::log::putc(data[index]);
        return length;
    }

    void yield() noexcept {
        scheduler::yield();
    }
}  // namespace kernel::syscall
