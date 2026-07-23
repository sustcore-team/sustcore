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
#include <sus/range.h>
#include <sustcore/addrspace.h>

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>

static constexpr bool is_pow2(size_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

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

using addrscope = util::range<addr_t>;

constexpr addrscope KVA_SCOPE   = {KVA_START, MAX_ADDR};
constexpr addrscope KPA_SCOPE   = {KPA_START, KVA_START - 1};
constexpr addrscope PA_SCOPE    = {NULL_ADDR, KPA_START - 1};
constexpr addrscope VADDR_SCOPE = {NULL_ADDR, MAX_ADDR};

constexpr addr_t KVA2PA(addr_t ka) {
    return ka - KVA_START;
}

constexpr addr_t PA2KVA(addr_t pa) {
    return pa + KVA_START;
}

constexpr addr_t KPA2PA(addr_t ka) {
    return ka - KPA_START;
}

constexpr addr_t PA2KPA(addr_t pa) {
    return pa + KPA_START;
}

constexpr addrscope get_scope(AddrType type) {
    switch (type) {
        case AddrType::KVA:   return KVA_SCOPE;
        case AddrType::KPA:   return KPA_SCOPE;
        case AddrType::PA:    return PA_SCOPE;
        case AddrType::VADDR: return VADDR_SCOPE;
        default:              return {NULL_ADDR, NULL_ADDR};  // Invalid scope
    }
}

template <AddrType Type>
class Addr {
private:
    addr_t _addr = 0;
public:
    explicit constexpr Addr(addr_t addr) : _addr(addr) {
        assert(within(get_scope(Type), addr));
    }
    constexpr Addr() = default;
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
using VirAddr = Addr<AddrType::VADDR>;

template <AddrType Type>
constexpr Addr<Type> Addr<Type>::null = Addr();

template <typename AddrT>
constexpr AddrT convert(PhyAddr pa) {
    if (!pa.nonnull()) {
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
    if (!kpa.nonnull()) {
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
    if (!kva.nonnull()) {
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

template <typename T>
inline static PhyAddr convert_pointer(T *ptr) {
    AddrType types[]       = {AddrType::PA, AddrType::KPA, AddrType::KVA};
    AddrType detected_type = AddrType::PA;  // Default to PA if no match
    for (AddrType type : types) {
        if (within(get_scope(type), (addr_t)ptr)) {
            detected_type = type;
            break;
        }
    }
    if (detected_type == AddrType::PA) {
        return convert<PhyAddr>(PhyAddr((addr_t)ptr));
    } else if (detected_type == AddrType::KPA) {
        return convert<PhyAddr>(KpaAddr((addr_t)ptr));
    } else if (detected_type == AddrType::KVA) {
        return convert<PhyAddr>(KvaAddr((addr_t)ptr));
    } else {
        assert(false && "Pointer is out of known address scopes");
        return PhyAddr::null;  // Unreachable, but silences compiler warning
    }
}

constexpr bool is_user_vaddr(VirAddr vaddr) {
    return within(get_scope(AddrType::VADDR), vaddr.arith()) &&
           !within(get_scope(AddrType::KVA), vaddr.arith()) &&
           !within(get_scope(AddrType::KPA), vaddr.arith());
}

using VirArea = util::range<VirAddr>;
using PhyArea = util::range<PhyAddr>;

template <typename AddrT>
inline static util::range<AddrT> page_align_inward(util::range<AddrT> area)
{
    return {
        area.begin.page_align_up(),
        area.end.page_align_down()
    };
}

template <typename AddrT>
inline static util::range<AddrT> page_align_outward(util::range<AddrT> area)
{
    return {
        area.begin.page_align_down(),
        area.end.page_align_up()
    };
}