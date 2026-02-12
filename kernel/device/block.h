/**
 * @file block.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 块设备接口
 * @version alpha-1.0.0
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/rtti.h>
#include <cstddef>

using lba_t = size_t;

enum class BlockDeviceType {
    BASIC = 0,
    RAMDISK = 1
};

class IBlockDevice : public RTTIBase<IBlockDevice, BlockDeviceType> {
public:
    virtual ~IBlockDevice() = default;
    /**
     * @brief 获得块大小
     *
     * @return size_t 块大小（字节）
     */
    virtual size_t block_sz(void) const                          = 0;
    /**
     * @brief 获得块数量
     *
     * @return size_t 块数量
     */
    virtual size_t block_cnt(void) const                         = 0;
    /**
     * @brief 读取块
     *
     * @param lba 起始块地址
     * @param buf 读取数据的缓冲区
     * @param cnt 读取块的数量
     * @return size_t 实际读取的块数量
     */
    virtual size_t read_blocks(lba_t lba, void *buf, size_t cnt) = 0;
    /**
     * @brief 写入块
     *
     * @param lba 起始块地址
     * @param buf 写入数据的缓冲区
     * @param cnt 写入块的数量
     * @return size_t 实际写入的块数量
     */
    virtual size_t write_blocks(lba_t lba, const void *buf, size_t cnt) = 0;
    /**
     * @brief 同步块设备
     *
     * @return bool 是否同步成功
     */
    virtual bool sync(void)                                             = 0;
};

class RamDiskDevice : public IBlockDevice {
private:
    void *D_base;
    size_t D_block_size;
    size_t D_block_count;
public:
    constexpr static BlockDeviceType IDENTIFIER = BlockDeviceType::RAMDISK;
    virtual BlockDeviceType type_id() const override;
    virtual ~RamDiskDevice() = default;
    constexpr RamDiskDevice(void *base, size_t block_size, size_t block_count)
        : D_base(base), D_block_size(block_size), D_block_count(block_count) {}
    virtual size_t block_sz(void) const override;
    virtual size_t block_cnt(void) const override;
    virtual size_t read_blocks(lba_t lba, void *buf, size_t cnt) override;
    virtual size_t write_blocks(lba_t lba, const void *buf, size_t cnt) override;
    virtual bool sync(void) override;
    constexpr void *base() const { return D_base; }
};