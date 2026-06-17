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

#include <features/attributes.h>
#include <sus/macros.h>
#include <sus/types.h>

using csr_t   = umb_t;
using csr32_t = dword;

/**
 * @brief 写入 CSR 寄存器.
 */
#define RV64_CSR_WRITE(csr, val)                                    \
    ({                                                              \
        csr_t __v = (val);                                          \
        asm volatile("csrw " SCSTRINGIFY(csr) ", %0" : : "r"(__v)); \
    })

/**
 * @brief 读取 CSR 寄存器.
 */
#define RV64_CSR_READ(csr)                                      \
    ({                                                          \
        csr_t __v;                                              \
        asm volatile("csrr %0, " SCSTRINGIFY(csr) : "=r"(__v)); \
        __v;                                                    \
    })

/**
 * @brief 交换 CSR 寄存器值并返回旧值.
 */
#define RV64_CSR_SWAP(csr, val)                           \
    ({                                                    \
        csr_t __v = (val);                                \
        asm volatile("csrrw %0, " SCSTRINGIFY(csr) ", %1" \
                     : "+r"(__v)                          \
                     : "r"(__v)                           \
                     : "memory");                         \
        __v;                                              \
    })

/**
 * @brief 对 CSR 掩码置位.
 */
#define RV64_CSR_SET(csr, mask)                             \
    ({                                                      \
        csr_t __m = (mask);                                 \
        asm volatile("csrrs zero, " SCSTRINGIFY(csr) ", %0" \
                     :                                      \
                     : "r"(__m)                             \
                     : "memory");                           \
    })

/**
 * @brief 对 CSR 掩码清位.
 */
#define RV64_CSR_CLEAR(csr, mask)                           \
    ({                                                      \
        csr_t __m = (mask);                                 \
        asm volatile("csrrc zero, " SCSTRINGIFY(csr) ", %0" \
                     :                                      \
                     : "r"(__m)                             \
                     : "memory");                           \
    })

#define __OPCSR__ static __ATTR_ALWAYS_INLINE__

// WRPI: (Reserved Writes Preserve Values, Reads Ignore Values)
// 在读取时应忽略, 并在写入时保留原值的位

enum class UXL : umb_t {
    RESERVERD = 0,  // 保留
    _32       = 1,  // XLEN = 32
    _64       = 2,  // XLEN = 64
    _128      = 3   // XLEN = 128
};

/**
 * @brief vs, fs, xs 字段枚举
 *
 */
enum XSStatus : umb_t {
    OFF     = 0,  // 关闭
    INITIAL = 1,  // 初始化
    CLEAN   = 2,  // 干净
    DIRTY   = 3   // 脏
};

/**
 * @brief SSTATUS寄存器结构体
 *
 * Supervisor Status Register
 *
 */
union csr_sstatus_t {
    csr_t value;  // 寄存器值
    struct {
        umb_t wrpi0 : 1;  // sstatus[0]  WRPI
        umb_t sie : 1;    // sstatus[1]  SIE  开启/禁用S-MODE下所有中断
        umb_t wrpi1 : 3;  // sstatus[2:4] WRPI
        umb_t spie : 1;   // sstatus[5]  SPIE 中断使能位备份
        umb_t ube : 1;    // sstatus[6]  UBE User Big-Endian (大端模式)
        umb_t wrpi2 : 1;  // sstatus[7]  WRPI
        umb_t spp : 1;    // sstatus[8]  SPP  上次特权级别
        XSStatus vs : 2;  // sstatus[9:10] VS[1:0] Vector Status (向量扩展状态)
        umb_t wrpi3 : 2;  // sstatus[11:12] WRPI
        XSStatus fs : 2;  // sstatus[13:14] FS[1:0] Float Status (浮点扩展状态)
        XSStatus xs : 2;  // sstatus[15:16] XS[1:0] Extension Status (扩展状态)
        umb_t wrpi4 : 1;  // sstatus[17] WRPI
        umb_t sum : 1;    // sstatus[18] SUM Supervisor User Memory Access
                          // (允许S-MODE访问U-MODE内存)
        umb_t
            mxr : 1;  // sstatus[19] MXR Make eXecutable Readable (可执行页可读)
        umb_t wrpi5 : 3;  // sstatus[20:22] WRPI
        umb_t spelp : 1;  // sstatus[23] SPELP Supervisor Previous(SP) Expected
                          // Landing Pad(ELP) (预期落地点)
        umb_t sdt : 1;  // sstatus[24] SDT S-Mode Disable Trap (禁用S-MODE陷阱)
                        // 控制double-trap. 只在SDT = 0时发送中断并令SDT = 1.
                        // 当且仅当sstatus.sdt = 0时, 会deliver一个trap,
                        // 并在此时令sstatus.sdt = 1 此时sstatus.sie自动清零 ;
                        // 调用sret则将sstatus.sie恢复到sstatus.spie的值,
                        // 将sstatus.sdt复位为0.
        umb_t wrpi6 : 7;  // sstatus[25:31] WRPI
        UXL uxl : 2;  // sstatus[32:33] UXL[1:0] User XLEN (用户模式下的XLEN)
        umb_t wrpi7 : 29;  // sstatus[34:62] WRPI
        umb_t sd : 1;      // sstatus[63] SD Supervisor Dirty (脏状态)
                           // 说明是否有任一扩展处于dirty状态
    } PACKED;
};

