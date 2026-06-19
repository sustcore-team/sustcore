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

#include <fwd.h>
#include <arch/description.h>
#include <object/memory.h>
#include <sus/list.h>
#include <sus/nonnull.h>
#include <sus/range.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/epacks.h>
#include <sustcore/errcode.h>

#include <cassert>

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

    using Growth = cap::MemoryGrowth;

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

    static constexpr bool cowable(Type type) {
        switch (type) {
            case Type::CODE:
            case Type::DATA:
            case Type::STACK:
            case Type::HEAP:  return true;
            default:          return false;
        }
    }

    Type type                  = Type::NONE;
    Growth growth              = Growth::FIXED;
    TaskMemoryManager *tm      = nullptr;
    /// 该 VMA 关联的 Memory payload. 所有 VMA 都必须持有有效 Memory. 
    cap::MemoryPayload *memory = nullptr;
    /// 该 VMA 映射时使用的页权限. 
    PageMan::RWX rwx           = PageMan::RWX::NONE;
    /// VMA 起始地址对应的 Memory 内偏移. 
    size_t mem_offset          = 0;
    VirArea varea;
    bool loading =
        false;  // 是否正在加载 (如ELF加载）, 用于处理缺页异常时区分是正常访问还是加载过程中访问
    util::ListHead<VMA> list_head = {};

    constexpr VMA() = default;
    /**
     * @brief 构造一个 Memory-backed VMA. 
     *
     * 构造时会增加 memory 的引用计数, 析构时释放引用. 
     *
     * @param tm 所属任务内存管理器. 
     * @param t VMA 类型. 
     * @param g VMA 增长方式. 
     * @param varea 虚拟地址范围. 
     * @param memory 关联 Memory payload. 
     * @param rwx 页权限. 
     * @param mem_offset VMA 起点对应的 Memory 内偏移. 
     */
    VMA(TaskMemoryManager *tm, Type t, Growth g, const VirArea &varea,
        cap::MemoryPayload *memory, PageMan::RWX rwx, size_t mem_offset)
        : type(t),
          growth(g),
          tm(tm),
          memory(memory),
          rwx(rwx),
          mem_offset(mem_offset),
          varea(varea),
          list_head({}) {
        assert(memory != nullptr);
        memory->keep();
    }
    /**
     * @brief 从已有 VMA 克隆元数据并绑定到指定 Memory. 
     *
     * 用于 fork/COW 复制 VMA. 构造时会增加 memory 引用计数. 
     */
    VMA(TaskMemoryManager *tm, Growth g, const VMA &other,
        cap::MemoryPayload *memory)
        : type(other.type),
          growth(g),
          tm(tm),
          memory(memory),
          rwx(other.rwx),
          mem_offset(other.mem_offset),
          varea(other.varea),
          loading(other.loading),
          list_head({}) {
        assert(memory != nullptr);
        memory->keep();
    }
    constexpr VMA(VMA &&other) = delete;
    /**
     * @brief 析构 VMA 并释放对 Memory payload 的引用. 
     */
    ~VMA() {
        if (memory != nullptr) {
            memory->release();
        }
    }

    [[nodiscard]]
    constexpr size_t size() const {
        return varea.size();
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
    struct ExistingPgdTag {};
    util::IntrusiveList<VMA> vma_list;
    PhyAddr _pgd;
    PageMan _pman;

    Result<VMA *> __check_vma(const util::nonnull<VMA *> &vma) {
        if (vma->tm != this) {
            return std::unexpected(ErrCode::INVALID_PARAM);
        }
        return vma.get();
    }

    void unmap_pages(const VirArea &varea);
    Result<void> clone_vma_pages_to_cow(const VMA &vma, const VirArea &map_area,
                                        TaskMemoryManager &dst);

public:
    TaskMemoryManager(PhyAddr _pgd);
    TaskMemoryManager(ExistingPgdTag, PhyAddr _pgd);
    [[nodiscard]]
    static Result<util::owner<TaskMemoryManager *>> from_existing_pgd(
        PhyAddr pgd) noexcept;
    ~TaskMemoryManager();

    /**
     * @brief 创建一个 Memory-backed VMA. 
     *
     * 所有 VMA 都必须关联 Memory payload. 该函数只建立虚拟区域与
     * Memory 的关系, 不立即分配物理页; 物理页在缺页异常时懒分配. 
     *
     * @param type VMA 类型. 
     * @param growth VMA 增长方式. 
     * @param varea 虚拟地址范围. 
     * @param memory 关联 Memory payload. 
     * @param rwx 页权限. 
     * @param mem_offset VMA 起点对应的 Memory 内偏移. 
     * @return 新建 VMA. 
     */
    Result<util::nonnull<VMA *>> add_vma(VMA::Type type, VMA::Growth growth,
                                         const VirArea &varea,
                                         cap::MemoryPayload *memory,
                                         PageMan::RWX rwx,
                                         size_t mem_offset = 0);
    Result<util::nonnull<VMA *>> locate(VirAddr vaddr);
    Result<util::nonnull<VMA *>> locate_range(const VirArea &varea);
    /**
     * @brief 定位指定 Memory 在某虚拟地址处的 VMA. 
     *
     * @param memory 关联的 Memory payload. 
     * @param vaddr 虚拟地址. 
     * @return 匹配的 VMA. 
     */
    Result<util::nonnull<VMA *>> locate_memory(cap::MemoryPayload *memory,
                                               VirAddr vaddr);

    Result<util::nonnull<VMA *>> clone_vma(util::nonnull<VMA *> vma,
                                           TaskMemoryManager &dst);
    /**
     * @brief 移除 VMA 并解除页表映射. 
     *
     * 不释放 Memory payload 持有的物理页; 物理页由 Memory payload
     * 生命周期管理. 
     */
    Result<void> remove_vma(util::nonnull<VMA *> vma);
    Result<VirArea> grow_vma(util::nonnull<VMA *> vma, const VirArea &varea);
    /**
     * @brief 查询当前地址空间是否仍有 VMA 引用指定 Memory. 
     */
    bool has_memory_mapping(cap::MemoryPayload *memory) const;
    /**
     * @brief 将指定 Memory 的当前映射设置为 COW 写保护. 
     *
     * 用于 clone 非 shared Memory capability 后保护父进程已有映射. 
     */
    Result<void> protect_memory_cow(cap::MemoryPayload *memory);
    /**
     * @brief 在 Memory 收缩时解除超出新大小的映射. 
     *
     * @param memory 被收缩的 Memory payload. 
     * @param new_size 新 Memory 大小. 
     */
    void unmap_memory_tail(cap::MemoryPayload *memory, size_t new_size);
    /**
     * @brief 根据 Memory 当前大小同步其关联 VMA 范围. 
     */
    Result<void> sync_memory_vmas(cap::MemoryPayload *memory);
    /**
     * @brief 在目标地址空间中查找源 Memory 对应的克隆 Memory. 
     *
     * fork 时 VMA 和 Memory payload 分别克隆; 复制 capability 表时使用该
     * 函数把子进程 cap 指向已经由 VMA 持有的子 Memory. 
     *
     * @param source 父地址空间中的 Memory payload. 
     * @param dst 子任务内存管理器. 
     * @return 子地址空间中对应的 Memory payload. 
     */
    Result<cap::MemoryPayload *> cloned_memory_for(
        cap::MemoryPayload *source, TaskMemoryManager &dst) const;

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
    // write protection
    bool on_wp(VirAddr fault_addr);
    Result<void> clone_to_cow(TaskMemoryManager &dst);
};

// TODO: 这两个值应当是架构相关的
// 但是我实在懒得管了, 遇到再说吧
constexpr static VirAddr USER_STACK_TOP =
    VirAddr(0x4000000000);  // 初始栈顶地址
constexpr static size_t MAX_INITIAL_STACK_SIZE =
    0x10000000;  // 初始栈最大大小(256MB)
constexpr static VirAddr USER_STACK_BOTTOM =
    USER_STACK_TOP - MAX_INITIAL_STACK_SIZE;  // 初始栈底地址
