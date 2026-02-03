/**
 * @file block.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块设备接口
 * @version alpha-1.0.0
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/block.h>
#include <cstring>

size_t RamDiskDevice::block_sz(void) const {
    return D_block_size;
}

size_t RamDiskDevice::block_cnt(void) const {
    return D_block_count;
}

size_t RamDiskDevice::read_blocks(lba_t lba, void *buf, size_t cnt) {
    size_t to_read = cnt;
    if (lba + cnt > D_block_count) {
        to_read = D_block_count - lba;
    }
    void *src = (char *)D_base + lba * D_block_size;
    memcpy(buf, src, to_read * D_block_size);
    return to_read;
}

size_t RamDiskDevice::write_blocks(lba_t lba, const void *buf, size_t cnt) {
    size_t to_write = cnt;
    if (lba + cnt > D_block_count) {
        to_write = D_block_count - lba;
    }
    void *dst = (char *)D_base + lba * D_block_size;
    memcpy(dst, buf, to_write * D_block_size);
    return to_write;
}

bool RamDiskDevice::sync(void) {
    // RamDisk不需要同步
    return true;
}