/**
 * @brief 设置SSTATUS寄存器值
 *
 * @param sstatus SSTATUS寄存器
 */
__OPCSR__ void csr_set_sstatus(csr_sstatus_t sstatus) {
    RV64_CSR_WRITE(sstatus, sstatus.value);
}

/**
 * @brief 获取SSTATUS寄存器值
 *
 * @return csr_sstatus_t SSTATUS寄存器
 */
__OPCSR__ csr_sstatus_t csr_get_sstatus() {
    return {RV64_CSR_READ(sstatus)};
}

/**
 * @brief 交换SSTATUS寄存器值
 *
 * @param sstatus 新的SSTATUS寄存器值
 * @return csr_sstatus_t 旧的SSTATUS寄存器值
 */
__OPCSR__ csr_sstatus_t csr_xchg_sstatus(csr_sstatus_t sstatus) {
    return {RV64_CSR_SWAP(sstatus, sstatus.value)};
}

/**
 * @brief STVEC寄存器结构体
 *
 * Supervisor Trap Vector Base Address Register
 *
 */
union csr_stvec_t {
    csr_t value;     // 寄存器值
    umb_t ivt_addr;  // IVT表地址
    struct {
        umb_t mode : 2;   // 位0-1: 中断模式
        umb_t base : 62;  // 位2-63:  IVT表地址基址
    } PACKED;
};

/**
 * @brief 设置STVEC寄存器
 *
 * @param stvec STVEC寄存器
 */
__OPCSR__ void csr_set_stvec(csr_stvec_t stvec) {
    RV64_CSR_WRITE(stvec, stvec.value);
}

/**
 * @brief 获取STVEC寄存器值
 *
 * @return csr_stvec_t STVEC寄存器
 */
__OPCSR__ csr_stvec_t csr_get_stvec() {
    return {RV64_CSR_READ(stvec)};
}

/**
 * @brief 交换STVEC寄存器值
 *
 * @param stvec 新的STVEC寄存器值
 * @return csr_stvec_t 旧的STVEC寄存器值
 */
__OPCSR__ csr_stvec_t csr_xchg_stvec(csr_stvec_t stvec) {
    return {RV64_CSR_SWAP(stvec, stvec.value)};
}

// SIE说明哪些中断允许被触发
// 软件只能通过SIE寄存器来允许或禁止中断

// SIP说明哪些中断满足触发条件, 但还未触发
// SIP寄存器的位只能通过硬件设置, 软件无法修改
// 当对应位被设置后, 如果满足条件(例如SIE寄存器中对应的位也被设置),
// 则会触发相应的中断

/**
 * @brief SIE寄存器结构体
 *
 * Supervisor Interrupt Enable Register
 *
 */
