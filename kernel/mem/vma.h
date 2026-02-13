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
        MMAP      = 5,
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
            case Type::MMAP:
            case Type::SHARE_RW:  return PageMan::rwx(true, true, false);
            case Type::SHARE_RO:  return PageMan::rwx(true, false, false);
            case Type::SHARE_RX:  return PageMan::rwx(true, false, true);
            case Type::SHARE_RWX: return PageMan::rwx(true, true, true);
            default:              return PageMan::RWX::NONE;
        }
    }

    TM *tm;
    Type type;
    void *vaddr;
    size_t size;
    util::ListHead<VMA> list_head;

    constexpr VMA()
        : tm(nullptr), type(Type::NONE), vaddr(nullptr), size(0), list_head({}) {}
    constexpr VMA(TM *tm, Type t, void *v, size_t s)
        : tm(tm), type(t), vaddr(v), size(s), list_head({}) {}
    constexpr VMA(TM *tm, const VMA &other)
        : tm(tm),
          type(other.type),
          vaddr(other.vaddr),
          size(other.size),
          list_head({}) {}
    constexpr VMA(VMA &&other) = delete;
};

class TM {
    util::IntrusiveList<VMA> vma_list;
    PageMan __pgd;

public:
    TM();
    ~TM();

    void add_vma(VMA::Type type, void *vaddr, size_t size);
    void clone_vma(TM &other, VMA *vma);
    VMA *find_vma(void *vaddr);
    void remove_vma(VMA *vma);

    constexpr PageMan &pgd() {
        return __pgd;
    }
};