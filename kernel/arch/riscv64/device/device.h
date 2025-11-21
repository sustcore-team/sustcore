/**
 * @file device.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备信息接口
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <sus/bits.h>

typedef void FDTDesc;
typedef int FDTNodeDesc;
typedef int FDTPropDesc;

/**
 * @brief 获取最近的设备错误码
 * 
 * @return int 错误码
 */
int device_get_errno(void);

/**
 * @brief 检测FDT
 * 
 * @param fdt FDT描述符
 * @return int 错误码
 */
FDTDesc *device_check_initial(void *dtb_ptr);

/**
 * @brief 获得根节点描述符
 * 
 * @param fdt 设备树描述符
 * @return FDTNodeDesc 根节点描述符
 */
FDTNodeDesc get_device_root(const FDTDesc *fdt);

/**
 * @brief 获得子设备描述符
 * 
 * @param fdt 设备树描述符
 * @param parent 父节点描述符
 * @param name 子设备名称
 * @return FDTNodeDesc 子设备描述符
 */
FDTNodeDesc get_sub_device(const FDTDesc *fdt, FDTNodeDesc parent, const char *name);

/**
 * @brief 获得设备属性描述符
 * 
 * @param fdt 设备树描述符
 * @param node 设备节点描述符
 * @param name 属性名称
 * @return FDTPropDesc 属性描述符
 */
FDTPropDesc get_device_property(const FDTDesc *fdt, FDTNodeDesc node, const char *name);

/**
 * @brief 设备属性值结构体
 * 
 */
typedef struct {
    void *ptr;
    int len;
} FDTPropVal;

/**
 * @brief 获得设备树属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return FDTPropVal 设备树属性值
 */
FDTPropVal get_device_property_value(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 以字符串形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return const char* 设备树属性值
 */
const char *get_device_property_value_as_string(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 以byte形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return byte 设备树属性值
 */
byte get_device_property_value_as_byte(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 以word形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return word 设备树属性值
 */
word get_device_property_value_as_word(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 以dword形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return dword 设备树属性值
 */
dword get_device_property_value_as_dword(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 以qword形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @return qword 设备树属性值
 */
qword get_device_property_value_as_qword(const FDTDesc *fdt, FDTPropDesc prop);

/**
 * @brief 获得reg区域数量
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @param addr_cells 父节点#address-cells
 * @param size_cells 父节点#size-cells
 * @return int 实际的reg区域数量
 */
int get_reg_region_number(const FDTDesc *fdt, FDTPropDesc prop, int addr_cells, int size_cells);

/**
 * 
 * @brief 以reg区域数组形式获取设备属性值
 * 
 * @param fdt 设备树描述符
 * @param prop 设备树属性描述符
 * @param addr_cells 父节点#address-cells
 * @param size_cells 父节点#size-cells
 * @param buffer 存放区域信息的缓冲区
 * @param sizes 存放每个区域大小的数组
 * @param max_regions 最大区域数量
 * @return int 实际读取的区域数量
 */
int get_device_property_value_as_reg_regions(const FDTDesc *fdt, FDTPropDesc prop, int addr_cells, int size_cells,
    void *buffer, uint64_t *sizes, int max_regions);

/**
 * @brief 获取父节点#address-cells
 * 
 * @param fdt 设备树描述符
 * @param node 设备节点描述符
 * @return int 父节点#address-cells的值
 */
int get_parent_address_cells(const FDTDesc *fdt, FDTNodeDesc node);

/**
 * @brief 获取父节点#size-cells
 * 
 * @param fdt 设备树描述符
 * @param node 设备节点描述符
 * @return int 父节点#size-cells的值
 */
int get_parent_size_cells(const FDTDesc *fdt, FDTNodeDesc node);

/**
 * @brief 打印整个设备树的所有信息
 * 
 * @param fdt 设备树描述符
 */
void print_entire_device_tree(const FDTDesc *fdt);

/**
 * @brief 打印设备树的详细信息
 * 
 * 会额外打印头部信息、内存保留区域和节点统计信息
 * 
 * @param fdt 设备树描述符
 */
void print_device_tree_detailed(const FDTDesc *fdt);