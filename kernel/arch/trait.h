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

#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <sustcore/errcode.h>

#include <concepts>
#include <cstddef>

/**
 * @brief 架构串口 Trait
 *
 * @tparam T 架构串口类
 */
template <typename T>
concept EarlySerialTrait = requires(char ch, size_t len, const char *str) {
    {
        T::serial_write_char(ch)
    } -> std::same_as<void>;
    {
        T::serial_write_string(len, str)
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
concept InitializationTrait = requires() {
    {
        T::pre_init()
    } -> std::same_as<void>;
    {
        T::post_init()
    } -> std::same_as<void>;
    {
        T::init_clock()
    } -> std::same_as<Result<void>>;
};

// 初始化条件
template <typename T>
concept ArchPageManTrait_Initialization = requires() {
    {
        T::init()
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
    requires(T::PageSize size, T::QueryResult query_res) {
        // PageSize 枚举类型
        // 要求有4K页
        std::is_scoped_enum_v<typename T::PageSize>;
        {
            T::PageSize::_4K
        } -> std::same_as<typename T::PageSize>;
        {
            T::psize(size)
        } -> std::convertible_to<size_t>;
        // 页表项类型
        typename T::PTE;
        {
            T::PTE_CNT
        } -> std::convertible_to<size_t>;
        // QueryResult
        typename T::QueryResult;
        {
            query_res.pte
        } -> std::same_as<typename T::PTE *&>;
        {
            query_res.size
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
    } -> std::same_as<PhyAddr>;
    {
        T::is_dirty(pte)
    } -> std::convertible_to<bool>;
};

template <typename T>
concept ArchPageManTrait_PageFlags =
    requires(typename T::PageFlags flags, typename T::RWX rwx) {
        typename T::PageFlags;
        {
            typename T::PageFlags{rwx, true, false, true}
        };
        {
            flags.rwx
        } -> std::same_as<typename T::RWX &>;
        {
            flags.u
        } -> std::convertible_to<bool>;
        {
            flags.g
        } -> std::convertible_to<bool>;
        {
            flags.p
        } -> std::convertible_to<bool>;
    };

// 修改器
template <typename T>
concept ArchPageManTrait_Modifier =
    requires(bool r, bool w, bool x, bool u, bool g, bool p) {
        std::is_scoped_enum_v<typename T::Modifier>;
        {
            T::make_mask(r, w, x, u, g, p)
        } -> std::same_as<typename T::Modifier>;
        {
            T::Modifier::NONE
        } -> std::same_as<typename T::Modifier>;
        {
            T::Modifier::RWX
        } -> std::same_as<typename T::Modifier>;
        {
            T::Modifier::ALL
        } -> std::same_as<typename T::Modifier>;
    };

// 页表管理器 Trait
template <typename T>
concept ArchPageManTrait = requires(T root, size_t size, VirAddr vaddr,
                                    PhyAddr paddr, T::RWX rwx,
                                    typename T::PageFlags flags,
                                    T::QueryResult query_res) {
    // 初始化条件
    requires ArchPageManTrait_Initialization<T>;
    // 满足RWX条件
    requires ArchPageManTrait_RWX<T>;
    // 满足页表结构条件
    requires ArchPageManTrait_PagingStructures<T>;
    // 满足PTE信息读取条件
    requires ArchPageManTrait_PTEInfoReader<T>;
    // 满足页标志结构条件
    requires ArchPageManTrait_PageFlags<T>;
    // 获得/构造页表根
    {
        T::read_root()
    } -> std::same_as<PhyAddr>;
    {
        T::make_root(paddr)
    } -> std::same_as<void>;
    // 构造页表管理器
    {
        T(paddr)
    } -> std::same_as<T>;
    // 查询页
    {
        root.query_page(vaddr)
    } -> std::same_as<Result<typename T::QueryResult>>;
    // 单页映射
    {
        root.template map_page<T::PageSize::_4K>(vaddr, paddr, flags)
    } -> std::same_as<void>;
    // 解除单页映射
    {
        root.unmap_page(vaddr)
    } -> std::same_as<void>;
    // 范围映射
    {
        root.template map_range<false>(vaddr, paddr, size, flags)
    } -> std::same_as<void>;
    {
        root.template map_range<true>(vaddr, paddr, size, flags)
    } -> std::same_as<void>;
    // 解除范围映射
    {
        root.unmap_range(vaddr, size)
    } -> std::same_as<void>;
    requires ArchPageManTrait_Modifier<T>;
    // 修改单个页表项标志
    {
        T::template modify_pte<T::Modifier::NONE>(query_res.pte, flags)
    } -> std::same_as<void>;
    // 修改页面标志
    {
        root.template modify_flags<T::Modifier::NONE>(vaddr, flags)
    } -> std::same_as<void>;
    // 修改范围页面标志
    {
        root.template modify_range_flags<T::Modifier::NONE>(vaddr, size, flags)
    } -> std::same_as<void>;
    // 更换页表根
    {
        T::__switch_root(paddr)
    } -> std::same_as<void>;
    // 刷新TLB
    {
        T::flush_tlb()
    } -> std::same_as<void>;
    // 获得页表根
    {
        root.get_root()
    } -> std::same_as<PhyAddr>;
    // 更换页表根
    {
        root.switch_root()
    } -> std::same_as<void>;
};

enum class SetupCase { UTHREAD_TRAMPOLINE, USER_THREAD, KTHREAD };

template <SetupCase setcase>
inline static constexpr bool setupcase_dependent_false = false;

template <typename T>
concept ContextTrait = requires(T *ctx, void *context) {
    {
        ctx->pc()
    } -> std::same_as<umb_t &>;
    {
        ctx->sp()
    } -> std::same_as<umb_t &>;
    {
        ctx->kstack_top()
    } -> std::same_as<umb_t &>;
    {
        ctx->template setup_regs<SetupCase::UTHREAD_TRAMPOLINE>()
    } -> std::same_as<void>;
    {
        ctx->template setup_regs<SetupCase::USER_THREAD>()
    } -> std::same_as<void>;
    {
        ctx->template setup_regs<SetupCase::KTHREAD>()
    } -> std::same_as<void>;
};

// 中断管理器 Trait
template <typename T>
concept InterruptTrait = requires() {
    {
        T::init()
    } -> std::same_as<void>;
    {
        T::sti()
    } -> std::same_as<void>;
    {
        T::cli()
    } -> std::same_as<void>;
    {
        T::enabled()
    } -> std::convertible_to<bool>;
};

template <typename T>
concept IdleTrait = requires() {
    {
        T::idle()
    } -> std::same_as<void>;
};
