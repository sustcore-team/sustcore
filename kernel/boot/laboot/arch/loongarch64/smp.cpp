/**
 * @file smp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LABOOT LoongArch 次级 CPU 启动后端
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/loongarch64/csrdef.h>
#include <boot/smp.h>
#include <cpu/smp.h>
#include <cpu/storage.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <atomic>
#include <cstddef>

extern "C" char laboot_secondary_trampoline[];

namespace loongarch64::boot::smp {
    struct SecondaryStartupData {
        xlen_t pwctl0;
        xlen_t pwctl1;
        xlen_t stlbpgsize;
        xlen_t pgdl;
        xlen_t pgdh;
        xlen_t dmw0;
        xlen_t dmw1;
        xlen_t dmw2;
        xlen_t dmw3;
        xlen_t tlbrentry;
        addr_t stack_top;
        addr_t cpu_local;
        addr_t arguments;
        addr_t entry;
    };

    static_assert(offsetof(SecondaryStartupData, pwctl0) == 0);
    static_assert(offsetof(SecondaryStartupData, pwctl1) == 8);
    static_assert(offsetof(SecondaryStartupData, stlbpgsize) == 16);
    static_assert(offsetof(SecondaryStartupData, pgdl) == 24);
    static_assert(offsetof(SecondaryStartupData, pgdh) == 32);
    static_assert(offsetof(SecondaryStartupData, dmw0) == 40);
    static_assert(offsetof(SecondaryStartupData, dmw1) == 48);
    static_assert(offsetof(SecondaryStartupData, dmw2) == 56);
    static_assert(offsetof(SecondaryStartupData, dmw3) == 64);
    static_assert(offsetof(SecondaryStartupData, tlbrentry) == 72);
    static_assert(offsetof(SecondaryStartupData, stack_top) == 80);
    static_assert(offsetof(SecondaryStartupData, cpu_local) == 88);
    static_assert(offsetof(SecondaryStartupData, arguments) == 96);
    static_assert(offsetof(SecondaryStartupData, entry) == 104);
    static_assert(sizeof(SecondaryStartupData) == 112);
    static_assert(alignof(SecondaryStartupData) <= 16);
    static_assert(sizeof(SecondaryStartupData) <= cpu::SECONDARY_ARCH_DATA_SZ);

    [[nodiscard]] addr_t kernel_physical(const void *ptr) noexcept {
        return reinterpret_cast<addr_t>(ptr) - KVA_START;
    }

    void send_mail(u64_t cpu, u64_t mailbox, u64_t value) noexcept {
        const u64_t command = IOCSR_MBUF_SEND_BLOCKING | (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
        const u64_t box_hi  = ((mailbox * 2 + 1) << IOCSR_MBUF_SEND_BOX_SHIFT);
        const u64_t box_lo  = ((mailbox * 2) << IOCSR_MBUF_SEND_BOX_SHIFT);
        hal::csr::iocsr_write64(IOCSR_MBUF_SEND,
                                command | box_hi | (value & 0xffffffff00000000ULL));
        hal::csr::iocsr_write64(IOCSR_MBUF_SEND, command | box_lo | (value << 32));
    }

    bool supports_secondary_start() noexcept {
        // LABOOT/QEMU starts an AP through IOCSR MBUF0 and IPI action zero.
        // This hand-off is a boot-protocol contract, rather than an FDT CPU
        // enable-method, so its absence from the DTB is expected.
        return true;
    }

    tay::expected<void, tay::error_code> start_secondary(cpu::cpu_hwid_t hardware_id,
                                                         PhyAddr arguments_physical) noexcept {
        if (hardware_id.value > 0xffffU || arguments_physical.arith() == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        // The common layer stores arguments in permanent kernel BSS. That
        // range intentionally has no KPA alias in the final kernel page table,
        // so the BSP must inspect it through its high-half mapping.
        auto *arguments = reinterpret_cast<const cpu::SecondaryBootArgs *>(
            arguments_physical.arith() + KVA_START);
        const auto index = arguments->cpu_id.value;
        if (index == 0 || index >= cpu::MAX_CPUS || arguments->magic != cpu::SECONDARY_BOOT_MAGIC) {
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }

        auto &resources = cpu::storage_for(cpu::cpu_id_t{static_cast<u32_t>(index)}).secondary;
        auto *data      = reinterpret_cast<SecondaryStartupData *>(
            static_cast<void *>(resources.architecture_data));
        *data = SecondaryStartupData{
            .pwctl0     = hal::csr::read<hal::csr::CSR::PWCTL0>(),
            .pwctl1     = hal::csr::read<hal::csr::CSR::PWCTL1>(),
            .stlbpgsize = hal::csr::read<hal::csr::CSR::STLBPGSIZE>(),
            .pgdl       = hal::csr::read<hal::csr::CSR::PGDL>(),
            .pgdh       = hal::csr::read<hal::csr::CSR::PGDH>(),
            .dmw0       = hal::csr::read<hal::csr::CSR::DMWIN0>(),
            .dmw1       = hal::csr::read<hal::csr::CSR::DMWIN1>(),
            .dmw2       = hal::csr::read<hal::csr::CSR::DMWIN2>(),
            .dmw3       = hal::csr::read<hal::csr::CSR::DMWIN3>(),
            .tlbrentry  = hal::csr::read<hal::csr::CSR::TLBRENTRY>(),
            .stack_top  = arguments->stack_top,
            .cpu_local  = arguments->cpu_local,
            .arguments  = reinterpret_cast<addr_t>(arguments),
            .entry      = arguments->entry,
        };
        std::atomic_thread_fence(std::memory_order_release);

        const auto target = static_cast<u64_t>(hardware_id.value);
        // The AP enters MBUF0 before it has established paging. The final
        // page table retains an identity mapping for this trampoline page,
        // allowing execution to continue across the CRMD.PG transition.
        // MBUF1 is likewise physical because it is read before that point.
        send_mail(target, 0, 0);
        send_mail(target, 0, kernel_physical(laboot_secondary_trampoline));
        send_mail(target, 1, kernel_physical(data));
        const auto command =
            static_cast<u32_t>(IOCSR_IPI_SEND_BLOCKING | (target << IOCSR_IPI_SEND_CPU_SHIFT));
        hal::csr::iocsr_write32(IOCSR_IPI_SEND, command);
        return {};
    }

    PhyAddr identity_trampoline_page() noexcept {
        return PhyAddr(kernel_physical(laboot_secondary_trampoline) & ~addr_t{PAGE_SIZE - 1});
    }
}  // namespace loongarch64::boot::smp