union csr_sie_t {
    csr_t value;  // 寄存器值
    struct {
        umb_t zero0 : 1;   // sie[0]
        umb_t ssie : 1;    // sie[1]     软件中断使能位
        umb_t zero1 : 3;   // sie[2:4]
        umb_t stie : 1;    // sie[5]     计时器中断使能位
        umb_t zero2 : 3;   // sie[6:8]
        umb_t seie : 1;    // sie[9]     外部中断使能位
        umb_t zero3 : 3;   // sie[10:12]
        umb_t lcofie : 1;  // sie[13]    性能计数器溢出中断使能位
        umb_t warl : 50;   // sie[14:63] WARL
    } PACKED;
};

/**
 * @brief SIP寄存器结构体
 *
 * Supervisor Interrupt Pending Register
 *
 */
union csr_sip_t {
    csr_t value;  // 寄存器值
    struct {
        umb_t zero0 : 1;      // sip[0]
        umb_t ssip : 1;       // sip[1]     软件中断未决位
        umb_t zero1 : 3;      // sip[2:4]
        umb_t stip : 1;       // sip[5]     计时器中断未决位
        umb_t zero2 : 3;      // sip[6:8]
        umb_t seip : 1;       // sip[9]     外部中断未决位
        umb_t zero3 : 3;      // sip[10:12]
        umb_t lcofip : 1;     // sip[13]    性能计数器溢出中断未决位
        umb_t reserved : 50;  // sip[14:63]
    } PACKED;
};

/**
 * @brief 设置SIE寄存器
 *
 * @param sie SIE寄存器
 */
__OPCSR__ void csr_set_sie(csr_sie_t sie) {
    RV64_CSR_WRITE(sie, sie.value);
}

/**
 * @brief 获取SIE寄存器值
 *
 * @return csr_sie_t SIE寄存器
 */
__OPCSR__ csr_sie_t csr_get_sie() {
    return {RV64_CSR_READ(sie)};
}

/**
 * @brief 交换SIE寄存器值
 *
 * @param sie 新的SIE寄存器值
 * @return csr_sie_t 旧的SIE寄存器值
 */
__OPCSR__ csr_sie_t csr_xchg_sie(csr_sie_t sie) {
    return {RV64_CSR_SWAP(sie, sie.value)};
}

/**
 * @brief 设置SIP寄存器
 *
 * @param sip SIP寄存器
 */
__OPCSR__ void csr_set_sip(csr_sip_t sip) {
    RV64_CSR_WRITE(sip, sip.value);
}

/**
 * @brief 获取SIP寄存器值
 *
 * @return csr_sip_t SIP寄存器
 */
__OPCSR__ csr_sip_t csr_get_sip() {
    return {RV64_CSR_READ(sip)};
}

/**
 * @brief 交换SIP寄存器值
 *
 * @param sip 新的SIP寄存器值
 * @return csr_sip_t 旧的SIP寄存器值
 */
__OPCSR__ csr_sip_t csr_xchg_sip(csr_sip_t sip) {
    return {RV64_CSR_SWAP(sip, sip.value)};
}

/**
 * @brief SCOUNTEREN寄存器结构体
 *
 * Supervisor Counter Enable Register
 *
 */
union csr_scounteren_t {
    csr32_t value;
    struct {
        umb_t hpm[32];  // [0:31] HPM计数器使能位
    } PACKED;
    struct {
        umb_t cy : 1;        // [0] Cycle计数器使能位
        umb_t tm : 1;        // [1] Time计数器使能位
        umb_t ir : 1;        // [2] Instret计数器使能位
        umb_t rest_hpm[29];  // [3:31] 其余HPM计数器使能位
    } PACKED;
};

/**
 * @brief 设置SCOUNTEREN寄存器
 *
 * @param scounteren SCOUNTEREN寄存器
 */
__OPCSR__ void csr_set_scounteren(csr_scounteren_t scounteren) {
    RV64_CSR_WRITE(scounteren, scounteren.value);
}

/**
 * @brief 获取SCOUNTEREN寄存器值
 *
 * @return csr_scounteren_t SCOUNTEREN寄存器
 */
__OPCSR__ csr_scounteren_t csr_get_scounteren() {
    return {static_cast<csr32_t>(RV64_CSR_READ(scounteren))};
}

