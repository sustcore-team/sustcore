/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构 Trait 定义
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <sus/types.h>

/**
 * @brief 架构串口 Trait
 *
 * @tparam T 架构串口类
 */
template <typename T>
concept ArchSerialTrait = requires(char ch, const char *str) {
    {
        T::serial_write_char(ch)
    } -> std::same_as<void>;
    {
        T::serial_write_string(str)
    } -> std::same_as<void>;
};

/**
 * @brief 内核启动函数
 *
 */
void kernel_setup(void);

/**
 * @brief 架构初始化 Trait
 *
 * @tparam T 架构初始化类
 */
template <typename T>
concept ArchInitializationTrait = requires() {
    {
        T::pre_init()
    } -> std::same_as<void>;
    {
        T::post_init()
    } -> std::same_as<void>;
};

/**
 * @brief 内存区域
 *
 */
struct MemRegion {
    /**
     * @brief 内存状态
     *
     */
    enum class MemoryStatus {
        FREE             = 0,
        RESERVED         = 1,
        ACPI_RECLAIMABLE = 2,
        ACPI_NVS         = 3,
        BAD_MEMORY       = 4
    };

    void *ptr;
    size_t size;
    MemoryStatus status;
};

/**
 * @brief 架构内存布局 Trait
 *
 * @tparam T 架构内存布局类
 */
template <typename T>
concept ArchMemLayoutTrait = requires(MemRegion *regions, int cnt) {
    {
        T::detect_memory_layout(regions, cnt)
    } -> std::same_as<int>;
};

// 初始化条件
template <typename T>
concept ArchPageManTrait_Initialization = requires() {
    {
        T::pre_init()
    } -> std::same_as<void>;
    {
        T::post_init()
    } -> std::same_as<void>;
};

// RWX条件
template <typename T>
concept ArchPageManTrait_RWX = requires(bool r, bool w, bool x, T::RWX rwx) {
    // RWX 枚举类型
    std::is_scoped_enum_v<typename T::RWX>;
    // 对RWX的构造
    {
        T::rwx(r, w, x)
    } -> std::same_as<typename T::RWX>;
    // 萃取RWX信息
    {
        T::is_readable(rwx)
    } -> std::convertible_to<bool>;
    {
        T::is_writable(rwx)
    } -> std::convertible_to<bool>;
    {
        T::is_executable(rwx)
    } -> std::convertible_to<bool>;
};

template <typename T>
concept ArchPageManTrait_PagingStructures =
    requires(T::PageSize size, T::ExtendedPTE ext_pte) {
        // PageSize 枚举类型
        // 要求有4K页
        std::is_scoped_enum_v<typename T::PageSize>;
        {
            T::PageSize::SIZE_4K
        } -> std::same_as<typename T::PageSize>;
        {
            T::get_size(size)
        } -> std::convertible_to<size_t>;
        // 页表项类型
        typename T::PTE;
        {
            T::PTE_CNT
        } -> std::convertible_to<size_t>;
        // ExtendedPTE
        typename T::ExtendedPTE;
        {
            ext_pte.pte
        } -> std::same_as<typename T::PTE *&>;
        {
            ext_pte.size
        } -> std::same_as<typename T::PageSize &>;
    };

template <typename T>
concept ArchPageManTrait_PTEInfoReader = requires(typename T::PTE pte) {
    {
        T::rwx(pte)
    } -> std::convertible_to<typename T::RWX>;
    {
        T::is_present(pte)
    } -> std::convertible_to<bool>;
    {
        T::is_user_accessible(pte)
    } -> std::convertible_to<bool>;
    {
        T::is_global(pte)
    } -> std::convertible_to<bool>;
    {
        T::is_valid(pte)
    } -> std::convertible_to<bool>;
    {
        T::get_physical_address(pte)
    } -> std::same_as<void *>;
    {
        T::is_dirty(pte)
    } -> std::convertible_to<bool>;
};

// 修改掩码
template <typename T>
concept ArchPageManTrait_Mask =
    requires(bool r, bool w, bool x, bool u, bool g, bool np) {
        std::is_scoped_enum_v<typename T::ModifyMask>;
        {
            T::make_mask(r, w, x, u, g, np)
        } -> std::same_as<typename T::ModifyMask>;
        {
            T::ModifyMask::NONE
        } -> std::same_as<typename T::ModifyMask>;
        {
            T::ModifyMask::RWX
        } -> std::same_as<typename T::ModifyMask>;
        {
            T::ModifyMask::ALL
        } -> std::same_as<typename T::ModifyMask>;
    };

// 页表管理器 Trait
template <typename T>
concept ArchPageManTrait = requires(T::PTE *__root, T root, size_t size,
                                    void *vaddr, void *paddr, T::RWX rwx,
                                    bool u, bool g) {
    // 初始化条件
    requires ArchPageManTrait_Initialization<T>;
    // 满足RWX条件
    requires ArchPageManTrait_RWX<T>;
    // 满足页表结构条件
    requires ArchPageManTrait_PagingStructures<T>;
    // 满足PTE信息读取条件
    requires ArchPageManTrait_PTEInfoReader<T>;
    // 获得/构造页表根
    {
        T::read_root()
    } -> std::same_as<typename T::PTE *>;
    {
        T::make_root()
    } -> std::same_as<typename T::PTE *>;
    // 构造页表管理器
    {
        T()
    } -> std::same_as<T>;
    {
        T(__root)
    } -> std::same_as<T>;
    // 查询页
    {
        root.query_page(vaddr)
    } -> std::same_as<typename T::ExtendedPTE>;
    // 单页映射
    {
        root.template map_page<T::PageSize::SIZE_4K>(vaddr, paddr, rwx, u, g)
    } -> std::same_as<void>;
    // 解除单页映射
    {
        root.unmap_page(vaddr)
    } -> std::same_as<void>;
    // 范围映射
    {
        root.template map_range<false>(vaddr, paddr, size, rwx, u, g)
    } -> std::same_as<void>;
    {
        root.template map_range<true>(vaddr, paddr, size, rwx, u, g)
    } -> std::same_as<void>;
    // 解除范围映射
    {
        root.unmap_range(vaddr, size)
    } -> std::same_as<void>;
    requires ArchPageManTrait_Mask<T>;
    // 修改页面标志
    {
        root.template modify_flags<T::ModifyMask::NONE>(vaddr, rwx, u, g)
    } -> std::same_as<void>;
    // 修改范围页面标志
    {
        root.template modify_range_flags<T::ModifyMask::NONE>(vaddr, size, rwx, u, g)
    } -> std::same_as<void>;
    // 更换页表根
    {
        T::__switch_root(__root)
    } -> std::same_as<void>;
    // 刷新TLB
    {
        T::flush_tlb()
    } -> std::same_as<void>;
    // 获得页表根
    {
        root.get_root()
    } -> std::same_as<typename T::PTE *&>;
    // 更换页表根
    {
        root.switch_root()
    } -> std::same_as<void>;
};

template <typename T>
concept ArchContextTrait = requires(T *ctx) {
    {
        ctx->pc()
    } -> std::same_as<umb_t &>;
    {
        ctx->sp()
    } -> std::same_as<umb_t &>;
};

// 中断管理器 Trait
template <typename T>
concept ArchInterruptTrait = requires() {
    {
        T::init()
    } -> std::same_as<void>;
    {
        T::sti()
    } -> std::same_as<void>;
    {
        T::cli()
    } -> std::same_as<void>;
};

// Write-Protection Fault Infomation Trait
template <typename T>
concept ArchWPFaultTrait = requires() {
    true;
};