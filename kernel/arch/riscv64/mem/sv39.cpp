/**
 * @file sv39.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief sv39页表管理实现
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/mem/sv39.h>
#include <kio.h>
#include <mem/addr.h>

template<>
void Riscv64SV39PageMan<KernelStage::PRE_INIT>::init(void) {
    // SV39页表管理器不需要特殊的前初始化步骤
    // 但我们可以在这里设置一些全局状态或日志
    PAGING::INFO("SV39页表管理器前初始化完成");
}

template<>
void Riscv64SV39PageMan<KernelStage::POST_INIT>::init(void) {
    // SV39页表管理器不需要特殊的后初始化步骤
    // 但我们可以在这里设置一些全局状态或日志
    PAGING::INFO("SV39页表管理器后初始化完成");
}