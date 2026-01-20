/**
 * @file memory.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存布局获取
 * @version alpha-1.0.0
 * @date 2026-01-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/trait.h>
#include <kio.h>
#include <symbols.h>

#include <algorithm>

constexpr int MEM_REGION_BUF      = 8;
constexpr int RESERVED_REGION_BUF = 32;
constexpr int MMODE_RESERVED_BUF  = 16;
constexpr int MMODE_REGISTER_BUF  = 8;

constexpr char MEMORY_NODE_NAME[]          = "memory";
constexpr char REG_PROPERTY_NAME[]         = "reg";
constexpr char RESERVED_MEMORY_NODE_NAME[] = "reserved-memory";
constexpr char MMODE_RESERVED_BASE[]       = "mmode_resv";

static bool read_regions(FDTHelper::RegVal regions[MEM_REGION_BUF],
                         FDTHelper::RegVal reserved[RESERVED_REGION_BUF],
                         int &num_regions, int &num_reserved) {
    // 读取 /memory 节点的 reg 属性
    FDTNodeDesc root = FDTHelper::get_root_node();

    FDTNodeDesc memory_node = FDTHelper::get_subnode(root, MEMORY_NODE_NAME);
    if (memory_node < 0) {
        // 未找到memory节点
        return false;
    }

    FDTPropDesc prop_reg =
        FDTHelper::get_property(memory_node, REG_PROPERTY_NAME);
    if (prop_reg < 0) {
        // 未找到reg属性
        return false;
    }

    int addr_cells = FDTHelper::get_parent_addresses_cells(memory_node);
    int size_cells = FDTHelper::get_parent_size_cells(memory_node);

    FDTHelper::RegVal mmode_register_buf[MMODE_REGISTER_BUF];

    num_regions = FDTHelper::get_property_value_as_reg_regions(
        prop_reg, addr_cells, size_cells, regions, MEM_REGION_BUF);
    if (num_regions <= 0) {
        // 解析失败
        return false;
    }

    FDTNodeDesc reserved_memory =
        FDTHelper::get_subnode(root, RESERVED_MEMORY_NODE_NAME);
    addr_cells = FDTHelper::get_parent_addresses_cells(reserved_memory);
    size_cells = FDTHelper::get_parent_size_cells(reserved_memory);

    // 遍历所有可能的保留内存区域
    int reserved_cnt = 0;
    for (int i = 0; i < 15; i++) {
        char name[32];
        sprintf(name, "%s%d", MMODE_RESERVED_BASE, i);
        FDTNodeDesc resv_node = FDTHelper::get_subnode(reserved_memory, name);
        if (resv_node < 0) {
            continue;
        }

        FDTPropDesc resv_prop =
            FDTHelper::get_property(resv_node, REG_PROPERTY_NAME);
        if (resv_prop < 0) {
            continue;
        }

        int resv_cnt = FDTHelper::get_property_value_as_reg_regions(
            resv_prop, addr_cells, size_cells, mmode_register_buf,
            MMODE_REGISTER_BUF);
        if (resv_cnt <= 0) {
            continue;
        }

        // 将保留区域加入reserved_buf
        for (int i = 0; i < resv_cnt; i++) {
            reserved[reserved_cnt] = mmode_register_buf[i];
            if (reserved_cnt + 1 >= RESERVED_REGION_BUF) {
                return false;
            }
            reserved_cnt++;
        }
    }

    num_reserved = reserved_cnt;
    return true;
}

static bool add_region(MemRegion *regions, int max_cnt, int &idx, void *addr,
                       size_t size, MemRegion::MemoryStatus status) {
    if (idx >= max_cnt) {
        return false;
    }
    regions[idx] = (MemRegion){
        .ptr    = addr,
        .size   = size,
        .status = status,
    };
    idx++;
    return true;
}

int Riscv64MemoryLayout::detect_memory_layout(MemRegion *regions, int max_cnt) {
    FDTHelper::RegVal regions_buf[MEM_REGION_BUF];
    FDTHelper::RegVal reserved_buf[RESERVED_REGION_BUF + 1];

    // 根据Regions与Reserved计算最终内存布局
    int num_regions, num_reserved;
    if (!read_regions(regions_buf, reserved_buf, num_regions, num_reserved)) {
        return -1;
    }

    int idx = 0;

    // 将内核划入保留区域
    size_t kernel_sz           = (size_t)(&skernel - &ekernel);
    reserved_buf[num_reserved] = {
        .ptr  = &skernel,
        .size = kernel_sz,
    };
    num_reserved++;

    // 把保留区域与内存区域按起始地址排序
    std::sort(regions_buf, regions_buf + num_regions,
              [](const FDTHelper::RegVal &a, const FDTHelper::RegVal &b) {
                  return a.ptr < b.ptr;
              });
    std::sort(reserved_buf, reserved_buf + num_reserved,
              [](const FDTHelper::RegVal &a, const FDTHelper::RegVal &b) {
                  return a.ptr < b.ptr;
              });

    // 加入所有保留区域
    for (int i = 0; i < num_reserved; i++) {
        add_region(regions, max_cnt, idx, reserved_buf[i].ptr,
                   reserved_buf[i].size, MemRegion::MemoryStatus::RESERVED);
    }

    // 由于现在保留区域和内存区域的存储具有有序性
    // 我们在遍历内存区域并处理保留区域时, 可以遵循线性扫描的原则
    int j = 0;
    for (int i = 0; i < num_regions; i++) {
        umb_t reg_s = (umb_t)(regions_buf[i].ptr);
        umb_t reg_e = reg_s + regions_buf[i].size;

        for (; j < num_reserved; j++) {
            umb_t rsvd_s = (umb_t)(reserved_buf[j].ptr);
            umb_t rsvd_e = rsvd_s + reserved_buf[j].size;

            // 如果内存区域在保留区域之后, 则继续下一个保留区域
            if (reg_s >= rsvd_e) {
                continue;
            }

            // 如果内存区域在保留区域之前, 则之后的所有保留区域都在内存区域之后
            // 因此直接处理剩余部分并跳出循环
            if (reg_e <= rsvd_s) {
                break;
            }

            // 此时内存区域与保留区域存在交集
            // 判断是前端重叠, 后端重叠还是完全覆盖

            // 完全覆盖
            // reg_s ----- rsvd_s ----- rsvd_e ----- reg_e
            // 此时[reg_s, rsvd_s)可直接加入FREE区域
            // [rsvd_e, reg_e)部分继续处理
            if (reg_s < rsvd_s && reg_e > rsvd_e) {
                size_t sz = rsvd_s - reg_s;
                add_region(regions, max_cnt, idx, (void *)reg_s, sz,
                           MemRegion::MemoryStatus::FREE);
                reg_s = rsvd_e;
                continue;
            }

            // 前端重叠
            // rsvd_s ----- reg_s ----- rsvd_e ----- reg_e
            // 此时直接将reg_s调整到rsvd_e位置, 继续处理
            if (reg_s >= rsvd_s && reg_e > rsvd_e) {
                reg_s = rsvd_e;
                continue;
            }

            // 后端重叠
            // reg_s ----- rsvd_s ----- reg_e ----- rsvd_e
            // 将reg_e设置为rsvd_s
            // 并结束处理
            if (reg_s < rsvd_s && reg_e <= rsvd_e) {
                reg_e = rsvd_s;
                break;
            }

            // 此时为完全包含, 令reg_s = reg_e以结束处理
            reg_s = reg_e;
            break;
        }

        if (reg_s < reg_e) {
            // 处理剩余部分
            size_t sz = reg_e - reg_s;
            add_region(regions, max_cnt, idx, (void *)reg_s, sz,
                       MemRegion::MemoryStatus::FREE);
        }
    }

    return idx;
}