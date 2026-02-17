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
#include <cstddef>

extern addr_t g_kva_offset;
constexpr addr_t KVA_OFFSET = 0xFFFF'FFFF'0000'0000ULL;
static inline addr_t KVA2PA(addr_t ka) {
    return ka - g_kva_offset;
}
static inline addr_t PA2KVA(addr_t pa) {
    return pa + g_kva_offset;
}

extern addr_t g_kpa_offset;
constexpr addr_t KPA_OFFSET = 0xFFFF'FFC0'0000'0000ULL;

static inline addr_t KPA2PA(addr_t ka) {
    return ka - g_kpa_offset;
}

static inline addr_t PA2KPA(addr_t pa) {
    return pa + g_kpa_offset;
}

static inline void *KPA2PA(void *ka) {
    return (void *)KPA2PA((addr_t)ka);
}

static inline void *PA2KPA(void *pa) {
    return (void *)PA2KPA((addr_t)pa);
}

static inline void *KVA2PA(void *ka) {
    return (void *)KVA2PA((addr_t)ka);
}

static inline void *PA2KVA(void *pa) {
    return (void *)PA2KVA((addr_t)pa);
}

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
    KVA,  // 内核虚拟地址
    KPA,  // 内核物理地址
    PA    // 物理地址
};

struct AddrScope {
    addr_t start;
    addr_t end;
};

