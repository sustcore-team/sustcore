/**
 * @file sbi_enum.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI相关枚举值
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

enum {
    SBI_SUCCESS               =  0,
    SBI_ERR_FAILED            = -1,
    SBI_ERR_NOT_SUPPORTED     = -2,
    SBI_ERR_INVALID_PARAM     = -3,
    SBI_ERR_DENIED            = -4,
    SBI_ERR_INVALID_ADDRESS   = -5,
    SBI_ERR_ALREADY_AVAILABLE = -6,
    SBI_ERR_ALREADY_STARTED   = -7,
    SBI_ERR_ALREADY_STOPPED   = -8,
    SBI_ERR_NO_SHMEM          = -9,
    SBI_ERR_INVALID_STATE     = -10,
    SBI_ERR_BAD_RANGE         = -11,
    SBI_ERR_TIMEOUT           = -12,
    SBI_ERR_IO                = -13,
    SBI_ERR_DENIED_LOCK       = -14
};

// SBI Extension IDs
// Reference: RISC-V Supervisor Binary Interface Specification
// Version v3.0, 2025-07-16: Ratified
enum EID {
    // Legacy SBI Calls starts
    SBI_EID_LEGACY_START           = 0x00000000,
    SBI_EID_SET_TIMER              = 0x00000000,
    SBI_EID_CONSOLE_PUTCHAR        = 0x00000001,
    SBI_EID_CONSOLE_GETCHAR        = 0x00000002,
    SBI_EID_CLEAR_IPI              = 0x00000003,
    SBI_EID_SEND_IPI               = 0x00000004,
    SBI_EID_REMOTE_FENCE_I         = 0x00000005,
    SBI_EID_REMOTE_SFENCE_VMA      = 0x00000006,
    SBI_EID_REMOTE_SFENCE_VMA_ASID = 0x00000007,
    SBI_EID_SHUTDOWN               = 0x00000008,
    SBI_EID_LEGACY_END             = 0x0000000F,
    // Legacy SBI Calls ends
    SBI_EID_BASE                   = 0x00000010,
    SBI_EID_TIME                   = 0x54494D45,
    SBI_EID_SPI                    = 0x00735049,
    SBI_EID_RFNC                   = 0x52464E43,
    SBI_EID_HSM                    = 0x0048534D,
    SBI_EID_SRST                   = 0x53525354,
    SBI_EID_PMU                    = 0x00504D55,
    SBI_EID_DBCN                   = 0x4442434E,
    SBI_EID_SUSP                   = 0x53555350,
    SBI_EID_CPPC                   = 0x43505043,
    SBI_EID_NACL                   = 0x4E41434C,
    SBI_EID_STA                    = 0x00535441,
    SBI_EID_SSE                    = 0x00535345,
    SBI_EID_FWFT                   = 0x46574654,
    SBI_EID_DBTR                   = 0x44425452,
    SBI_EID_MPXY                   = 0x4D505859,
    // Expermimental SBI Extension Space
    SBI_EID_EXPERIMENTAL_START     = 0x08000000,
    SBI_EID_EXPERIMENTAL_END       = 0x08FFFFFF,
    // Vendor Specific Extension Space
    SBI_EID_VENDOR_START           = 0x09000000,
    SBI_EID_VENDOR_END             = 0x09FFFFFF,
    // Firmware Specific Extension Space
    SBI_EID_FIRMWARE_START         = 0x0A000000,
    SBI_EID_FIRMWARE_END           = 0x0AFFFFFF,
};

// SBI Base Function IDs
enum FID {
    // Base: 基础 SBI Calls
    SBI_GET_SPEC_VERSION = 0x00000000,
    SBI_GET_IMPL_ID      = 0x00000001,
    SBI_GET_IMPL_VERSION = 0x00000002,
    SBI_PROBE_EXTENSION  = 0x00000003,
    SBI_GET_MVENDORID    = 0x00000004,
    SBI_GET_MARCHID      = 0x00000005,
    SBI_GET_MIMPID       = 0x00000006,

    // DBCN: Debug Console
    SBI_CONSOLE_WRITE      = 0x00000000,
    SBI_CONSOLE_READ       = 0x00000001,
    SBI_CONSOLE_WRITE_BYTE = 0x00000002,

    // TIME: Timer Management
    SBI_SET_TIMER = 0x00000000,

    // SPI: Send IPI
    SBI_SEND_IPI = 0x00000000,

    // RFNC: Remote Fence
    SBI_REMOTE_FENCE_I          = 0x00000000,
    SBI_REMOTE_SFENCE_VMA       = 0x00000001,
    SBI_REMOTE_SFENCE_VMA_ASID  = 0x00000002,
    SBI_REMOTE_HFENCE_GVMA_VMID = 0x00000003,
    SBI_REMOTE_HFENCE_GVMA      = 0x00000004,
    SBI_REMOTE_HFENCE_VVMA_ASID = 0x00000005,
    SBI_REMOTE_HFENCE_VVMA      = 0x00000006,

    // HSM: Hart State Management
    SBI_HART_START       = 0x00000000,
    SBI_HART_STOP        = 0x00000001,
    SBI_HART_GET_STATUS  = 0x00000002,
    SBI_HART_SUSPEND     = 0x00000003,
};

// 所有可能的 HSM 状态
enum HSM_STATE {
    STARTED         = 0,  // Hart 已物理上电并正常执行
    STOPPED         = 1,  // Hart 未在监控模式及更低权限模式，或已被 SBI 下电
    START_PENDING   = 2,  // 已请求启动该 hart，仍在努力使其进入 STARTED
    STOP_PENDING    = 3,  // 已请求停止或下电自身，仍在努力使其进入 STOPPED
    SUSPENDED       = 4,  // 此 hart 处于平台特定的挂起（或低功耗）状态
    SUSPEND_PENDING = 5,  // 已请求挂起该 hart，仍在努力使其进入 SUSPENDED
    RESUME_PENDING  = 6   // 已请求恢复该 hart，仍在努力使其进入 STARTED
};

#ifdef __cplusplus
}
#endif