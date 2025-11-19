/**
 * @file csr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSR寄存器设置
 * @version alpha-1.0.0
 * @date 2025-11-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <sus/bits.h>
#include <sus/attributes.h>

typedef umb_t csr_t;

// WRPI: (Reserved Writes Preserve Values, Reads Ignore Values)
// 在读取是应忽略, 并在写入时保留原值的位

/**
 * @brief 设置CSR寄存器
 * 
 */
#define CSR_SET(name, val) \
    asm volatile ("csrs " #name ", %0" :: "r"(val));

/**
 * @brief 获得CSR寄存器值
 * 
 */
#define CSR_GET(name, ret) \
    asm volatile ("csrr %0, " #name : "=r"(ret));

/**
 * @brief 交换CSR寄存器值
 * 
 */
#define CSR_XCHG(name, val, ret) \
    asm volatile ("csrrw %0, " #name ", %1" : "=r"(ret) : "r"(val));

/**
 * @brief STVEC寄存器结构体
 * 
 */
typedef union {
    csr_t value; // 寄存器值
    umb_t ivt_addr; // IVT表地址
    struct {
        umb_t mode      : 2;   // 位0-1: 中断模式
        umb_t base      : 62;  // 位2-63:  IVT表地址基址
    } PACKED;
} csr_stvec_t;

/**
 * @brief 设置STVEC寄存器
 * 
 * @param svect STVEC寄存器
 */
static inline void csr_set_stvec(csr_stvec_t svect) {
    CSR_SET(stvec, svect.value);
}

/**
 * @brief 获取STVEC寄存器值
 * 
 * @return csr_stvec_t STVEC寄存器
 */
static inline csr_stvec_t csr_get_stvec(void) {
    csr_stvec_t svect;
    CSR_GET(stvec, svect.value);
    return svect;
}

/**
 * @brief 交换STVEC寄存器值
 * 
 * @param svect 新的STVEC寄存器值
 * @return csr_stvec_t 旧的STVEC寄存器值
 */
static inline csr_stvec_t csr_xchg_stvec(csr_stvec_t svect) {
    csr_stvec_t old;
    CSR_XCHG(stvec, svect.value, old.value);
    return old;
}

/**
 * @brief SIE寄存器结构体
 * 
 */
typedef union {
    csr_t value; // 寄存器值
    struct {
        umb_t zero0       : 1; // sie[0]
        umb_t ssie        : 1; // sie[1]     软件中断使能位
        umb_t zero1       : 3; // sie[2:4]
        umb_t stie        : 1; // sie[5]     计时器中断使能位
        umb_t zero2       : 3; // sie[6:8]
        umb_t seie        : 1; // sie[9]     外部中断使能位
        umb_t zero3       : 3; // sie[10:12]
        umb_t lcofie      : 1; // sie[13]    性能计数器溢出中断使能位
        umb_t warl        : 50;// sie[14:63] WARL
    } PACKED;
} csr_sie_t;

/**
 * @brief SIP寄存器结构体
 * 
 */
typedef union {
    csr_t value; // 寄存器值
    struct {
        umb_t zero0       : 1; // sip[0]
        umb_t ssip        : 1; // sip[1]     软件中断搁置位
        umb_t zero1       : 3; // sip[2:4]
        umb_t stip        : 1; // sip[5]     计时器中断搁置位
        umb_t zero2       : 3; // sip[6:8]
        umb_t seip        : 1; // sip[9]     外部中断搁置位
        umb_t zero3       : 3; // sip[10:12]
        umb_t lcofip      : 1; // sip[13]    性能计数器溢出中断搁置位
        umb_t reserved   : 50;// sip[14:63]
    } PACKED;
} csr_sip_t;

/**
 * @brief 设置SIE寄存器
 * 
 * @param sie SIE寄存器
 */
static inline void csr_set_sie(csr_sie_t sie) {
    CSR_SET(sie, sie.value);
}

/**
 * @brief 获取SIE寄存器值
 * 
 * @return csr_sie_t SIE寄存器
 */
static inline csr_sie_t csr_get_sie(void) {
    csr_sie_t sie;
    CSR_GET(sie, sie.value);
    return sie;
}

/**
 * @brief 交换SIE寄存器值
 * 
 * @param sie 新的SIE寄存器值
 * @return csr_sie_t 旧的SIE寄存器值
 */
static inline csr_sie_t csr_xchg_sie(csr_sie_t sie) {
    csr_sie_t old;
    CSR_XCHG(sie, sie.value, old.value);
    return old;
}

/**
 * @brief 设置SIP寄存器
 * 
 * @param sip SIP寄存器
 */
static inline void csr_set_sip(csr_sip_t sip) {
    CSR_SET(sip, sip.value);
}

/**
 * @brief 获取SIP寄存器值
 * 
 * @return csr_sip_t SIP寄存器
 */
static inline csr_sip_t csr_get_sip(void) {
    csr_sip_t sip;
    CSR_GET(sip, sip.value);
    return sip;
}

/**
 * @brief 交换SIP寄存器值
 * 
 * @param sip 新的SIP寄存器值
 * @return csr_sip_t 旧的SIP寄存器值
 */
static inline csr_sip_t csr_xchg_sip(csr_sip_t sip) {
    csr_sip_t old;
    CSR_XCHG(sip, sip.value, old.value);
    return old;
}

/**
 * @brief SCAUSE寄存器结构体
 * 
 */
typedef union {
    csr_t value; // 寄存器值
    struct {
        umb_t cause     : 63; // 异常/中断原因码
        umb_t interrupt  : 1;  // 是否为中断
    } PACKED;
} csr_scause_t;

/**
 * @brief 设置SCAUSE寄存器值
 * 
 * @param scause SCAUSE寄存器
 */
static inline void csr_set_scause(csr_scause_t scause) {
    CSR_SET(scause, scause.value);
}

/**
 * @brief 获取SCAUSE寄存器值
 * 
 * @return csr_scause_t SCAUSE寄存器
 */
static inline csr_scause_t csr_get_scause(void) {
    csr_scause_t scause;
    CSR_GET(scause, scause.value);
    return scause;
}

/**
 * @brief 交换SCAUSE寄存器值
 * 
 * @param scause 新的SCAUSE寄存器值
 * @return csr_scause_t 旧的SCAUSE寄存器值
 */
static inline csr_scause_t csr_xchg_scause(csr_scause_t scause) {
    csr_scause_t old;
    CSR_XCHG(scause, scause.value, old.value);
    return old;
}

/**
 * @brief SSTATUS寄存器结构体
 * 
 */
typedef union {
    csr_t value; // 寄存器值
    struct {
        umb_t  wrpi0 : 1;      // sstatus[0]  WRPI
        umb_t  sie   : 1;      // sstatus[1]  SIE  开启/禁用S-MODE下所有中断
        umb_t  wrpi1 : 3;      // sstatus[2:4] WRPI
        umb_t  spie  : 1;      // sstatus[5]  SPIE 中断使能位备份
        umb_t  ube   : 1;      // sstatus[6]  UBE  非对齐访问异常使能位
        umb_t  wrpi2 : 1;      // sstatus[7]  WRPI
        umb_t  spp   : 1;      // sstatus[8]  SPP  上次特权级别
        umb_t  vs    : 2;      // sstatus[9:10] VS[1:0]
        umb_t  wrpi3 : 2;      // sstatus[11:12] WRPI
        umb_t  fs    : 2;      // sstatus[13:14] FS[1:0]
        umb_t  xs    : 2;      // sstatus[15:16] XS[1:0]
        umb_t  wrpi4 : 1;      // sstatus[17] WRPI
        umb_t  sum   : 1;      // sstatus[18] SUM
        umb_t  mxr   : 1;      // sstatus[19] MXR
        umb_t  wrpi5 : 3;      // sstatus[20:22] WRPI
        umb_t  spelp : 1;      // sstatus[23] SPELP
        umb_t  sdt   : 1;      // sstatus[24] SDT
        umb_t  wrpi6 : 7;      // sstatus[25:31] WRPI
        umb_t  uxl   : 2;      // sstatus[32:33] UXL[1:0]
        umb_t wrpi7 : 29;     // sstatus[34:62] WRPI
        umb_t  sd    : 1;      // sstatus[63] SD
    } PACKED;
} csr_sstatus_t;

/**
 * @brief 设置SSTATUS寄存器值
 * 
 * @param sstatus SSTATUS寄存器
 */
static inline void csr_set_sstatus(csr_sstatus_t sstatus) {
    CSR_SET(sstatus, sstatus.value);
}

/**
 * @brief 获取SSTATUS寄存器值
 * 
 * @return csr_sstatus_t SSTATUS寄存器
 */
static inline csr_sstatus_t csr_get_sstatus(void) {
    csr_sstatus_t sstatus;
    CSR_GET(sstatus, sstatus.value);
    return sstatus;
}

/**
 * @brief 交换SSTATUS寄存器值
 * 
 * @param sstatus 新的SSTATUS寄存器值
 * @return csr_sstatus_t 旧的SSTATUS寄存器值
 */
static inline csr_sstatus_t csr_xchg_sstatus(csr_sstatus_t sstatus) {
    csr_sstatus_t old;
    CSR_XCHG(sstatus, sstatus.value, old.value);
    return old;
}

/**
 * @brief 获得计时器值
 * 
 * @return csr_t 计时器值
 */
static inline csr_t csr_get_time(void) {
    csr_t x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
}