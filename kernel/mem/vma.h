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
            case Type::CODE:      return PageMan::rwx(true, false, true);
            case Type::DATA:
            case Type::STACK:
            case Type::HEAP:
            case Type::SHARE_RW:  return PageMan::rwx(true, true, false);
            case Type::SHARE_RO:  return PageMan::rwx(true, false, false);
            case Type::SHARE_RX:  return PageMan::rwx(true, false, true);
            case Type::SHARE_RWX: return PageMan::rwx(true, true, true);
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

    /**
     * @brief 判断该VMA是否能够通过TM::map_vma映射到页表中
     *
     * 仅有CODE, DATA类型的VMA允许这么做, 因为程序加载时,
     * 需要让CODE与DATA段的VMA映射 到内存中实际存储程序代码与数据的页上
     *
     * @param type
     * @return true
     * @return false
     */
    static constexpr bool mappable(Type type) {
        switch (type) {
            case Type::CODE:
            case Type::DATA: return true;
            default:         return false;
        }
    }

    TM *tm;
    Type type;
    VirAddr vaddr;
    size_t size;
    util::ListHead<VMA> list_head;

    constexpr VMA()
        : tm(nullptr),
          type(Type::NONE),
          vaddr(),
          size(0),
          list_head({}) {}
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

class TM {
    util::IntrusiveList<VMA> vma_list;
    PageMan __pgd;
public:
    TM();
    ~TM();

    void add_vma(VMA::Type type, VirAddr vaddr, size_t size);
    void clone_vma(TM &other, VMA *vma);
    VMA *find_vma(VirAddr vaddr);
    void remove_vma(VMA *vma);
    void map_vma(VMA *vma, PhyAddr paddr, size_t size);

    constexpr PageMan &pgd() {
        return __pgd;
    }
};