/**
 * @file paging.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V Sv39 页表编码、激活与本地失效策略
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/csr.h>
#include <arch/riscv64/paging.h>
#include <memory/virtual/kernel/kernel_space.h>
#include <sustcore/addrspace.h>

namespace riscv64::hal {
    constexpr u64_t PTE_VALID    = u64_t{1} << 0;
    constexpr u64_t PTE_READ     = u64_t{1} << 1;
    constexpr u64_t PTE_WRITE    = u64_t{1} << 2;
    constexpr u64_t PTE_EXECUTE  = u64_t{1} << 3;
    constexpr u64_t PTE_USER     = u64_t{1} << 4;
    constexpr u64_t PTE_GLOBAL   = u64_t{1} << 5;
    constexpr u64_t PTE_ACCESSED = u64_t{1} << 6;
    constexpr u64_t PTE_DIRTY    = u64_t{1} << 7;

    PageTableOps::EntryType *PageTableOps::table(PhyAddr physical) noexcept {
        return reinterpret_cast<EntryType *>(PA2KPA(physical.arith()));
    }

    PageTableOps::EntryType PageTableOps::load_entry(const EntryType *entry) noexcept {
        return __atomic_load_n(entry, __ATOMIC_ACQUIRE);
    }

    void PageTableOps::store_leaf(EntryType *entry, EntryType value) noexcept {
        __atomic_store_n(entry, value, __ATOMIC_RELEASE);
    }

    void PageTableOps::publish_table(EntryType *entry, EntryType value) noexcept {
        __atomic_store_n(entry, value, __ATOMIC_RELEASE);
    }

    bool PageTableOps::present(EntryType entry) noexcept {
        return (entry & PTE_VALID) != 0;
    }
    bool PageTableOps::leaf(EntryType entry) noexcept {
        return (entry & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) != 0;
    }
    PhyAddr PageTableOps::next_table(EntryType entry) noexcept {
        return PhyAddr((entry >> 10) << 12);
    }
    PageTableOps::EntryType PageTableOps::make_table(PhyAddr physical) noexcept {
        return (physical.arith() >> 2) | PTE_VALID;
    }

    tay::expected<PageTableOps::EntryType, tay::error_code> PageTableOps::make_leaf(
        PhyAddr physical, const memory::PageFlags &flags) noexcept {
        if (!physical.aligned<PAGE_SIZE>() || flags.cache != memory::CacheMode::NORMAL ||
            (flags.writable && !flags.readable))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        EntryType bits = PTE_VALID | PTE_ACCESSED;
        if (flags.readable)
            bits |= PTE_READ;
        if (flags.writable)
            bits |= PTE_WRITE | PTE_DIRTY;
        if (flags.executable)
            bits |= PTE_EXECUTE;
        if (flags.user)
            bits |= PTE_USER;
        if (flags.global)
            bits |= PTE_GLOBAL;
        return (physical.arith() >> 2) | bits;
    }

    memory::PageFlags PageTableOps::decode_flags(EntryType entry) noexcept {
        return memory::PageFlags{
            .readable   = (entry & PTE_READ) != 0,
            .writable   = (entry & PTE_WRITE) != 0,
            .executable = (entry & PTE_EXECUTE) != 0,
            .user       = (entry & PTE_USER) != 0,
            .global     = (entry & PTE_GLOBAL) != 0,
            .cache      = memory::CacheMode::NORMAL,
        };
    }

    PhyAddr PageTableOps::leaf_physical(EntryType entry, addr_t address, size_t level) noexcept {
        const addr_t offset_mask = (addr_t{1} << (12 + level * 9)) - 1;
        return PhyAddr(next_table(entry).arith() | (address & offset_mask));
    }

    bool PageTableOps::canonical(addr_t address) noexcept {
        const addr_t upper = address >> 39;
        return upper == 0 || upper == ((addr_t{1} << 25) - 1);
    }

    void PageTableOps::activate_binding(const memory::RootBinding &binding) noexcept {
        constexpr xlen_t SV39 = xlen_t{8} << 60;
        const PhyAddr root    = binding.role == memory::RootRole::KERNEL
                                    ? memory::kernel_space().root()
                                    : binding.client_root;
        csr::write<csr::CSR::SATP>(SV39 | (static_cast<xlen_t>(binding.asid) << 44) |
                                   (root.arith() >> 12));
        flush_tlb();
    }

    void PageTableOps::flush_tlb() noexcept {
        asm volatile("sfence.vma zero, zero" ::: "memory");
    }

}  // namespace riscv64::hal