/**
 * @brief 交换SCOUNTEREN寄存器值
 *
 * @param scounteren    新的SCOUNTEREN寄存器值
 * @return csr_scounteren_t 旧的SCOUNTEREN寄存器值
 */
__OPCSR__ csr_scounteren_t csr_xchg_scounteren(csr_scounteren_t scounteren) {
    return {static_cast<csr32_t>(RV64_CSR_SWAP(scounteren, scounteren.value))};
}

/**
 * @brief SSCRATCH寄存器类型
 *
 * Supervisor Scratch Register
 *
 */
using csr_sscratch_t = csr_t;

/**
 * @brief 设置SSCRATCH寄存器
 *
 * @param sscratch SSCRATCH寄存器
 */
__OPCSR__ void csr_set_sscratch(csr_sscratch_t sscratch) {
    RV64_CSR_WRITE(sscratch, sscratch);
}

/**
 * @brief 获取SSCRATCH寄存器值
 *
 * @return csr_sscratch_t SSCRATCH寄存器
 */
__OPCSR__ csr_sscratch_t csr_get_sscratch() {
    return RV64_CSR_READ(sscratch);
}

/**
 * @brief 交换SSCRATCH寄存器值
 *
 * @param sscratch 新的SSCRATCH寄存器值
 * @return csr_sscratch_t 旧的SSCRATCH寄存器值
 */
__OPCSR__ csr_sscratch_t csr_xchg_sscratch(csr_sscratch_t sscratch) {
    return RV64_CSR_SWAP(sscratch, sscratch);
}

/**
 * @brief SEPC寄存器类型
 *
 * Supervisor Exception Program Counter Register
 *
 */
using csr_sepc_t = csr_t;

/**
 * @brief 设置SEPC寄存器
 *
 * @param sepc SEPC寄存器
 */
__OPCSR__ void csr_set_sepc(csr_sepc_t sepc) {
    RV64_CSR_WRITE(sepc, sepc);
}

/**
 * @brief 获取SEPC寄存器值
 *
 * @return csr_sepc_t SEPC寄存器
 */
__OPCSR__ csr_sepc_t csr_get_sepc() {
    return RV64_CSR_READ(sepc);
}

/**
 * @brief 交换SEPC寄存器值
 *
 * @param sepc 新的SEPC寄存器值
 * @return csr_sepc_t 旧的SEPC寄存器值
 */
__OPCSR__ csr_sepc_t csr_xchg_sepc(csr_sepc_t sepc) {
    return RV64_CSR_SWAP(sepc, sepc);
}

/**
 * @brief SCAUSE寄存器结构体
 *
 * Supervisor Cause Register
 *
 */
union csr_scause_t {
    csr_t value;  // 寄存器值
    struct {
        umb_t cause : 63;     // 异常/中断原因码
        umb_t interrupt : 1;  // 是否为中断
    } PACKED;
};

/**
 * @brief 设置SCAUSE寄存器值
 *
 * @param scause SCAUSE寄存器
 */
__OPCSR__ void csr_set_scause(csr_scause_t scause) {
    RV64_CSR_WRITE(scause, scause.value);
}

/**
 * @brief 获取SCAUSE寄存器值
 *
 * @return csr_scause_t SCAUSE寄存器
 */
__OPCSR__ csr_scause_t csr_get_scause() {
    return {RV64_CSR_READ(scause)};
}

/**
 * @brief 交换SCAUSE寄存器值
 *
 * @param scause 新的SCAUSE寄存器值
 * @return csr_scause_t 旧的SCAUSE寄存器值
 */
__OPCSR__ csr_scause_t csr_xchg_scause(csr_scause_t scause) {
    return {RV64_CSR_SWAP(scause, scause.value)};
}

/**
 * @brief STVAL寄存器类型
 *
 * Supervisor Trap Value Register
 *
 */
using csr_stval_t = csr_t;

/**
 * @brief 设置STVAL寄存器
 *
 * @param stval STVAL寄存器
 */
__OPCSR__ void csr_set_stval(csr_stval_t stval) {
    RV64_CSR_WRITE(stval, stval);
}

/**
 * @brief 获取STVAL寄存器值
 *
 * @return csr_stval_t STVAL寄存器
 */
