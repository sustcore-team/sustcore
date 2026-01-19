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

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief 设定定时器
 * 
 * (已弃用)
 * 
 * @param stime_value 在stime_value时间后触发定时器中断
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_set_timer(qword stime_value);

/**
 * @brief 向控制台输出一个字符
 * 
 * (已弃用)
 * 
 * @param ch 字符
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_console_putchar(char ch);

/**
 * @brief 从控制台获取一个字符
 * 
 * (已弃用)
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_console_getchar(void);

/**
 * @brief 清空IPI
 * 
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_clear_ipi(void);

/**
 * @brief 发送IPI
 * 
 * @param hart_mask_ptr hart掩码指针
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_send_ipi(const void *hart_mask_ptr);

/**
 * @brief 远程指令缓存刷新
 * 
 * @param hart_mask_ptr hart掩码指针
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_remote_fence_i(const void *hart_mask_ptr);

/**
 * @brief 远程虚拟地址刷新
 * 
 * @param hart_mask_ptr hart掩码指针
 * @param start_addr 起始地址
 * @param size 大小
 * @return SBIRet 返回值
 */
SBIRet sbi_legacy_remote_sfence_vma(const void *hart_mask_ptr, 
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
SBIRet sbi_legacy_remote_sfence_vma_asid(const void *hart_mask_ptr, 
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
SBIRet sbi_legacy_shutdown(void);

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

//-----------------------
// Timer Extension
// EID #0x54494D45 "TIME"
//-----------------------

/**
 * @brief 在 `stime_value` 时间后为下一个事件编程时钟
 *
 * @param stime_value 绝对时间
 * @return SBIRet 返回值
 * @note
 * 如果 supervisor 希望清除定时器中断而不安排下一个定时器事件，
 * 它可以请求一个无限远的定时器中断，即 `(uint64_t)-1`。
 * 或者，为了不接收定时器中断，它可以通过清除 `sie.STIE` CSR 位
 * 来屏蔽定时器中断。当 `stime_value` 被设置为未来的某个时间时，
 * 此功能必须清除挂起的定时器中断位，无论定时器中断是否被屏蔽。
 * 此功能总是在 `sbiret.error` 中返回 SBI_SUCCESS。
 */
SBIRet sbi_set_timer(qword stime_value);

//-----------------------
// IPI Extension
// EID #0x00735049 "sPI: s-mode IPI"
//-----------------------

/**
 * @brief 向 hart_mask 中定义的所有 harts 发送处理器间中断
 *
 * @return SBIRet 返回值
 * @note
 * 处理器间的中断在接收方的 harts 上表现为监控程序软件中断。
 */
SBIRet sbi_send_ipi(umb_t hart_mask, umb_t hart_mask_base);

//-----------------------
// RFENCE Extension
// EID #0x52464E43 "RFNC"
//-----------------------

/**
 * @brief 远程 FENCE.I (FID #0)
 *
 * 指示远程 harts 执行 FENCE.I 指令。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_fence_i(umb_t hart_mask, umb_t hart_mask_base);

/**
 * @brief 远程 SFENCE.VMA (FID #1)
 *
 * 指示远程 harts 执行一个或多个 SFENCE.VMA 指令，覆盖从 start_addr 到
 * start_addr + size 的虚拟地址范围。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_sfence_vma(umb_t hart_mask, umb_t hart_mask_base,
                             umb_t start_addr, umb_t size);

/**
 * @brief 远程 SFENCE.VMA with ASID (FID #2)
 *
 * 指示远程 harts 执行一个或多个 SFENCE.VMA 指令，覆盖从 start_addr 到
 * start_addr + size 的虚拟地址范围，仅覆盖指定的 ASID。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @param asid 地址空间标识符
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_sfence_vma_asid(umb_t hart_mask, umb_t hart_mask_base,
                                  umb_t start_addr, umb_t size, umb_t asid);

/**
 * @brief 带 VMID 的远程 HFENCE.GVMA (FID #3)
 *
 * 指示远程 harts 执行一个或多个 HFENCE.GVMA 指令，覆盖从 start_addr 到
 * start_addr + size 的客户机物理地址范围，且仅针对给定的
 * VMID。此函数调用仅对实现了虚拟机监控器扩展的 harts 有效。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @param vmid 虚拟机标识符
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_hfence_gvma_vmid(umb_t hart_mask, umb_t hart_mask_base,
                                   umb_t start_addr, umb_t size, umb_t vmid);

/**
 * @brief 远程 HFENCE.GVMA (FID #4)
 *
 * 指示远程 harts 执行一个或多个 HFENCE.GVMA 指令，覆盖从 start_addr 到
 * start_addr + size 的客户机物理地址范围，针对所有客户机。
 * 此函数调用仅对实现了虚拟机监控器扩展的 harts 有效。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_hfence_gvma(umb_t hart_mask, umb_t hart_mask_base,
                              umb_t start_addr, umb_t size);

/**
 * @brief 带 ASID 的远程 HFENCE.VVMA (FID #5)
 * 
 * 指示远程 harts 执行一个或多个 HFENCE.VVMA 指令，覆盖从 start_addr 到
 * start_addr + size 的客户机虚拟地址范围，针对给定的 ASID 和调用 hart 的当前
 * VMID（在 hgatp CSR 中）。此函数调用仅对实现了虚拟机监控器扩展的 harts 有效。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @param asid 地址空间标识符
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_hfence_vvma_asid(umb_t hart_mask, umb_t hart_mask_base,
                                   umb_t start_addr, umb_t size, umb_t asid);

/**
 * @brief 远程 HFENCE.VVMA (FID #6)
 * 
 * 指示远程 harts 执行一个或多个 HFENCE.VVMA 指令，覆盖从 start_addr 到
 * start_addr + size 的客户机虚拟地址范围，针对调用 hart 的当前 VMID（在
 * hgatp CSR 中）和所有 ASID。此函数调用仅对实现了虚拟机监控器扩展的 harts 有效。
 *
 * @param hart_mask hart掩码
 * @param hart_mask_base hart掩码基址
 * @param start_addr 起始地址
 * @param size 大小
 * @return SBIRet 返回值
 */
SBIRet sbi_remote_hfence_vvma(umb_t hart_mask, umb_t hart_mask_base,
                              umb_t start_addr, umb_t size);

#ifdef __cplusplus
}
#endif