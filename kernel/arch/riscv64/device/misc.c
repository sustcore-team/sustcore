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

#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/device/device.h>
#include <basec/logger.h>

int get_clock_freq_hz(void) {
    // 读取 /cpus/timebase-frequency 属性
    FDTNodeDesc root = get_device_root(fdt);
    
    FDTNodeDesc cpus_node = get_sub_device(fdt, root, "cpus");
    if (cpus_node < 0) {
        // 未找到cpus节点
        log_error("未找到/cpus节点, 无法获取时钟频率");
        return -1;
    }

    FDTPropDesc prop_freq = get_device_property(fdt, cpus_node, "timebase-frequency");
    if (prop_freq < 0) {
        // 未找到timebase-frequency属性
        log_error("未找到/cpus/timebase-frequency属性, 无法获取时钟频率");
        return -1;
    }

    int freq = get_device_property_value_as_dword(fdt, prop_freq);
    if (freq <= 0) {
        // 属性值无效
        log_error("/cpus/timebase-frequency不能以dword读取, 无法获取时钟频率");
        return -1;
    }

    // 对于QEMU的virt机器, 时钟频率恒为10MHz
    return freq;
}