__OPCSR__ csr_stval_t csr_get_stval() {
    return RV64_CSR_READ(stval);
}

/**
 * @brief 交换STVAL寄存器值
 *
 * @param stval 新的STVAL寄存器值
 * @return csr_stval_t 旧的STVAL寄存器值
 */
__OPCSR__ csr_stval_t csr_xchg_stval(csr_stval_t stval) {
    return RV64_CSR_SWAP(stval, stval);
}

/**
 * @brief SENVCFG寄存器结构体
 *
 * Supervisor Environment Configuration Register
 *
 */
union csr_senvcfg_t {
    csr_t value;
    struct {
        umb_t fiom : 1;    // [0] Fence of I/O implies Memory
        umb_t wrpi0 : 1;   // [1] WRPI
        umb_t lpe : 1;     // [2]
        umb_t sse : 1;     // [3]
        umb_t cbie : 2;    // [4:5]
        umb_t cbcfe : 1;   // [6]
        umb_t cbze : 1;    // [7]
        umb_t wrpi1 : 24;  // [8:31] WRPI
        umb_t
            pmm : 2;  // [32:33] Pause Mask in M-Mode M-Mode下是否屏蔽PAUSE指令
        umb_t wrpi2 : 27;  // [34:60]
        umb_t pbmt : 1;    // [61] PBMTE
        umb_t wrpi3 : 2;   // [62:63]
    } PACKED;
};

/**
 * @brief 获取SENVCFG寄存器值
 *
 * @return csr_senvcfg_t SENVCFG寄存器
 */
__OPCSR__ csr_senvcfg_t csr_get_senvcfg() {
    return {RV64_CSR_READ(senvcfg)};
}

/**
 * @brief 设置SENVCFG寄存器值
 *
 * @param senvcfg SENVCFG寄存器
 */
__OPCSR__ void csr_set_senvcfg(csr_senvcfg_t senvcfg) {
    RV64_CSR_WRITE(senvcfg, senvcfg.value);
}

/**
 * @brief 交换SENVCFG寄存器值
 *
 * @param senvcfg SENVCFG寄存器
 * @return csr_senvcfg_t 旧的SENVCFG寄存器值
 */
__OPCSR__ csr_senvcfg_t csr_xchg_senvcfg(csr_senvcfg_t senvcfg) {
    return {RV64_CSR_SWAP(senvcfg, senvcfg.value)};
}

enum class SATPMode : umb_t {
    BARE = 0,   // 物理地址模式
    SV39 = 8,   // 39位虚拟地址(512G)
    SV48 = 9,   // 48位虚拟地址(256TB)
    SV57 = 10,  // 57位虚拟地址(128PB)
    SV64 = 11   // 64位虚拟地址(16EB)
};

/**
 * @brief SATP寄存器结构体
 * Supervisor Address Translation and Protection
 */
union csr_satp_t {
    csr_t value;
    struct {
        umb_t ppn : 44;     // 位[0:43]:  根页表的物理页号
        umb_t asid : 16;    // 位[44:59]: 地址空间标识符
        SATPMode mode : 4;  // 位[60:63]: 地址转换模式
    } PACKED;
};

/**
 * @brief 设置SATP寄存器
 *
 * @param satp SATP寄存器
 */
__OPCSR__ void csr_set_satp(csr_satp_t satp) {
    RV64_CSR_WRITE(satp, satp.value);
}

/**
 * @brief 获取SATP寄存器值
 *
 * @return csr_satp_t SATP寄存器
 */
__OPCSR__ csr_satp_t csr_get_satp() {
    return {RV64_CSR_READ(satp)};
}

/**
 * @brief 交换SATP寄存器值
 *
 * @param satp 新的SATP寄存器值
 * @return csr_satp_t 旧的SATP寄存器值
 */
__OPCSR__ csr_satp_t csr_xchg_satp(csr_satp_t satp) {
    return {RV64_CSR_SWAP(satp, satp.value)};
}

/**
 * @brief 获得计时器值
 *
 * @return csr_t 计时器值
 */
__OPCSR__ csr_t csr_get_time() {
    return RV64_CSR_READ(time);
}

#undef __OPCSR__