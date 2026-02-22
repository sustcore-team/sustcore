/**
 * @file misc.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 杂项
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/misc.h>
#include <kio.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sus/units.h>

TimerInfo timer_info;

units::frequency get_clock_freq(void) {
    // 读取 /cpus/timebase-frequency 属性
    FDTNodeDesc root = FDTHelper::get_root_node();

    FDTNodeDesc cpus_node = FDTHelper::get_subnode(root, "cpus");
    if (cpus_node < 0) {
        // 未找到cpus节点
        DEVICE::ERROR("未找到/cpus节点, 无法获取时钟频率");
        return 0_Hz;
    }

    FDTPropDesc prop_freq =
        FDTHelper::get_property(cpus_node, "timebase-frequency");
    if (prop_freq < 0) {
        // 未找到timebase-frequency属性
        DEVICE::ERROR("未找到/cpus/timebase-frequency属性, 无法获取时钟频率");
        return 0_Hz;
    }

    int freq = FDTHelper::get_property_value_as<int, 0>(prop_freq);
    if (freq <= 0) {
        // 属性值无效
        DEVICE::ERROR(
            "/cpus/timebase-frequency不能以dword读取, 无法获取时钟频率");
        return 0_Hz;
    }

    // 对于QEMU的virt机器, 时钟频率恒为10MHz
    return units::frequency::from_hz(freq);
}

void init_timer(units::frequency freq, units::frequency expected_freq) {
    units::tick increment    = 1_ticks * (freq.to_hz() / expected_freq.to_hz());
    timer_info.freq          = freq;
    timer_info.expected_freq = expected_freq;
    timer_info.increment     = increment;

    // 之后稳定触发
    sbi_legacy_set_timer(csr_get_time() + increment.to_ticks());

    // 启用S-Mode计时器中断
    csr_sie_t sie = csr_get_sie();
    sie.stie      = 1;
    csr_set_sie(sie);
}