constexpr AddrScope KVA_SCOPE = {KVA_OFFSET, 0xFFFF'FFFF'FFFF'FFFFULL};
constexpr AddrScope KPA_SCOPE = {KPA_OFFSET, KVA_OFFSET - 1};
constexpr AddrScope PA_SCOPE  = {0, KPA_OFFSET - 1};

constexpr bool within_scope(addr_t addr, AddrType type) {
    switch (type) {
        case AddrType::KVA:
            return addr >= KVA_SCOPE.start && addr < KVA_SCOPE.end;
        case AddrType::KPA:
            return addr >= KPA_SCOPE.start && addr < KPA_SCOPE.end;
        case AddrType::PA:
            return addr >= PA_SCOPE.start && addr < PA_SCOPE.end;
        default:
            return false;
    }
}

// class Addr {
// protected:
//     umb_t _addr;

// public:
//     static const Addr null;
//     explicit constexpr Addr(umb_t addr = 0) : _addr(addr) {}
//     explicit inline Addr(void *addr) : _addr((umb_t)addr) {}
//     inline void *addr() const noexcept {
//         return (void *)this->_addr;
//     }
//     inline umb_t arith() const noexcept {
//         return this->_addr;
//     }
//     template <typename T>
//     inline T *as() const noexcept {
//         return (T *)this->addr();
//     }
//     inline bool valid() const noexcept {
//         return this->_addr != 0;
//     }
//     template <size_t alignment>
//     inline bool aligned() const noexcept {
//         static_assert(is_pow2(alignment), "Alignment must be a power of two");
//         return (this->_addr % alignment) == 0;
//     }
//     constexpr bool aligned(size_t alignment) const noexcept {
//         assert(is_pow2(alignment));
//         return (this->_addr % alignment) == 0;
//     }

//     inline operator bool() const noexcept {
//         return valid();
//     }

//     inline bool operator==(const Addr &other) const noexcept {
//         return this->_addr == other._addr;
//     }
//     inline bool operator!=(const Addr &other) const noexcept {
//         return this->_addr != other._addr;
//     }

//     inline std::strong_ordering operator<=>(const Addr &other) const noexcept {
//         return this->_addr <=> other._addr;
//     }

//     inline size_t operator-(const Addr &other) const noexcept {
//         return this->_addr - other._addr;
//     }

//     inline Addr operator+(size_t offset) const noexcept {
//         return Addr(this->_addr + offset);
//     }

//     inline Addr operator-(size_t offset) const noexcept {
//         return Addr(this->_addr - offset);
//     }

//     inline Addr &operator+=(size_t offset) noexcept {
//         this->_addr = this->_addr + offset;
//         return *this;
//     }

//     inline Addr &operator-=(size_t offset) noexcept {
//         this->_addr = this->_addr - offset;
//         return *this;
//     }

//     inline Addr &operator=(const Addr &other) noexcept {
//         this->_addr = other._addr;
//         return *this;
//     }
// };

// static inline Addr KPA2PA(Addr ka) {
//     return Addr(KPA2PA(ka.arith()));
// }

// static inline Addr PA2KPA(Addr pa) {
//     return Addr(PA2KPA(pa.arith()));
// }

// // 向上对齐到页边界
// inline Addr page_align_up(Addr addr) {
//     return Addr(page_align_up(addr.arith()));
// }

// // 向下对齐到页边界
// inline Addr page_align_down(Addr addr) {
//     return Addr(page_align_down(addr.arith()));
// }

// static constexpr umb_t convert_pa(umb_t addr) {
//     if (addr < KPA_OFFSET) {
//         // 还没有进入内核虚拟地址空间，直接返回物理地址
//         return addr;
//     } else if (addr < KVA_OFFSET) {
//         // 内核物理地址空间，转换为物理地址
//         return KPA2PA(addr);
//     } else {
//         // 内核虚拟地址空间，转换为物理地址
//         return KVA2PA(addr);
//     }
// }

// /**
//  * @brief 物理地址类型
//  * 该类型封装了物理地址的相关操作, 包括地址转换和比较等.
//  * 同时进一步地, 使用这个类型将强迫你在使用物理地址时显式地进行转换,
//  * 避免混淆物理地址和内核虚拟地址. 该类型的本质就是一个 void * 指针,
//  * 因此是一个zero-cost的抽象, 但它提供了更安全和清晰的接口来处理物理地址.
//  */
// class PhyAddr {
// protected:
//     umb_t _paddr;

// public:
//     static const PhyAddr null;
//     explicit constexpr PhyAddr(umb_t paddr = 0) : _paddr(convert_pa(paddr)) {}
//     explicit inline PhyAddr(Addr paddr) : PhyAddr(paddr.arith()) {}
//     explicit inline PhyAddr(void *paddr) : PhyAddr((umb_t)paddr) {}
//     inline Addr paddr() const noexcept {
//         return Addr(this->_paddr);
//     }
//     inline Addr kaddr() const noexcept {
//         if (this->_paddr == 0)
//             return Addr();
//         return Addr(PA2KPA(this->_paddr));
//     }
//     inline void *ppaddr() const noexcept {
//         return (void *)this->_paddr;
//     }
//     inline void *pkaddr() const noexcept {
//         if (this->_paddr == 0)
//             return nullptr;
//         return (void *)PA2KPA(this->_paddr);
//     }
//     // arithmetic physical/kernel physical address
//     inline umb_t apaddr() const noexcept {
//         return this->paddr().arith();
//     }
//     inline umb_t akaddr() const noexcept {
//         return this->kaddr().arith();
//     }
//     template <size_t alignment>
//     inline bool aligned() const noexcept {
//         static_assert(is_pow2(alignment), "Alignment must be a power of two");
//         return (this->_paddr % alignment) == 0;
//     }
//     constexpr bool aligned(size_t alignment) const noexcept {
//         assert(is_pow2(alignment));
//         return (this->_paddr % alignment) == 0;
//     }
//     template <typename T>
//     inline T *as() const noexcept {
//         return (T *)this->pkaddr();
//     }
//     inline bool valid() const noexcept {
//         return this->_paddr != 0;
//     }
//     inline operator bool() const noexcept {
//         return valid();
//     }

//     inline bool operator==(const PhyAddr &other) const noexcept {
//         return this->_paddr == other._paddr;
//     }
//     inline bool operator!=(const PhyAddr &other) const noexcept {
//         return this->_paddr != other._paddr;
//     }

//     inline std::strong_ordering operator<=>(
//         const PhyAddr &other) const noexcept {
//         return this->_paddr <=> other._paddr;
//     }

//     inline size_t operator-(const PhyAddr &other) const noexcept {
//         return this->_paddr - other._paddr;
//     }

//     inline PhyAddr operator+(size_t offset) const noexcept {
//         return PhyAddr(this->_paddr + offset);
//     }

//     inline PhyAddr operator-(size_t offset) const noexcept {
//         return PhyAddr(this->_paddr - offset);
//     }

//     inline PhyAddr &operator+=(size_t offset) noexcept {
//         this->_paddr = this->_paddr + offset;
//         return *this;
//     }

//     inline PhyAddr &operator-=(size_t offset) noexcept {
//         this->_paddr = this->_paddr - offset;
//         return *this;
//     }
// };

// inline constexpr Addr Addr::null       = Addr();
// inline constexpr PhyAddr PhyAddr::null = PhyAddr();

// // 向上对齐到页边界
// inline PhyAddr page_align_up(PhyAddr addr) {
//     return PhyAddr(page_align_up(addr.apaddr()));
// }

// // 向下对齐到页边界
// inline PhyAddr page_align_down(PhyAddr addr) {
//     return PhyAddr(page_align_down(addr.apaddr()));
// }