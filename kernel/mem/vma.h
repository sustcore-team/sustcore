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
#include <sustcore/addr.h>
#include <sus/list.h>
#include <sus/nonnull.h>
#include <sus/range.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/epacks.h>
#include <sustcore/errcode.h>

using util::range::Range;

class TaskMemoryManager;

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

    TaskMemoryManager *tm         = nullptr;
    Type type      = Type::NONE;
    VirAddr vstart = VirAddr::null;
    VirAddr vend   = VirAddr::null;
    bool loading =
        false;  // 是否正在加载（如ELF加载），用于处理缺页异常时区分是正常访问还是加载过程中访问
    util::ListHead<VMA> list_head = {};

    constexpr VMA() = default;
    constexpr VMA(TaskMemoryManager *tm, Type t, Range<VirAddr> varea)
        : tm(tm),
          type(t),
          vstart(varea.begin),
          vend(varea.end),
          list_head({}) {}
    constexpr VMA(TaskMemoryManager *tm, const VMA &other)
        : tm(tm),
          type(other.type),
          vstart(other.vstart),
          vend(other.vend),
          list_head({}) {}
    constexpr VMA(VMA &&other) = delete;

    [[nodiscard]]
    constexpr size_t size() const {
        return vend - vstart;
    }
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
class TaskMemoryManager {
private:
    util::IntrusiveList<VMA> vma_list;
    PhyAddr _pgd;
    PageMan _pman;

public:
    TaskMemoryManager(PhyAddr _pgd);
    ~TaskMemoryManager();

    Result<util::nonnull<VMA *>> add_vma(VMA::Type type, VirAddr vstart,
                                         VirAddr vend);
    Result<util::nonnull<VMA *>> clone_vma(TaskMemoryManager &other, VirAddr vma_addr);
    Result<util::nonnull<VMA *>> locate(VirAddr vaddr);
    Result<util::nonnull<VMA *>> locate_range(VirAddr vaddr, size_t size);
    Result<void> remove_vma(VirAddr vma_addr);

    [[nodiscard]]
    constexpr const util::IntrusiveList<VMA> &vmas() const {
        return vma_list;
    }

    [[nodiscard]]
    constexpr util::IntrusiveList<VMA> &vmas() {
        return vma_list;
    }

    [[nodiscard]]
    constexpr PhyAddr pgd() const {
        return _pgd;
    }

    [[nodiscard]]
    constexpr PageMan &pman() {
        return _pman;
    }

    // On No Present Pages
    bool on_np(const NoPresentEvent &e);
};

// TODO: 这两个值应当是架构相关的
// 但是我实在懒得管了, 遇到再说吧
constexpr static VirAddr USER_STACK_TOP =
    VirAddr(0x4000000000);  // 初始栈顶地址
constexpr static size_t MAX_INITIAL_STACK_SIZE =
    0x10000000;  // 初始栈最大大小(256MB)
constexpr static VirAddr USER_STACK_BOTTOM =
    USER_STACK_TOP - MAX_INITIAL_STACK_SIZE;  // 初始栈底地址