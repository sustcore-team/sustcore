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
#include <basecpp/logger.h>
#include <kio.h>
#include <sbi/sbi.h>

TimerInfo timer_info;

int get_clock_freq_hz(void) {
    // 读取 /cpus/timebase-frequency 属性
    FDTNodeDesc root = FDTHelper::get_root_node();

    FDTNodeDesc cpus_node = FDTHelper::get_subnode(root, "cpus");
    if (cpus_node < 0) {
        // 未找到cpus节点
        DEVICE::ERROR("未找到/cpus节点, 无法获取时钟频率");
        return -1;
    }

    FDTPropDesc prop_freq =
        FDTHelper::get_property(cpus_node, "timebase-frequency");
    if (prop_freq < 0) {
        // 未找到timebase-frequency属性
        DEVICE::ERROR("未找到/cpus/timebase-frequency属性, 无法获取时钟频率");
        return -1;
    }

    int freq = FDTHelper::get_property_value_as<int, 0>(prop_freq);
    if (freq <= 0) {
        // 属性值无效
        DEVICE::ERROR(
            "/cpus/timebase-frequency不能以dword读取, 无法获取时钟频率");
        return -1;
    }

    // 对于QEMU的virt机器, 时钟频率恒为10MHz
    return freq;
}

void init_timer(int freq, int expected_freq) {
    // 设置计时器频率(freq:Hz, expected_freq:mHz(10^-3 Hz))
    int increasment          = freq / expected_freq * 1000;
    timer_info.freq          = freq;
    timer_info.expected_freq = expected_freq;
    timer_info.increasment   = increasment;

    // 之后稳定触发
    sbi_legacy_set_timer(csr_get_time() + increasment);

    // 启用S-Mode计时器中断
    csr_sie_t sie = csr_get_sie();
    sie.stie      = 1;
    csr_set_sie(sie);
}