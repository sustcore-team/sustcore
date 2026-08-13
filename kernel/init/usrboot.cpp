/**
 * @file usrboot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 校验内嵌 usrboot 并创建首个用户 Process/Thread。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/virtual/page_flags.h>
#include <obj/address_space.h>
#include <obj/memory_segment.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <usrboot.h>

#include <cstddef>
#include <cstdint>

extern "C" const std::byte s_usrboot[];
extern "C" const std::byte e_usrboot[];

namespace init {
    namespace {
        [[nodiscard]] bool add_overflow(u64_t lhs, u64_t rhs, u64_t &result) noexcept {
            if (rhs > UINT64_MAX - lhs)
                return true;
            result = lhs + rhs;
            return false;
        }

        [[nodiscard]] tay::expected<void, tay::error_code> validate_segment(
            const usrboot_segment &segment, size_t image_size) noexcept {
            u64_t end = 0;
            if (segment.memsz == 0 || segment.filesz > segment.memsz) {
                kernel::log::error("usrboot 段大小无效: memsz={}, filesz={}", segment.memsz,
                                   segment.filesz);
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }

            if (add_overflow(segment.vaddr, segment.memsz, end)) {
                kernel::log::error("usrboot 段地址溢出: vaddr={}, memsz={}", segment.vaddr,
                                   segment.memsz);
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }

            if (segment.vaddr >= KPA_START || end > KPA_START) {
                kernel::log::error("usrboot 段地址越界: vaddr={}, end={}", segment.vaddr, end);
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }

            if (segment.off < sizeof(usrboot_header) || segment.off > image_size ||
                segment.filesz > image_size - segment.off)
            {
                kernel::log::error("usrboot 段文件偏移无效: off={}, filesz={}, image_size={}",
                                   segment.off, segment.filesz, image_size);
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }

            static_assert(is_pow2(PAGE_SIZE), "PAGE_SIZE 必须为 2 的幂");

            if ((segment.vaddr & (PAGE_SIZE - 1)) != 0) {
                kernel::log::error("usrboot 段地址未对齐: vaddr={}, PAGE_SIZE={}", segment.vaddr,
                                   PAGE_SIZE);
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }

            return {};
        }

    }  // namespace

    tay::expected<void, tay::error_code> start_usrboot() noexcept {
        const auto *begin = s_usrboot;
        const auto *end   = e_usrboot;
        const size_t size = static_cast<size_t>(end - begin);
        if (size < sizeof(usrboot_header)) {
            kernel::log::error("usrboot 镜像过小: {} 字节", size);
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }

        usrboot_header header{};
        __builtin_memcpy(&header, begin, sizeof(header));
        if (header.magic != USRBOOT_MAGIC) {
            kernel::log::error("usrboot 镜像魔数错误");
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        if (header.body_size != size - sizeof(header) || header.entry == 0) {
            kernel::log::error("usrboot 镜像格式无效");
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        kernel::log::info("usrboot header: size={}, rx_off={}, rw_off={}, ro_off={}", size,
                          header.seg_rx.off, header.seg_rw.off, header.seg_ro.off);
        auto result = validate_segment(header.seg_rx, size);
        if (!result) {
            kernel::log::error("usrboot RX 段校验失败: {}", tay::to_string(result.error()));
            return tay::Err(result.error());
        }
        result = validate_segment(header.seg_rw, size);
        if (!result) {
            kernel::log::error("usrboot RW 段校验失败: {}", tay::to_string(result.error()));
            return tay::Err(result.error());
        }
        result = validate_segment(header.seg_ro, size);
        if (!result) {
            kernel::log::error("usrboot RO 段校验失败: {}", tay::to_string(result.error()));
            return tay::Err(result.error());
        }

        auto address_space = task::AddressSpace::create();
        auto process       = task::Process::create();
        auto cspace        = cap::CSpace::create();
        if (!address_space || !process || !cspace) {
            kernel::log::error("usrboot 对象创建失败: as={}, process={}, cspace={}",
                               address_space ? 1 : 0, process ? 1 : 0, cspace ? 1 : 0);
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        }
        if (auto result = (*process)->set_address_space(**address_space); !result) {
            kernel::log::error("绑定 AddressSpace 失败");
            return tay::Err(result.error());
        }
        if (auto result = (*process)->set_cspace(**cspace); !result) {
            kernel::log::error("绑定 CSpace 失败");
            return tay::Err(result.error());
        }

        auto rx = memory::MemorySegment::create(header.seg_rx.memsz);
        auto rw = memory::MemorySegment::create(header.seg_rw.memsz);
        auto ro = memory::MemorySegment::create(header.seg_ro.memsz);
        if (!rx || !rw || !ro)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        const auto segment_rights = static_cast<u64_t>(cap::RIGHT_READ | cap::RIGHT_WRITE);
        const auto rx_bytes       = page_align_up(header.seg_rx.memsz);
        const auto rw_bytes       = page_align_up(header.seg_rw.memsz);
        const auto ro_bytes       = page_align_up(header.seg_ro.memsz);
        auto rx_vma               = (*address_space)
                          ->add_vma(cap::CapabilityRef<memory::MemorySegment>(*rx, segment_rights),
                                    VirArea{VirAddr(header.seg_rx.vaddr),
                                            VirAddr(header.seg_rx.vaddr + rx_bytes)},
                                    0, memory::PageFlags{.readable = true, .executable = true});
        auto rw_vma = (*address_space)
                          ->add_vma(cap::CapabilityRef<memory::MemorySegment>(*rw, segment_rights),
                                    VirArea{VirAddr(header.seg_rw.vaddr),
                                            VirAddr(header.seg_rw.vaddr + rw_bytes)},
                                    0, memory::PageFlags{.readable = true, .writable = true});
        auto ro_vma = (*address_space)
                          ->add_vma(cap::CapabilityRef<memory::MemorySegment>(*ro, segment_rights),
                                    VirArea{VirAddr(header.seg_ro.vaddr),
                                            VirAddr(header.seg_ro.vaddr + ro_bytes)},
                                    0, memory::PageFlags{.readable = true});
        if (!rx_vma || !rw_vma || !ro_vma)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (auto written = (*rx)->write(0, begin + header.seg_rx.off, header.seg_rx.filesz);
            !written)
            return tay::Err(written.error());
        if (auto written = (*rw)->write(0, begin + header.seg_rw.off, header.seg_rw.filesz);
            !written)
            return tay::Err(written.error());
        if (auto written = (*ro)->write(0, begin + header.seg_ro.off, header.seg_ro.filesz);
            !written)
            return tay::Err(written.error());

        constexpr addr_t STACK_TOP   = 0x0000000000800000ULL;
        constexpr size_t STACK_BYTES = 64 * 1024;
        auto stack                   = memory::MemorySegment::create(STACK_BYTES);
        if (!stack)
            return tay::Err(stack.error());
        auto stack_vma =
            (*address_space)
                ->add_vma(cap::CapabilityRef<memory::MemorySegment>(*stack, segment_rights),
                          VirArea{VirAddr(STACK_TOP - STACK_BYTES), VirAddr(STACK_TOP)}, 0,
                          memory::PageFlags{.readable = true, .writable = true});
        if (!stack_vma)
            return tay::Err(stack_vma.error());
        // 首次进入用户态前物化栈顶页，使初始 SP 可立即使用。
        if (auto page = (*stack)->ensure_page(STACK_BYTES - PAGE_SIZE); !page)
            return tay::Err(page.error());

        auto thread = task::Thread::create_user(**process);
        if (!thread)
            return tay::Err(thread.error());
        if (auto result = (*thread)->configure_user(header.entry, STACK_TOP - 16); !result)
            return tay::Err(result.error());
        if (auto result = (*process)->submit(); !result)
            return tay::Err(result.error());
        if (auto result = scheduler::instance().resume(**thread); !result)
            return tay::Err(result.error());
        kernel::log::info("usrboot 已装载并提交首个用户 Thread");
        return {};
    }
}  // namespace init
