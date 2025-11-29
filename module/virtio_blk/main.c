/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtio块设备驱动主文件
 * @version alpha-1.0.0
 * @date 2025-11-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sus/bits.h>
#include <kmod/system_args.h>
#include <kmod/syscall.h>
#include <kmod/device.h>

#define VIRTIO_MAGIC 0x74726976
#define VIRTIO_DEVICE_BLOCK 0x10000000

struct virtio_mmio_regs {
    dword magic;
    dword version;
    dword device_id;
    dword vendor_id;
    // TODO: 其他寄存器
};

int detect_virtio_blk(void *base_addr) {
    struct virtio_mmio_regs *regs = (struct virtio_mmio_regs *)base_addr;

    if (regs->magic != VIRTIO_MAGIC) {
        return -1; // 不是 virtio 设备
    }

    if (regs->device_id != VIRTIO_DEVICE_BLOCK) {
        return -1; // 不是块设备
    }

    return 0; // 找到 virtio-blk 设备
}

int kmod_main(void) {
    // 获得系统传递的virtio设备能力
    Capability device_cap = sa_get_device();
    Capability mem_cap = getdevice(device_cap);
    void *device_base = mapmem(mem_cap);
    int ret = detect_virtio_blk(device_base);
    if (ret != 0) {
        // 不是virtio-blk设备
        return -1;
    }
    // ...
    return 0;
}