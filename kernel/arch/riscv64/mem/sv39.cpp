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
#include <logger.h>
#include <sustcore/addr.h>

void Riscv64SV39PageMan::init(void) {
    loggers::PAGING::INFO("SV39页表管理器初始化完成");
}

KpaAddr Riscv64SV39PageMan::_convert(PhyAddr paddr) {
    return convert<KpaAddr>(paddr);
}

void Riscv64SV39PageMan::make_root(PhyAddr root) {
    memset(_convert(root).addr(), 0, PAGESIZE);
}

void Riscv64SV39PageMan::__switch_root(PhyAddr __root) {
    csr_satp_t new_satp;
    new_satp.mode = SATPMode::SV39;
    new_satp.asid = 0;  // TODO: ASID支持
    new_satp.ppn  = Riscv64SV39PageMan::to_ppn(__root);
    csr_set_satp(new_satp);
}

void Riscv64SV39PageMan::flush_tlb() {
    asm volatile("sfence.vma");
}
