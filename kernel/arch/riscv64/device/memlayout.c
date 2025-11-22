/**
 * @file memlayout.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存布局
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/device/device.h>
#include <basec/logger.h>
#include <libfdt.h>
#include <mem/alloc.h>
#include <sus/arch.h>
#include <sus/boot.h>
#include <sus/symbols.h>

/**
 * @brief 向内存区域链表中添加一个新的内存区域
 *
 * 我们有意将该链表维护成一个有序链表
 *
 * @param root 链表头指针
 * @param addr 地址
 * @param size 大小
 * @param status 状态
 */
static void add_mem_region(MemRegion **root, void *addr, size_t size,
                           int status) {
    MemRegion *region = (MemRegion *)kmalloc(sizeof(MemRegion));
    region->addr      = addr;
    region->size      = size;
    region->status    = status;
    region->next      = nullptr;

    // 插入链表(有意将其作为有序链表)
    if (*root == nullptr) {
        *root = region;
    } else {
        MemRegion *iter = *root;
        MemRegion *last = nullptr;
        while (iter != nullptr && iter->addr <= region->addr) {
            last = iter;
            iter = iter->next;
        }
        if (last == nullptr) {
            // 应当被放置在链表头
            region->next = *root;
            *root        = region;
        } else {
            // last -> next => last -> region -> next
            last->next   = region;
            region->next = iter;
        }
    }
}

