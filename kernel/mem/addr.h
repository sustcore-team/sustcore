/**
 * @file addr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核虚拟地址
 * @version alpha-1.0.0
 * @date 2026-01-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>

static constexpr bool is_pow2(size_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

constexpr size_t PAGESIZE = 0x1000;  // 4KB

// 向上对齐到页边界
constexpr addr_t page_align_up(addr_t addr_val) {
    return (addr_val + 0xFFF) & ~0xFFF;
}

// 向下对齐到页边界
constexpr addr_t page_align_down(addr_t addr_val) {
    return addr_val & ~0xFFF;
}

enum class AddrType {
    KVA,    // 内核虚拟地址
    KPA,    // 内核物理地址
    PA,     // 物理地址
    VADDR,  // 任意地址类型
};

struct AddrScope {
    addr_t start;
    addr_t end;
};

constexpr addr_t NULL_ADDR  = 0;
constexpr addr_t MAX_ADDR   = 0xFFFF'FFFF'FFFF'FFFFULL;
constexpr addr_t KVA_OFFSET = 0xFFFF'FFFF'0000'0000ULL;
constexpr addr_t KPA_OFFSET = 0xFFFF'FFC0'0000'0000ULL;

constexpr AddrScope KVA_SCOPE   = {KVA_OFFSET, MAX_ADDR};
constexpr AddrScope KPA_SCOPE   = {KPA_OFFSET, KVA_OFFSET - 1};
constexpr AddrScope PA_SCOPE    = {NULL_ADDR, KPA_OFFSET - 1};
constexpr AddrScope VADDR_SCOPE = {NULL_ADDR, MAX_ADDR};

static inline addr_t KVA2PA(addr_t ka) {
    return ka - KVA_OFFSET;
}
static inline addr_t PA2KVA(addr_t pa) {
    return pa + KVA_OFFSET;
}

static inline addr_t KPA2PA(addr_t ka) {
    return ka - KPA_OFFSET;
}

static inline addr_t PA2KPA(addr_t pa) {
    return pa + KPA_OFFSET;
}

constexpr AddrScope get_scope(AddrType type) {
    switch (type) {
        case AddrType::KVA:   return KVA_SCOPE;
        case AddrType::KPA:   return KPA_SCOPE;
        case AddrType::PA:    return PA_SCOPE;
        case AddrType::VADDR: return VADDR_SCOPE;
        default:              return {NULL_ADDR, NULL_ADDR};  // Invalid scope
    }
}

constexpr bool within_scope(addr_t addr, AddrType type) {
    if (addr == 0) {
        return true;  // null address is considered valid for all types
    }
    AddrScope scope = get_scope(type);
    return (addr >= scope.start) && (addr <= scope.end);
}

template <AddrType Type>
class Addr {
protected:
    addr_t _addr;

public:
    explicit constexpr Addr(addr_t addr = 0) : _addr(addr) {
        assert(within_scope(addr, Type));
    }
    explicit inline Addr(void *addr) : Addr((addr_t)addr) {}
    static const Addr null;
    inline void *addr() const noexcept {
        return (void *)this->_addr;
    }
    constexpr addr_t arith() const noexcept {
        return this->_addr;
    }
    template <typename T>
    inline T *as() const noexcept {
        return (T *)this->addr();
    }
    constexpr bool nonnull() const noexcept {
        return this->_addr != 0;
    }
    template <size_t alignment>
    constexpr bool aligned() const noexcept {
        static_assert(is_pow2(alignment), "Alignment must be a power of two");
        return (this->_addr % alignment) == 0;
    }
    constexpr bool aligned(size_t alignment) const noexcept {
        assert(is_pow2(alignment));
        return (this->_addr % alignment) == 0;
    }

    constexpr Addr align_up(size_t alignment) const noexcept {
        assert(is_pow2(alignment));
        return Addr((this->_addr + alignment - 1) & ~(alignment - 1));
    }
    constexpr Addr align_down(size_t alignment) const noexcept {
        assert(is_pow2(alignment));
        return Addr(this->_addr & ~(alignment - 1));
    }

    constexpr Addr page_align_up() const noexcept {
        return Addr(::page_align_up(this->_addr));
    }
    constexpr Addr page_align_down() const noexcept {
        return Addr(::page_align_down(this->_addr));
    }

    constexpr operator bool() const noexcept {
        return nonnull();
    }

    constexpr bool operator==(const Addr &other) const noexcept {
        return this->_addr == other._addr;
    }
    constexpr bool operator!=(const Addr &other) const noexcept {
        return this->_addr != other._addr;
    }
    constexpr std::strong_ordering operator<=>(
        const Addr &other) const noexcept {
        return this->_addr <=> other._addr;
    }

    constexpr size_t operator-(const Addr &other) const noexcept {
        return this->_addr - other._addr;
    }
    constexpr Addr operator+(size_t offset) const noexcept {
        return Addr(this->_addr + offset);
    }
    constexpr Addr operator-(size_t offset) const noexcept {
        return Addr(this->_addr - offset);
    }
    constexpr Addr &operator+=(size_t offset) noexcept {
        this->_addr = this->_addr + offset;
        return *this;
    }
    constexpr Addr &operator-=(size_t offset) noexcept {
        this->_addr = this->_addr - offset;
        return *this;
    }
    constexpr Addr &operator=(const Addr &other) noexcept {
        this->_addr = other._addr;
        return *this;
    }
};

using PhyAddr = Addr<AddrType::PA>;
using KpaAddr = Addr<AddrType::KPA>;
using KvaAddr = Addr<AddrType::KVA>;
using VirAddr   = Addr<AddrType::VADDR>;

template <AddrType Type>
constexpr Addr<Type> Addr<Type>::null = Addr();

template <typename AddrT>
constexpr AddrT convert(PhyAddr pa) {
    if (! pa.nonnull()) {
        return AddrT::null;
    }
    if constexpr (std::same_as<AddrT, PhyAddr>) {
        return pa;
    } else if constexpr (std::same_as<AddrT, KpaAddr>) {
        return KpaAddr(PA2KPA(pa.arith()));
    } else if constexpr (std::same_as<AddrT, KvaAddr>) {
        return KvaAddr(PA2KVA(pa.arith()));
    }
}

template <typename AddrT>
constexpr AddrT convert(KpaAddr kpa) {
    if (! kpa.nonnull()) {
        return AddrT::null;
    }
    if constexpr (std::same_as<AddrT, PhyAddr>) {
        return PhyAddr(KPA2PA(kpa.arith()));
    } else if constexpr (std::same_as<AddrT, KpaAddr>) {
        return kpa;
    } else if constexpr (std::same_as<AddrT, KvaAddr>) {
        return KvaAddr(PA2KVA(KPA2PA(kpa.arith())));
    }
}

template <typename AddrT>
constexpr AddrT convert(KvaAddr kva) {
    if (! kva.nonnull()) {
        return AddrT::null;
    }
    if constexpr (std::same_as<AddrT, PhyAddr>) {
        return PhyAddr(KVA2PA(kva.arith()));
    } else if constexpr (std::same_as<AddrT, KpaAddr>) {
        return KpaAddr(PA2KPA(KVA2PA(kva.arith())));
    } else if constexpr (std::same_as<AddrT, KvaAddr>) {
        return kva;
    }
}

enum class KernelStage { PRE_INIT, POST_INIT };

template <KernelStage Stage>
struct __StageAddr;

template <>
struct __StageAddr<KernelStage::PRE_INIT> {
    using Type = PhyAddr;
};

template <>
struct __StageAddr<KernelStage::POST_INIT> {
    using Type = KpaAddr;
};

template <KernelStage Stage>
using _StageAddr = typename __StageAddr<Stage>::Type;