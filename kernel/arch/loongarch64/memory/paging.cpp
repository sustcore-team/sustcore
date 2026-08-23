/**
 * @file paging.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 四级页表编码、激活与本地失效策略
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/csr.h>
#include <arch/loongarch64/namespace.h>
#include <arch/loongarch64/pagedef.h>
#include <arch/loongarch64/paging.h>
#include <arch/loongarch64/valdef.h>
#include <memory/virtual/kernel/vm.h>
#include <sustcore/addrspace.h>

#include <atomic>

namespace loongarch64::hal {
    namespace {
        constinit std::atomic<u64_t> tlb_flushes{0};
    }

    constexpr u64_t PTE_VALID          = LA_PAGE_VALID;
    constexpr u64_t PTE_DIRTY          = LA_PAGE_DIRTY;
    constexpr u64_t PTE_USER           = u64_t{3} << 2;
    constexpr u64_t PTE_CACHE_COHERENT = LA_PAGE_CACHE_CC;
    constexpr u64_t PTE_GLOBAL         = LA_PAGE_GLOBAL;
    constexpr u64_t PTE_PRESENT        = LA_PAGE_PRESENT;
    constexpr u64_t PTE_WRITE          = LA_PAGE_WRITE;
    constexpr u64_t PTE_MODIFIED       = LA_PAGE_MODIFIED;
    constexpr u64_t PTE_NO_READ        = u64_t{1} << 61;
    constexpr u64_t PTE_NO_EXECUTE     = u64_t{1} << 62;
    constexpr u64_t PHYSICAL_MASK      = LA_PPN_MASK;

    PtOps::EntryType *PtOps::table(PhyAddr physical) noexcept {
        return reinterpret_cast<EntryType *>(PA2KPA(physical.arith()));
    }

    PtOps::EntryType PtOps::load_entry(const EntryType *entry) noexcept {
        return __atomic_load_n(entry, __ATOMIC_ACQUIRE);
    }

    void PtOps::store_leaf(EntryType *entry, EntryType value) noexcept {
        __atomic_store_n(entry, value, __ATOMIC_RELEASE);
    }

    void PtOps::publish_table(EntryType *entry, EntryType value) noexcept {
        __atomic_store_n(entry, value, __ATOMIC_RELEASE);
    }

    bool PtOps::present(EntryType entry) noexcept {
        return entry != 0;
    }
    bool PtOps::leaf(EntryType entry) noexcept {
        return (entry & PTE_VALID) != 0;
    }
    PhyAddr PtOps::next_table(EntryType entry) noexcept {
        return PhyAddr(entry & PHYSICAL_MASK);
    }
    PtOps::EntryType PtOps::make_table(PhyAddr physical) noexcept {
        return physical.arith() & PHYSICAL_MASK;
    }

    tay::expected<PtOps::EntryType, tay::error_code> PtOps::make_leaf(
        PhyAddr physical, const memory::PageFlags &flags) noexcept {
        if (!physical.aligned<PAGE_SIZE>())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        EntryType bits = PTE_VALID | PTE_PRESENT | PTE_MODIFIED;
        if (flags.global)
            bits |= PTE_GLOBAL;
        if (flags.user)
            bits |= PTE_USER;
        if (flags.cache == memory::CacheMode::NORMAL)
            bits |= PTE_CACHE_COHERENT;
        if (flags.writable)
            bits |= PTE_WRITE | PTE_DIRTY;
        if (!flags.readable)
            bits |= PTE_NO_READ;
        if (!flags.executable)
            bits |= PTE_NO_EXECUTE;
        return (physical.arith() & PHYSICAL_MASK) | bits;
    }

    memory::PageFlags PtOps::decode_flags(EntryType entry) noexcept {
        return memory::PageFlags{
            .readable   = (entry & PTE_NO_READ) == 0,
            .writable   = (entry & PTE_WRITE) != 0,
            .executable = (entry & PTE_NO_EXECUTE) == 0,
            .user       = (entry & PTE_USER) == PTE_USER,
            .global     = (entry & PTE_GLOBAL) != 0,
            .cache      = (entry & PTE_CACHE_COHERENT) != 0 ? memory::CacheMode::NORMAL
                                                            : memory::CacheMode::DEVICE,
        };
    }

    PhyAddr PtOps::leaf_physical(EntryType entry, addr_t address, size_t level) noexcept {
        const addr_t offset_mask = (addr_t{1} << (12 + level * 9)) - 1;
        return PhyAddr((entry & PHYSICAL_MASK) | (address & offset_mask));
    }

    bool PtOps::canonical(addr_t address) noexcept {
        const addr_t upper = address >> 48;
        return upper == 0 || upper == 0xffff;
    }

    void PtOps::activate_binding(const memory::RootBinding &binding) noexcept {
        csr::write<csr::CSR::PWCTL0>(static_cast<xlen_t>(PWCTL0_4LEVEL));
        csr::write<csr::CSR::PWCTL1>(static_cast<xlen_t>(PWCTL1_4LEVEL));
        csr::write<csr::CSR::STLBPGSIZE>(static_cast<xlen_t>(STLBPGSIZE_4K));
        csr::write<csr::CSR::ASID>(static_cast<xlen_t>(binding.asid) & 0x03ffu);
        csr::write<csr::CSR::PGDL>(binding.private_root.arith());
        csr::write<csr::CSR::PGDH>(memory::kernel_vm().root().arith());
        flush_tlb();
        asm volatile("ibar 0\ndbar 0" ::: "memory");
    }

    void PtOps::flush_tlb() noexcept {
        asm volatile("invtlb 0, $zero, $zero" ::: "memory");
        tlb_flushes.fetch_add(1, std::memory_order_relaxed);
    }

    u64_t PtOps::debug_flushes() noexcept {
        return tlb_flushes.load(std::memory_order_relaxed);
    }

}  // namespace loongarch64::hal
