/**
 * @file vma.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 虚拟内存区域
 * @version alpha-1.0.0
 * @date 2026-02-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <mem/addr.h>
#include <sus/list.h>
#include <sus/types.h>
#include <sus/nonnull.h>
#include <sustcore/epacks.h>
#include <sustcore/errcode.h>

class TM;

struct VMA {
    enum class Type {
        NONE      = 0,
        CODE      = 1,
        DATA      = 2,
        STACK     = 3,
        HEAP      = 4,
        SHARE_RW  = 6,
        SHARE_RO  = 7,
        SHARE_RX  = 8,
        SHARE_RWX = 9,
    };

    static constexpr PageMan::RWX seg2rwx(Type type) {
        switch (type) {
            case Type::CODE:      return PageMan::RWX::RX;
            case Type::DATA:
            case Type::STACK:
            case Type::HEAP:
            case Type::SHARE_RW:  return PageMan::RWX::RW;
            case Type::SHARE_RO:  return PageMan::RWX::RO;
            case Type::SHARE_RX:  return PageMan::RWX::RX;
            case Type::SHARE_RWX: return PageMan::RWX::RWX;
            default:              return PageMan::RWX::NONE;
        }
    }

    static constexpr bool sharable(Type type) {
        switch (type) {
            case Type::SHARE_RW:
            case Type::SHARE_RO:
            case Type::SHARE_RX:
            case Type::SHARE_RWX: return true;
            default:              return false;
        }
    }

    static constexpr bool growable(Type type) {
        switch (type) {
            case Type::STACK: return true;
            default:          return false;
        }
    }

    static constexpr bool cowable(Type type) {
        switch (type) {
            case Type::CODE:
            case Type::DATA:
            case Type::STACK:
            case Type::HEAP:  return true;
            default:          return false;
        }
    }

    TM *tm;
    Type type;
    VirAddr vaddr;
    size_t size;
    util::ListHead<VMA> list_head;

    constexpr VMA()
        : tm(nullptr), type(Type::NONE), vaddr(), size(0), list_head({}) {}
    constexpr VMA(TM *tm, Type t, VirAddr v, size_t s)
        : tm(tm), type(t), vaddr(v), size(s), list_head({}) {}
    constexpr VMA(TM *tm, const VMA &other)
        : tm(tm),
          type(other.type),
          vaddr(other.vaddr),
          size(other.size),
          list_head({}) {}
    constexpr VMA(VMA &&other) = delete;
};

constexpr const char *to_string(VMA::Type type) {
    switch (type) {
        case VMA::Type::NONE:      return "NONE";
        case VMA::Type::CODE:      return "CODE";
        case VMA::Type::DATA:      return "DATA";
        case VMA::Type::STACK:     return "STACK";
        case VMA::Type::HEAP:      return "HEAP";
        case VMA::Type::SHARE_RW:  return "SHARE_RW";
        case VMA::Type::SHARE_RO:  return "SHARE_RO";
        case VMA::Type::SHARE_RX:  return "SHARE_RX";
        case VMA::Type::SHARE_RWX: return "SHARE_RWX";
        default:                   return "UNKNOWN";
    }
}

// Task Memory
class TM {
    util::IntrusiveList<VMA> vma_list;
    PhyAddr _pgd;
    PageMan _pman;

public:
    TM(PhyAddr _pgd);
    ~TM();

    Result<void> add_vma(VMA::Type type, VirAddr vaddr, size_t size);
    Result<void> clone_vma(TM &other, VirAddr vma_addr);
    Result<util::nonnull<VMA *>> locate(VirAddr vaddr);
    Result<util::nonnull<VMA *>> locate_range(VirAddr vaddr, size_t size);
    Result<void> remove_vma(VirAddr vma_addr);

    constexpr PhyAddr pgd() const {
        return _pgd;
    }

    constexpr PageMan &pman() {
        return _pman;
    }

    // On No Present Pages
    bool on_np(const NoPresentEvent &e);
};