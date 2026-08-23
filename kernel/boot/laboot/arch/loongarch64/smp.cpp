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
#include <arch/loongarch64/valdef.h>
#include <boot/smp.h>
#include <sustcore/addrspace.h>

#include <atomic>
#include <cstddef>

extern "C" char laboot_secondary_trampoline[];

namespace boot::smp {
    using namespace hal;
    struct ApStartupData {
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
        addr_t entry_pc;
    };

    static_assert(offsetof(ApStartupData, pwctl0) == 0);
    static_assert(offsetof(ApStartupData, pwctl1) == 8);
    static_assert(offsetof(ApStartupData, stlbpgsize) == 16);
    static_assert(offsetof(ApStartupData, pgdl) == 24);
    static_assert(offsetof(ApStartupData, pgdh) == 32);
    static_assert(offsetof(ApStartupData, dmw0) == 40);
    static_assert(offsetof(ApStartupData, dmw1) == 48);
    static_assert(offsetof(ApStartupData, dmw2) == 56);
    static_assert(offsetof(ApStartupData, dmw3) == 64);
    static_assert(offsetof(ApStartupData, tlbrentry) == 72);
    static_assert(offsetof(ApStartupData, stack_top) == 80);
    static_assert(offsetof(ApStartupData, cpu_local) == 88);
    static_assert(offsetof(ApStartupData, arguments) == 96);
    static_assert(offsetof(ApStartupData, entry_pc) == 104);
    static_assert(sizeof(ApStartupData) == 112);
    static_assert(alignof(ApStartupData) <= 16);
    static_assert(sizeof(ApStartupData) <= AP_ARCH_DATA_SIZE);

    [[nodiscard]] addr_t kernel_physical(const void *ptr) noexcept {
        return reinterpret_cast<addr_t>(ptr) - KVA_START;
    }

    void send_mail(u64_t cpu, u64_t mailbox, u64_t value) noexcept {
        const u64_t command = IOCSR_MBUF_SEND_BLOCKING | (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
        const u64_t box_hi  = ((mailbox * 2 + 1) << IOCSR_MBUF_SEND_BOX_SHIFT);
        const u64_t box_lo  = ((mailbox * 2) << IOCSR_MBUF_SEND_BOX_SHIFT);
        csr::iocsr_write64(IOCSR_MBUF_SEND, command | box_hi | (value & 0xffffffff00000000ULL));
        csr::iocsr_write64(IOCSR_MBUF_SEND, command | box_lo | (value << 32));
    }

    bool supports_ap_start() noexcept {
        // LABOOT/QEMU starts an AP through IOCSR MBUF0 and IPI action zero.
        // This hand-off is a boot-protocol contract, rather than an FDT CPU
        // enable-method, so its absence from the DTB is expected.
        return true;
    }

    tay::expected<void, tay::error_code> start_ap(cpu::CpuHwId hw_id,
                                                  PhyAddr arguments_physical) noexcept {
        if (hw_id.value > 0xffffU || arguments_physical.arith() == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        // The common layer stores arguments in permanent kernel BSS. That
        // range intentionally has no KPA alias in the final kernel page table,
        // so the BSP must inspect it through its high-half mapping.
        auto *resources = reinterpret_cast<ApBootRes *>(arguments_physical.arith() + KVA_START);
        const auto *arguments = &resources->arguments;
        const auto index      = arguments->cpu_id.value;
        if (index == 0 || index >= cpu::MAX_CPUS || arguments->magic != AP_BOOT_MAGIC ||
            arguments->abi_version != AP_BOOT_ABI_VERSION)
        {
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }

        auto *data             = reinterpret_cast<ApStartupData *>(resources->arch_data);
        // 恒等 trampoline 映射安装在 KernelVm 的永久 root；BSP 当前 PGDL 可能是
        // kernel guard_root，不能直接复制给尚未进入 C++ 的 AP。过渡阶段让低、高地址都从
        // 同一个 kernel root 查找，进入 ap_main() 后再安装正式绑定。
        const auto kernel_root = arguments->root_pt.arith();
        const ApStartupData startup_data{
            .pwctl0     = csr::read<csr::CSR::PWCTL0>(),
            .pwctl1     = csr::read<csr::CSR::PWCTL1>(),
            .stlbpgsize = csr::read<csr::CSR::STLBPGSIZE>(),
            .pgdl       = kernel_root,
            .pgdh       = kernel_root,
            .dmw0       = csr::read<csr::CSR::DMWIN0>(),
            .dmw1       = csr::read<csr::CSR::DMWIN1>(),
            .dmw2       = csr::read<csr::CSR::DMWIN2>(),
            .dmw3       = csr::read<csr::CSR::DMWIN3>(),
            .tlbrentry  = csr::read<csr::CSR::TLBRENTRY>(),
            .stack_top  = arguments->stack_top,
            .cpu_local  = arguments->cpu_local,
            .arguments  = reinterpret_cast<addr_t>(arguments),
            .entry_pc   = arguments->entry_pc,
        };
        *data = startup_data;
        std::atomic_thread_fence(std::memory_order_release);

        const auto target = static_cast<u64_t>(hw_id.value);
        // The AP enters MBUF0 before it has established paging. The final
        // page table retains an identity mapping for this trampoline page,
        // allowing execution to continue across the CRMD.PG transition.
        // MBUF1 is likewise physical because it is read before that point.
        send_mail(target, 0, 0);
        send_mail(target, 0, kernel_physical(laboot_secondary_trampoline));
        send_mail(target, 1, kernel_physical(data));
        const auto command =
            static_cast<u32_t>(IOCSR_IPI_SEND_BLOCKING | (target << IOCSR_IPI_SEND_CPU_SHIFT));
        csr::iocsr_write32(IOCSR_IPI_SEND, command);
        return {};
    }

    PhyAddr trampoline_page() noexcept {
        return PhyAddr(kernel_physical(laboot_secondary_trampoline) & ~addr_t{PAGE_SIZE - 1});
    }
}  // namespace boot::smp
