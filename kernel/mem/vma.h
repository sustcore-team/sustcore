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
#include <sustcore/capability.h>
#include <sustcore/epacks.h>
#include <sustcore/errcode.h>

#include <cassert>
#include <functional>
#include <vector>

struct VMA {
    enum class Type {
        NONE  = 0,
        CODE  = 1,
        DATA  = 2,
        STACK = 3,
        HEAP  = 4,
        SHARE = 5,
    };

    using Growth = cap::MemoryGrowth;
    using Prot = b64;
    static constexpr Prot PROT_R     = 0x1;
    static constexpr Prot PROT_W     = 0x2;
    static constexpr Prot PROT_X     = 0x4;
    static constexpr Prot PROT_SHARE = 0x8;

    static constexpr PageMan::RWX prot_to_rwx(Prot prot) {
        const bool readable   = (prot & PROT_R) != 0;
        const bool writable   = (prot & PROT_W) != 0;
        const bool executable = (prot & PROT_X) != 0;

        if (readable && writable && executable) {
            return PageMan::RWX::RWX;
        }
        if (readable && writable) {
            return PageMan::RWX::RW;
        }
        if (readable && executable) {
            return PageMan::RWX::RX;
        }
        if (readable) {
            return PageMan::RWX::RO;
        }
        return PageMan::RWX::NONE;
    }

    static constexpr Prot rwx_to_prot(PageMan::RWX rwx, bool shared = false) {
        Prot prot = 0;
        if (PageMan::is_readable(rwx)) {
            prot |= PROT_R;
        }
        if (PageMan::is_writable(rwx)) {
            prot |= PROT_W;
        }
        if (PageMan::is_executable(rwx)) {
            prot |= PROT_X;
        }
        if (shared) {
            prot |= PROT_SHARE;
        }
        return prot;
    }

    static constexpr bool sharable(Prot prot) {
        return (prot & PROT_SHARE) != 0;
    }

    static constexpr bool cowable(Prot prot) {
        return !sharable(prot);
    }

    Type type                  = Type::NONE;
    Growth growth              = Growth::FIXED;
    TaskMemoryManager *tm      = nullptr;
    /// 该 VMA 持有的 Memory capability. 所有 VMA 都必须持有有效 Memory.
    /// Capability 负责维护其指向的 Memory payload 生命周期.
    util::owner<cap::Capability *> memory{nullptr};
    /// 该 VMA 映射时使用的权限位掩码. 
    Prot prot                  = 0;
    /// VMA 起始地址对应的 Memory 内偏移. 
    size_t mem_offset          = 0;
    VirArea varea;
    util::ListHead<VMA> list_head = {};

    constexpr VMA() = default;
    /**
     * @brief 构造一个 Memory-backed VMA. 
     *
     * 构造时会创建 VMA 独立持有的 Memory capability, 析构时释放该 capability.
     *
     * @param tm 所属任务内存管理器. 
     * @param t VMA 类型. 
     * @param g VMA 增长方式. 
     * @param varea 虚拟地址范围. 
     * @param memory 关联 Memory payload. 
     * @param prot 权限位掩码. 
     * @param mem_offset VMA 起点对应的 Memory 内偏移. 
     */
    VMA(TaskMemoryManager *tm, Type t, Growth g, const VirArea &varea,
        cap::MemoryPayload *memory, Prot prot, size_t mem_offset)
        : type(t),
          growth(g),
          tm(tm),
          memory(util::owner(new cap::Capability(memory, 0))),
          prot(prot),
          mem_offset(mem_offset),
          varea(varea),
          list_head({}) {
        assert(memory != nullptr);
    }
    /**
     * @brief 从已有 VMA 克隆元数据并绑定到指定 Memory. 
     *
     * 用于 fork/COW 复制 VMA. 构造时会创建 VMA 独立持有的 Memory capability.
     */
    VMA(TaskMemoryManager *tm, Growth g, const VMA &other,
        cap::MemoryPayload *memory)
        : type(other.type),
          growth(g),
          tm(tm),
          memory(util::owner(new cap::Capability(memory, 0))),
          prot(other.prot),
          mem_offset(other.mem_offset),
          varea(other.varea),
          list_head({}) {
        assert(memory != nullptr);
    }
    VMA(const VMA &other)            = delete;
    VMA &operator=(const VMA &other) = delete;
    VMA(VMA &&other)                 = delete;
    VMA &operator=(VMA &&other)      = delete;
    /**
     * @brief 析构 VMA 并释放持有的 Memory capability. 
     */
    ~VMA() {
        if (memory != nullptr) {
            delete memory.get();
            memory = util::owner<cap::Capability *>(nullptr);
        }
    }

    [[nodiscard]]
    cap::MemoryPayload *memory_payload() const {
        if (memory == nullptr) {
            return nullptr;
        }
        return memory->payload_as<cap::MemoryPayload>();
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
        case VMA::Type::SHARE:     return "SHARE";
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
                                         VMA::Prot prot,
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
    Result<void> remove_vma_range(const VirArea &varea);
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

    struct VMAQueryResult {
        VMA::Type type;
        VMA::Prot prot;
        VirAddr start;
        size_t size;
        CapIdx mem_cap;
    };

    [[nodiscard]]
    Result<VMAQueryResult> query_vaddr(VirAddr vaddr, CapIdx mem_cap) const;
    [[nodiscard]]
    std::vector<VMAQueryResult> query_vspace(
        size_t offset, size_t max_entries,
        const std::function<CapIdx(const VMA &)> &mem_cap_resolver) const;

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
