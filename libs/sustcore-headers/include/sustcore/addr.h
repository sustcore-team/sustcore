/**
 * @file addr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Sustcore 内核使用的物理、虚拟和通用地址类型。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addrspace.h>
#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/format.h>
#include <tay/range.h>

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>

#include "tay/utility.h"

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

enum class AddrType : u8_t {
    PA,   // 低半区物理地址
    VA,   // 低半区虚拟地址
    HVA,  // 高半区地址（包含 KVA 和 KPA）
    KVA,  // 内核虚拟地址
    KPA,  // 内核物理地址
};

using addrscope = tay::range<addr_t>;

// tay::range 使用半开区间；高半区的闭上界 MAX_ADDR 由 in_addr_scope() 精确处理。
constexpr addrscope PA_SCOPE  = {NULL_ADDR, KPA_START};
constexpr addrscope VA_SCOPE  = PA_SCOPE;
constexpr addrscope HVA_SCOPE = {KPA_START, MAX_ADDR};
constexpr addrscope KPA_SCOPE = {KPA_START, KVA_START};
constexpr addrscope KVA_SCOPE = {KVA_START, MAX_ADDR};

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
        case AddrType::PA:  return PA_SCOPE;
        case AddrType::VA:  return VA_SCOPE;
        case AddrType::HVA: return HVA_SCOPE;
        case AddrType::KVA: return KVA_SCOPE;
        case AddrType::KPA: return KPA_SCOPE;
    }
    return {NULL_ADDR, NULL_ADDR};
}

/**
 * @brief 判断地址是否属于 AddrType 的完整逻辑范围。
 * @note 不能只调用 within(get_scope(...))：range 的 end 为排他边界，无法表达 HVA/KVA
 *       包含 MAX_ADDR 的闭区间。
 */
constexpr bool in_addr_scope(AddrType type, addr_t address) noexcept {
    switch (type) {
        case AddrType::PA:
        case AddrType::VA:  return address < KPA_START;
        case AddrType::HVA: return address >= KPA_START;
        case AddrType::KVA: return address >= KVA_START;
        case AddrType::KPA: return address >= KPA_START && address < KVA_START;
    }
    return false;
}

template <AddrType Type>
class Addr {
private:
    addr_t _addr = 0;

public:
    explicit constexpr Addr(addr_t addr) : _addr(addr) {
        assert(in_addr_scope(Type, addr));
    }
    constexpr Addr() = default;
    explicit inline Addr(void *addr) : Addr((addr_t)addr) {}

    /** @brief 从原始整数地址构造；地址不属于 Type 的区域时返回 OUT_OF_RANGE。 */
    [[nodiscard]] static constexpr tay::expected<Addr, tay::error_code> try_from(
        addr_t addr) noexcept {
        if (!in_addr_scope(Type, addr))
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        return Addr(addr);
    }

    /** @brief 从指针构造；判定规则与整数地址版本一致。 */
    [[nodiscard]] static inline tay::expected<Addr, tay::error_code> try_from(void *addr) noexcept {
        return try_from(reinterpret_cast<addr_t>(addr));
    }

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
    constexpr std::strong_ordering operator<=>(const Addr &other) const noexcept {
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
using VirAddr = Addr<AddrType::VA>;
using HvaAddr = Addr<AddrType::HVA>;
using KpaAddr = Addr<AddrType::KPA>;
using KvaAddr = Addr<AddrType::KVA>;

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
        if (in_addr_scope(type, (addr_t)ptr)) {
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
    return in_addr_scope(AddrType::VA, vaddr.arith());
}

using VirArea = tay::range<VirAddr>;
using PhyArea = tay::range<PhyAddr>;

template <typename AddrT>
inline static tay::range<AddrT> page_align_inward(tay::range<AddrT> area) {
    return {area.begin.page_align_up(), area.end.page_align_down()};
}

template <typename AddrT>
inline static tay::range<AddrT> page_align_outward(tay::range<AddrT> area) {
    return {area.begin.page_align_down(), area.end.page_align_up()};
}

namespace tay {
    template <AddrType Type>
    struct formatter<Addr<Type>> {
        constexpr format_parse_context::iterator parse(format_parse_context &context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const Addr<Type> &addr,
                                                FormatContext &context) const {
            context.write("Addr<");
            if constexpr (Type == AddrType::PA) {
                context.write("PA");
            } else if constexpr (Type == AddrType::VA) {
                context.write("VA");
            } else if constexpr (Type == AddrType::HVA) {
                context.write("HVA");
            } else if constexpr (Type == AddrType::KVA) {
                context.write("KVA");
            } else if constexpr (Type == AddrType::KPA) {
                context.write("KPA");
            } else {
                static_assert(tay::dependent_false_v<Addr<Type>>, "Unsupported AddrType");
            }
            context.format(">{:#016x}", addr.arith());
            return context.out();
        }
    };

    template <AddrType Type>
    struct formatter<tay::range<Addr<Type>>> {
        constexpr format_parse_context::iterator parse(format_parse_context &context) noexcept {
            return context.begin();
        }  // namespace tay

        template <class FormatContext>
        typename FormatContext::iterator format(const tay::range<Addr<Type>> &area,
                                                FormatContext &context) const {
            context.write("Range<");
            if constexpr (Type == AddrType::PA) {
                context.write("PA");
            } else if constexpr (Type == AddrType::VA) {
                context.write("VA");
            } else if constexpr (Type == AddrType::HVA) {
                context.write("HVA");
            } else if constexpr (Type == AddrType::KVA) {
                context.write("KVA");
            } else if constexpr (Type == AddrType::KPA) {
                context.write("KPA");
            } else {
                static_assert(tay::dependent_false_v<Addr<Type>>, "Unsupported AddrType");
            }
            context.format(">[{:#016x}, {:#016x})", area.begin.arith(), area.end.arith());
            return context.out();
        }
    };
}  // namespace tay
