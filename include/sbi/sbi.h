/**
 * @file sbi.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI接口
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <sus/bits.h>

#include <sbi/sbi_enum.h>

/**
 * @brief SBI调用返回结构体
 * 
 */
typedef struct {
    smb_t error; // 错误码
    union {
        smb_t value; // 返回值(有符号)
        umb_t uvalue; // 返回值(无符号)
    };
} SBIRet;

/**
 * @brief SBI功能调用函数
 * 
 * @param eid   extension ID
 * @param fid   function ID
 * @param arg0  第1个参数
 * @param arg1  第2个参数
 * @param arg2  第3个参数
 * @param arg3  第4个参数
 * @param arg4  第5个参数
 * @param arg5  第6个参数
 * @return SBIRet 返回值
 */
SBIRet sbi_ecall(dword eid, dword fid, 
                 umb_t arg0, umb_t arg1,
                 umb_t arg2, umb_t arg3, 
                 umb_t arg4, umb_t arg5);

//-----------------------
// Legacy SBI Calls
//-----------------------

SBIRet sbi_set_timer(qword stime_value);

/**
 * @brief 向控制台输出一个字符
 * 
 * (已弃用)
 * 
 * @param ch 字符
 * @return SBIRet 返回值
 */
SBIRet sbi_console_putchar(char ch);

/**
 * @brief 从控制台获取一个字符
 * 
 * (已弃用)
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_console_getchar(void);

/**
 * @brief 清空IPI
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_clear_ipi(void);

/**
 * @brief 发送IPI
 * 
 * @param hart_mask_ptr hart掩码指针
 * @return SBIRet 返回值
 */
SBIRet sbi_send_ipi(const void *hart_mask_ptr);

/**
 * @brief 远程指令缓存刷新
 * 
 * @param hart_mask_ptr hart掩码指针
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_fence_i(const void *hart_mask_ptr);

/**
 * @brief 远程虚拟地址刷新
 * 
 * @param hart_mask_ptr hart掩码指针
 * @param start_addr 起始地址
 * @param size 大小
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_sfence_vma(const void *hart_mask_ptr, 
                             umb_t start_addr, 
                             umb_t size);

/**
 * @brief 远程虚拟地址刷新(带ASID)
 * 
 * @param hart_mask_ptr hart掩码指针
 * @param start_addr 起始地址
 * @param size 大小
 * @param asid 地址空间标识符
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_sfence_vma_asid(const void *hart_mask_ptr, 
                                 umb_t start_addr, 
                                 umb_t size,
                                 umb_t asid);

/**
 * @brief 关闭系统
 * 
 * (已弃用)
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_shutdown(void);

//-----------------------
// Base 基础 SBI Calls
//-----------------------

/**
 * @brief 获取SBI规范版本
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_get_spec_version(void);

/**
 * @brief 获取SBI实现ID
 * 
 * @return SBIRet 返回值    
 */
SBIRet sbi_get_impl_id(void);

/**
 * @brief 获取SBI实现版本
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_get_impl_version(void);

/**
 * @brief 探测SBI扩展
 * 
 * @param extension_id 扩展ID
 * @return SBIRet 返回值
 */
SBIRet sbi_probe_extension(dword extension_id);

/**
 * @brief 获取Mvendorid
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_get_mvendorid(void);

/**
 * @brief 获取Marchid
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_get_marchid(void);

/**
 * @brief 获取Mimpid
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_get_mimpid(void);

//-----------------------
// DBCN: Debug Console
//-----------------------

/**
 * @brief 向调试控制台写入数据
 * 
 * @param len 数据长度
 * @param buf 数据缓冲区
 * @return SBIRet 返回值
 */
SBIRet sbi_dbcn_console_write(umb_t len, const void* buf);

/**
 * @brief 向调试控制台读取数据
 * 
 * @param len 数据长度
 * @param buf 数据缓冲区
 * @return SBIRet 返回值
 */
SBIRet sbi_dbcn_console_read(umb_t len, void* buf);

/**
 * @brief 向调试控制台写入一个字节
 * 
 * @param ch 字节
 * @return SBIRet 返回值
 */
SBIRet sbi_dbcn_console_write_byte(char ch);