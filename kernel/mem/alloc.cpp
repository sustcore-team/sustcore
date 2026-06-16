/**
 * @file lga.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 线性增长分配器
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <logger.h>
#include <sustcore/addr.h>
#include <mem/alloc_def.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <sus/logger.h>

char LinearGrowAllocator::LGA_HEAP[LinearGrowAllocator::SIZE];
size_t LinearGrowAllocator::lga_offset = 0;

void* LinearGrowAllocator::malloc(size_t size) {
    size_t rdsz = (size + 7) & ~7;
    if (rdsz >= 4096) {
        size_t pages = page_align_up(rdsz) / PAGESIZE;
        Result<PhyAddr> gfp_res = GFP::get_free_page(pages);
        if (! gfp_res.has_value()) {
            loggers::MEMORY::ERROR("无法分配大对象内存");
            return nullptr;
        }
        PhyAddr paddr = gfp_res.value();
        return convert<KpaAddr>(paddr).addr();
    }
    if (lga_offset + rdsz > SIZE) {
        loggers::MEMORY::FATAL("%s", "内存不足");
        return nullptr;  // 内存不足
    }
    void* ptr   = &LGA_HEAP[lga_offset];
    lga_offset += rdsz;
    return ptr;
}

void LinearGrowAllocator::free(void* ptr) {
    // 线性增长分配器不支持释放内存
    loggers::MEMORY::DEBUG("%s", "线性增长分配器不支持释放内存");
    (void)ptr;  // 避免未使用参数警告
}

void LinearGrowAllocator::init(void) {
    loggers::SUSTCORE::INFO("线性增长分配器初始化完成, 可用内存大小: %u 字节", SIZE);
}