MemRegion *arch_get_memory_layout(void) {
    // 首先读取 /memory@<base>/reg 属性
    FDTNodeDesc root = get_device_root(fdt);

    FDTNodeDesc mem = get_sub_device(fdt, root, "memory");
    if (mem < 0) {
        log_error("未找到/memory@<base>节点, 无法获取内存布局");
        return nullptr;
    }

    FDTPropDesc prop_reg = get_device_property(fdt, mem, "reg");
    if (prop_reg < 0) {
        log_error("未找到/memory@<base>/reg属性, 无法获取内存布局");
        return nullptr;
    }

    // 获得root的#address-cells与#size-cells(memory父节点)
    int addr_cells = fdt_address_cells(fdt, root);
    int size_cells = fdt_size_cells(fdt, root);

    // 目前假设最多有8个内存区域
    FDTRegVal regions[8];
    int num_regions = get_device_property_value_as_reg_regions(
        fdt, prop_reg, addr_cells, size_cells, regions, 32);
    if (num_regions <= 0) {
        log_error("无效的/memory@<base>/reg属性, 无法获取内存布局");
        return nullptr;
    }

    // 目前假设最多有 32 个保留内存区域
    FDTRegVal reserved_regions[32];
    // 假设每个mmode_resv节点下最多有8个保留区域
    FDTRegVal tmp_resereved_regions[8];
    int num_reserved_regions = 0;

    // 再读取reserved-memory节点下的保留内存区域
    FDTNodeDesc reserved_mem = get_sub_device(fdt, root, "reserved-memory");
    addr_cells               = fdt_address_cells(fdt, reserved_mem);
    size_cells               = fdt_size_cells(fdt, reserved_mem);

    const char *resv_name_base = "mmode_resv";
    // 处理mmode_resv($i) (0 <= i < 16)
    for (int i = 0; i < 16; i++) {
        char resv_name[32];
        ssprintf(resv_name, "%s%d", resv_name_base, i);
        FDTNodeDesc resv_node = get_sub_device(fdt, reserved_mem, resv_name);
        if (resv_node == -1) {
            // 未找到该节点, 结束
            break;
        }

        // 读取reg属性
        FDTPropDesc resv_prop_reg = get_device_property(fdt, resv_node, "reg");
        if (resv_prop_reg == -1) {
            continue;
        }

        // 读取保留区域
        int tmp_num = get_device_property_value_as_reg_regions(
            fdt, resv_prop_reg, addr_cells, size_cells, tmp_resereved_regions,
            8);
        if (tmp_num <= 0) {
            continue;
        }

        // 依次复制
        for (int j = 0; j < tmp_num; j++) {
            reserved_regions[num_reserved_regions] = tmp_resereved_regions[j];
            num_reserved_regions++;
        }
    }

    // 根据 regions 与 reserved_regions 描绘出内存布局
    // 创建内存区域链表头, 该表头不存储实际内存区域, 只是过渡作用
    MemRegion *head = nullptr;

    // 首先加入内核所在内存块
    add_mem_region(&head, &skernel,
                   (size_t)((umb_t)&ekernel - (umb_t)&skernel),
                   MEM_REGION_RESERVED);

    // 再加入所有reserved区域
    for (int i = 0; i < num_reserved_regions; i++) {
        add_mem_region(&head, reserved_regions[i].ptr, reserved_regions[i].size,
                       MEM_REGION_RESERVED);
    }

    // 然后加入所有可用内存区域(即regions中不与reserved_regions冲突的区域)
    for (int i = 0; i < num_regions; i++) {
        umb_t region_start = (umb_t)regions[i].ptr;
        umb_t region_end   = region_start + (umb_t)regions[i].size;

        // 检查该区域是否与已有的保留区域冲突
        MemRegion *current = head;
        bool conflict      = false;
        while (current != nullptr) {
            // 保留区域[reserved_start, reserved_end)
            umb_t reserved_start = (umb_t)current->addr;
            umb_t reserved_end   = reserved_start + (umb_t)current->size;

            // 检查[region_start, region_end)与[reserved_start,
            // reserved_end)是否有交集
            if (region_start < reserved_end && reserved_start < region_end) {
                conflict = true;
                break;
            }
            current = current->next;
        }

        if (conflict) {
            // 有冲突
            // 将冲突部分剔除, 分块加入链表
            umb_t curr_start = region_start;
            // 由于我们有意将链表维护成有序链表, 因此可以直接遍历链表
            current          = head;
            while (curr_start < region_end && current != nullptr) {
                // 保留区域[reserved_start, reserved_end)
                umb_t reserved_start = (umb_t)current->addr;
                umb_t reserved_end   = reserved_start + (umb_t)current->size;

                // 四种情况
                // 全包含
                // curr_start <= reserved_start < reserved_end <= region_end
                // 前端重叠
                // reserved_start < curr_start < reserved_end < region_end
                // 后端重叠
                // curr_start < reserved_start < region_end < reserved_end
                // 全不包含
                // reserved_end <= curr_start || region_end <= reserved_start

                // 全包含
                if (curr_start <= reserved_start && reserved_end <= region_end)
                {
                    log_debug("可用内存区域[%p, %p)全包含保留区域[%p, %p)",
                              curr_start, region_end, reserved_start,
                              reserved_end);
                    // 添加[curr_start, reserved_start)
                    if (curr_start < reserved_start) {
                        add_mem_region(&head, (void *)curr_start,
                                       (size_t)(reserved_start - curr_start),
                                       MEM_REGION_FREE);
                    }
                    // 移动curr_start到reserved_end
                    curr_start = reserved_end;
                }
                // 前端重叠
                else if (reserved_start < curr_start &&
                         curr_start < reserved_end && reserved_end < region_end)
                {
                    log_debug("可用内存区域[%p, %p)前端重叠保留区域[%p, %p)",
                              curr_start, region_end, reserved_start,
                              reserved_end);
                    // 移动curr_start到reserved_end
                    curr_start = reserved_end;
                    // 前端无处适合添加
                }
                // 后端重叠
                else if (curr_start < reserved_start &&
                         reserved_start < region_end &&
                         region_end < reserved_end)
                {
                    log_debug("可用内存区域[%p, %p)后端重叠保留区域[%p, %p)",
                              curr_start, region_end, reserved_start,
                              reserved_end);
                    // 添加[curr_start, reserved_start)
                    if (curr_start < reserved_start) {
                        add_mem_region(&head, (void *)curr_start,
                                       (size_t)(reserved_start - curr_start),
                                       MEM_REGION_FREE);
                    }
                    // 处理完毕
                    curr_start = region_end;
                }
                // 全不包含
                else
                {
                    // 全不包含
                    // 继续检查下一个保留区域
                }
                current = current->next;
            }

            // 处理剩余部分
            if (curr_start < region_end) {
                add_mem_region(&head, (void *)curr_start,
                               (size_t)(region_end - curr_start),
                               MEM_REGION_FREE);
            }
            continue;
        }

        // 无冲突
        add_mem_region(&head, regions[i].ptr, regions[i].size, MEM_REGION_FREE);
    }

    // 头指向的下一个区域就是实际的内存区域链表头
    return head;
}