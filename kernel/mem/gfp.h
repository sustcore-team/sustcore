/**
 * @file gfp.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Get Free Page
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <mem/buddy.h>
#include <mem/gfp_def.h>
#include <sustcore/addr.h>

/**
 * @brief 当前 GFP 使用的底层裸页框分配器。
 *
 * RawGFPImpl 只负责实际页框的分配与释放，不维护页框共享状态。
 * GFP 在此基础上叠加引用计数，用于支持 fork 后的 COW 页面共享。
 */
using RawGFPImpl = BuddyAllocator;

/**
 * @brief 带引用计数管理的页框分配器接口。
 *
 * GFP 是内核的统一页框分配器,
 * 其实际通过RawGFPImpl实现页的分配与归还,
 * 并为每个页维护一个引用计数, 以实现 COW 功能.
 */
class GFP {
private:
    // 最大追踪 4GB 内存
    constexpr static size_t MAX_TRACKED_MEMORY = 4ull * 1024 * 1024 * 1024;
    constexpr static size_t MAX_TRACKED_PAGES  = MAX_TRACKED_MEMORY / PAGESIZE;

    /**
     * @brief 每个物理页的引用计数。
     *
     * 数组下标由物理地址除以 PAGESIZE 得到。值为 0 表示该页当前
     * 未由 GFP 管理或已归还底层分配器。
     *
     * TODO: 使用根据当前设备动态决定的refcounts表, 或是使用哈希表
     */
    inline static size_t refcounts[MAX_TRACKED_PAGES] = {};

    /**
     * @brief 将物理地址转换为引用计数表下标。
     *
     * @param addr 需要查询的物理地址。
     * @return 物理页在 refcounts 中的下标。
     */
    static size_t topfn(PhyAddr addr) {
        return addr.page_align_down().arith() / PAGESIZE;
    }

    /**
     * @brief 判断一段物理页是否在引用计数表可跟踪范围内。
     *
     * @param addr 起始物理地址。
     * @param page_count 页数。
     * @return true 表示整段页都可被 refcounts 跟踪。
     */
    static bool tracked(PhyAddr addr, size_t page_count = 1) {
        size_t pfn = topfn(addr);
        return pfn < MAX_TRACKED_PAGES && page_count <= MAX_TRACKED_PAGES - pfn;
    }

public:
    /**
     * @brief 初始化 pre-init 阶段的底层裸页框分配器。
     */
    static void pre_init() {
        RawGFPImpl::pre_init();
    }

    /**
     * @brief 初始化 post-init 阶段的底层裸页框分配器。
     */
    static void post_init() {
        RawGFPImpl::post_init();
    }

    /**
     * @brief 分配连续物理页并初始化引用计数。
     *
     * @tparam Stage 当前内核初始化阶段。
     * @param page_count 需要分配的 4KiB 页数。
     * @return 成功时返回起始物理地址;失败时返回底层分配器错误。
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static Result<PhyAddr> get_free_page(size_t page_count = 1) {
        auto res = RawGFPImpl::template get_free_page<Stage>(page_count);
        if (!res.has_value()) {
            unexpect_return(res.error());
        }
        PhyAddr paddr = res.value();
        if (tracked(paddr, page_count)) {
            size_t pfn = topfn(paddr);
            for (size_t i = 0; i < page_count; ++i) {
                refcounts[pfn + i] = 1;
            }
        }
        return paddr;
    }

    /**
     * @brief 释放连续物理页的一次引用。
     *
     * 对可跟踪页，本函数逐页降低引用计数，仅将引用计数归零的连续页段
     * 归还给 RawGFPImpl。对不可跟踪页，直接委托 RawGFPImpl 释放。
     *
     * @tparam Stage 当前内核初始化阶段。
     * @param addr 起始物理地址。
     * @param page_count 页数。
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page(PhyAddr addr, size_t page_count = 1) {
        if (!addr.nonnull()) {
            return;
        }
        if (!tracked(addr, page_count)) {
            RawGFPImpl::template put_page<Stage>(addr, page_count);
            return;
        }

        size_t pfn       = topfn(addr);
        size_t run_start = 0;
        size_t run_len   = 0;
        for (size_t i = 0; i < page_count; ++i) {
            if (refcounts[pfn + i] > 0) {
                refcounts[pfn + i]--;
            }
            if (refcounts[pfn + i] == 0) {
                if (run_len == 0) {
                    run_start = i;
                }
                run_len++;
            } else if (run_len != 0) {
                RawGFPImpl::template put_page<Stage>(
                    addr + run_start * PAGESIZE, run_len);
                run_len = 0;
            }
        }
        if (run_len != 0) {
            RawGFPImpl::template put_page<Stage>(addr + run_start * PAGESIZE,
                                                 run_len);
        }
    }

    /**
     * @brief 增加连续物理页的引用计数。
     *
     * COW 建立共享映射时调用本函数，表示另一个地址空间也引用了这些
     * 物理页。不可跟踪页会被忽略。
     *
     * @param addr 起始物理地址。
     * @param page_count 页数。
     */
    static void keep_page(PhyAddr addr, size_t page_count = 1) {
        if (!tracked(addr, page_count)) {
            return;
        }
        size_t pfn = topfn(addr);
        for (size_t i = 0; i < page_count; ++i) {
            refcounts[pfn + i]++;
        }
    }

    /**
     * @brief 查询物理页当前引用计数。
     *
     * @param addr 需要查询的物理地址。
     * @return 可跟踪页返回实际引用计数;不可跟踪页按独占页处理，返回 1。
     */
    static size_t ref_count(PhyAddr addr) {
        if (!tracked(addr, 1)) {
            return 1;
        }
        return refcounts[topfn(addr)];
    }
};
