/**
 * @file usrboot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 校验内嵌 usrboot 并创建首个用户 Process/Thread。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <init/usrboot_error.h>
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

        [[nodiscard]] tay::expected<void, UsrbootError> validate_segment(
            UsrbootError::Segment kind, const usrboot_segment &segment,
            size_t image_size) noexcept {
            u64_t end = 0;
            if (segment.memsz == 0 || segment.filesz > segment.memsz)
                return tay::Err(
                    UsrbootError::InvalidSegmentSize(kind, segment.memsz, segment.filesz));

            if (add_overflow(segment.vaddr, segment.memsz, end))
                return tay::Err(
                    UsrbootError::SegmentAddressOverflow(kind, segment.vaddr, segment.memsz));

            if (segment.vaddr >= KPA_START || end > KPA_START)
                return tay::Err(UsrbootError::SegmentOutsideUserRange(kind, segment.vaddr, end));

            if (segment.off < sizeof(usrboot_header) || segment.off > image_size ||
                segment.filesz > image_size - segment.off)
                return tay::Err(UsrbootError::SegmentFileRangeInvalid(kind, segment.off,
                                                                      segment.filesz, image_size));

            static_assert(is_pow2(PAGE_SIZE), "PAGE_SIZE 必须为 2 的幂");

            if ((segment.vaddr & (PAGE_SIZE - 1)) != 0)
                return tay::Err(UsrbootError::SegmentUnaligned(kind, segment.vaddr));

            return {};
        }

    }  // namespace

    tay::expected<void, UsrbootError> start_usrboot() noexcept {
        const auto *begin = s_usrboot;
        const auto *end   = e_usrboot;
        const size_t size = static_cast<size_t>(end - begin);
        if (size < sizeof(usrboot_header))
            return tay::Err(UsrbootError::ImageTooSmall(size, sizeof(usrboot_header)));

        usrboot_header header{};
        __builtin_memcpy(&header, begin, sizeof(header));
        if (header.magic != USRBOOT_MAGIC)
            return tay::Err(UsrbootError::InvalidMagic(header.magic));
        if (header.body_size != size - sizeof(header) || header.entry == 0)
            return tay::Err(UsrbootError::InvalidHeader(size, header.body_size, header.entry));
        kernel::log::info("usrboot header: size={}, rx_off={}, rw_off={}, ro_off={}", size,
                          header.seg_rx.off, header.seg_rw.off, header.seg_ro.off);
        TAY_TRYV(validate_segment(UsrbootError::Segment::RX, header.seg_rx, size));
        TAY_TRYV(validate_segment(UsrbootError::Segment::RW, header.seg_rw, size));
        TAY_TRYV(validate_segment(UsrbootError::Segment::RO, header.seg_ro, size));

        auto address_space = task::AddressSpace::create();
        if (!address_space)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::ADDRESS_SPACE,
                                                               address_space.error().code()));
        auto process = task::Process::create();
        if (!process)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::PROCESS,
                                                               process.error().code()));
        auto cspace = cap::CSpace::create();
        if (!cspace)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::CSPACE,
                                                               cspace.error().code()));
        if (auto configured = (*process)->set_address_space(**address_space); !configured)
            return tay::Err(UsrbootError::ProcessConfigurationFailed(configured.error().code()));
        if (auto configured = (*process)->set_cspace(**cspace); !configured)
            return tay::Err(UsrbootError::ProcessConfigurationFailed(configured.error().code()));

        auto rx = memory::MemorySegment::create(header.seg_rx.memsz);
        auto rw = memory::MemorySegment::create(header.seg_rw.memsz);
        auto ro = memory::MemorySegment::create(header.seg_ro.memsz);
        if (!rx)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::MEMORY_SEGMENT,
                                                               rx.error().code()));
        if (!rw)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::MEMORY_SEGMENT,
                                                               rw.error().code()));
        if (!ro)
            return tay::Err(UsrbootError::ObjectCreationFailed(UsrbootError::Object::MEMORY_SEGMENT,
                                                               ro.error().code()));
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
        if (!rx_vma)
            return tay::Err(
                UsrbootError::VmaCreationFailed(UsrbootError::Segment::RX, rx_vma.error().code()));
        if (!rw_vma)
            return tay::Err(
                UsrbootError::VmaCreationFailed(UsrbootError::Segment::RW, rw_vma.error().code()));
        if (!ro_vma)
            return tay::Err(
                UsrbootError::VmaCreationFailed(UsrbootError::Segment::RO, ro_vma.error().code()));
        if (auto written = (*rx)->write(0, begin + header.seg_rx.off, header.seg_rx.filesz);
            !written)
            return tay::Err(UsrbootError::SegmentWriteFailed(UsrbootError::Segment::RX, 0,
                                                             written.error().code()));
        if (auto written = (*rw)->write(0, begin + header.seg_rw.off, header.seg_rw.filesz);
            !written)
            return tay::Err(UsrbootError::SegmentWriteFailed(UsrbootError::Segment::RW, 0,
                                                             written.error().code()));
        if (auto written = (*ro)->write(0, begin + header.seg_ro.off, header.seg_ro.filesz);
            !written)
            return tay::Err(UsrbootError::SegmentWriteFailed(UsrbootError::Segment::RO, 0,
                                                             written.error().code()));

        constexpr addr_t STACK_TOP   = 0x0000000000800000ULL;
        constexpr size_t STACK_BYTES = 64 * 1024;
        auto stack                   = memory::MemorySegment::create(STACK_BYTES);
        if (!stack)
            return tay::Err(UsrbootError::InitialStackFailed(stack.error().code()));
        auto stack_vma =
            (*address_space)
                ->add_vma(cap::CapabilityRef<memory::MemorySegment>(*stack, segment_rights),
                          VirArea{VirAddr(STACK_TOP - STACK_BYTES), VirAddr(STACK_TOP)}, 0,
                          memory::PageFlags{.readable = true, .writable = true});
        if (!stack_vma)
            return tay::Err(UsrbootError::VmaCreationFailed(UsrbootError::Segment::STACK,
                                                            stack_vma.error().code()));
        // 首次进入用户态前物化栈顶页，使初始 SP 可立即使用。
        if (auto page = (*stack)->ensure_page(STACK_BYTES - PAGE_SIZE); !page)
            return tay::Err(UsrbootError::InitialStackFailed(page.error().code()));

        auto thread = task::Thread::create_user(**process);
        if (!thread)
            return tay::Err(UsrbootError::ThreadCreationFailed(thread.error().code()));
        if (auto result = (*thread)->configure_user(header.entry, STACK_TOP - 16); !result)
            return tay::Err(UsrbootError::UserContextConfigurationFailed(result.error().code()));
        if (auto result = (*process)->submit(); !result)
            return tay::Err(UsrbootError::ProcessSubmissionFailed(result.error().code()));
        if (auto result = scheduler::instance().attach(**thread); !result) {
            return tay::Err(UsrbootError::ThreadAttachFailed(result.error().code()));
        }
        kernel::log::info("usrboot 已装载并提交首个用户 Thread");
        return {};
    }
}  // namespace init
