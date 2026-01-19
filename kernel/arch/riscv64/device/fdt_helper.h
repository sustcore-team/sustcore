/**
 * @file fdt_helper.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT Helper 头文件
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/bits.h>
#include <libfdt.h>
#include <cstddef>

typedef void FDTDesc;
typedef int FDTNodeDesc;
typedef int FDTPropDesc;

namespace FDTHelper {
    extern FDTDesc *fdt;
    extern int errno;
    FDTDesc *fdt_init(void *dtb_ptr);
    FDTNodeDesc get_root_node(void);
    FDTNodeDesc get_subnode(FDTNodeDesc parent, const char *name);
    FDTPropDesc get_property(FDTNodeDesc node, const char *name);
    struct PropVal {
        void *ptr;
        size_t len;
    };
    struct RegVal {
        void *ptr;
        size_t size;
    };
    PropVal get_property_value(FDTPropDesc prop);
    const char *get_property_value_as_string(FDTPropDesc prop);
    template<typename T, T errno = 0>
    T get_property_value_as(FDTPropDesc prop) {
        PropVal val = get_property_value(prop);
        if (val.len < sizeof(T)) {
            return 0;
        }
        return *(T *)(val.ptr);
    }
    template<>
    word get_property_value_as<word, 0>(FDTPropDesc prop);
    template<>
    dword get_property_value_as<dword, 0>(FDTPropDesc prop);
    template<>
    int get_property_value_as<int, -1>(FDTPropDesc prop);
    template<>
    qword get_property_value_as<qword, 0>(FDTPropDesc prop);
    int get_reg_regions_cnt(FDTPropDesc prop, int addr_cells, int size_cells);
    int get_property_value_as_reg_regions(FDTPropDesc prop, int addr_cells,
                                          int size_cells, RegVal *regions,
                                          int max_regions);
    int get_parent_addresses_cells(FDTNodeDesc node);
    int get_parent_size_cells(FDTNodeDesc node);
    // void print_device_tree(void);
    // void print_device_tree_detailed(void);
};