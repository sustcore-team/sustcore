/**
 * @file csr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSR
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/csrnum.h>
#include <features/attributes.h>
#include <sus/macros.h>
#include <sus/types.h>

using csr_t   = qword;
using csr32_t = dword;

#define LA64_CSR_READ(csr)                                       \
    ({                                                           \
        csr_t __v;                                               \
        asm volatile("csrrd %0, " SCSTRINGIFY(csr) : "=r"(__v)); \
        __v;                                                     \
    })

#define LA64_CSR_WRITE(csr, val)                                  \
    ({                                                            \
        csr_t __v = (val);                                        \
        asm volatile("csrwr %0, " SCSTRINGIFY(csr) : : "r"(__v)); \
    })

#define LA64_CSR_SWAP(csr, val)                                  \
    ({                                                           \
        csr_t __v = (val);                                       \
        asm volatile("csrwr %0, " SCSTRINGIFY(csr) : "+r"(__v)); \
        __v;                                                     \
    })

#define LA64_CSR_SET(csr, mask)                          \
    ({                                                   \
        csr_t __m = (mask);                              \
        asm volatile("csrxchg %0, %0, " SCSTRINGIFY(csr) \
                     : "+r"(__m)                         \
                     :                                   \
                     : "memory");                        \
    })

#define LA64_CSR_CLEAR(csr, mask)                           \
    ({                                                      \
        csr_t __m = (mask);                                 \
        asm volatile("csrxchg $zero, %0, " SCSTRINGIFY(csr) \
                     :                                      \
                     : "r"(__m)                             \
                     : "memory");                           \
    })

/* ===================== CSR结构体定义 ===================== */

// 7.4.1 当前模式信息 (CRMD)
union csr_crmd_t {
    csr_t value;
    struct {
        uint64_t plv : 2;    // [1:0]  当前特权等级
        uint64_t ie : 1;     // [2]    全局中断使能
        uint64_t da : 1;     // [3]    直接地址翻译使能
        uint64_t pg : 1;     // [4]    映射地址翻译使能
        uint64_t datf : 2;   // [6:5]  取指访问类型
        uint64_t datm : 2;   // [8:7]  load/store访问类型
        uint64_t we : 1;     // [9]    监视点使能
        uint64_t rsvd : 54;  // [63:10] 保留
    } PACKED;
};

#define __OPCSR__ static __ATTR_ALWAYS_INLINE__

__OPCSR__ void csr_set_crmd(csr_crmd_t crmd) {
    LA64_CSR_WRITE(CSR_CRMD, crmd.value);
}

__OPCSR__ csr_crmd_t csr_get_crmd() {
    return {LA64_CSR_READ(CSR_CRMD)};
}

__OPCSR__ csr_crmd_t csr_xchg_crmd(csr_crmd_t crmd) {
    return {LA64_CSR_SWAP(CSR_CRMD, crmd.value)};
}

// 7.4.2 例外前模式信息 (PRMD)
union csr_prmd_t {
    csr_t value;
    struct {
        uint64_t pplv : 2;   // [1:0]
        uint64_t pie : 1;    // [2]
        uint64_t pwe : 1;    // [3]
        uint64_t rsvd : 60;  // [63:4]
    } PACKED;
};
__OPCSR__ void csr_set_prmd(csr_prmd_t prmd) {
    LA64_CSR_WRITE(CSR_PRMD, prmd.value);
}
__OPCSR__ csr_prmd_t csr_get_prmd() {
    return {LA64_CSR_READ(CSR_PRMD)};
}
__OPCSR__ csr_prmd_t csr_xchg_prmd(csr_prmd_t prmd) {
    return {LA64_CSR_SWAP(CSR_PRMD, prmd.value)};
}

// 7.4.3 扩展部件使能 (EUEN)
union csr_euen_t {
    csr_t value;
    struct {
        uint64_t fpe : 1;    // [0] 浮点
        uint64_t sxe : 1;    // [1] 128位向量
        uint64_t asxe : 1;   // [2] 256位向量
        uint64_t bte : 1;    // [3] 二进制翻译
        uint64_t rsvd : 60;  // [63:4]
    } PACKED;
};
__OPCSR__ void csr_set_euen(csr_euen_t euen) {
    LA64_CSR_WRITE(CSR_EUEN, euen.value);
}
__OPCSR__ csr_euen_t csr_get_euen() {
    return {LA64_CSR_READ(CSR_EUEN)};
}
__OPCSR__ csr_euen_t csr_xchg_euen(csr_euen_t euen) {
    return {LA64_CSR_SWAP(CSR_EUEN, euen.value)};
}

// 7.4.4 杂项 (MISC)
union csr_misc_t {
    csr_t value;
    struct {
        uint64_t rsvd0 : 1;   // [0]
        uint64_t va32l1 : 1;  // [1] PLV1 32位地址模式
        uint64_t va32l2 : 1;  // [2] PLV2
        uint64_t va32l3 : 1;  // [3] PLV3
        uint64_t rsvd1 : 1;   // [4]
        uint64_t drdtl1 : 1;  // [5] PLV1禁用RDTIME
        uint64_t drdtl2 : 1;  // [6] PLV2
        uint64_t drdtl3 : 1;  // [7] PLV3
        uint64_t rsvd2 : 56;  // [63:8]
    } PACKED;
};
__OPCSR__ void csr_set_misc(csr_misc_t misc) {
    LA64_CSR_WRITE(CSR_MISC, misc.value);
}
__OPCSR__ csr_misc_t csr_get_misc() {
    return {LA64_CSR_READ(CSR_MISC)};
}
__OPCSR__ csr_misc_t csr_xchg_misc(csr_misc_t misc) {
    return {LA64_CSR_SWAP(CSR_MISC, misc.value)};
}

// 7.4.5 例外配置 (ECFG)
union csr_ecfg_t {
    csr_t value;
    struct {
        uint64_t lie : 13;    // [12:0] 局部中断使能
        uint64_t rsvd0 : 3;   // [15:13]
        uint64_t vs : 3;      // [18:16] 中断向量间距
        uint64_t rsvd1 : 45;  // [63:19]
    } PACKED;
};
__OPCSR__ void csr_set_ecfg(csr_ecfg_t ecfg) {
    LA64_CSR_WRITE(CSR_ECFG, ecfg.value);
}
__OPCSR__ csr_ecfg_t csr_get_ecfg() {
    return {LA64_CSR_READ(CSR_ECFG)};
}
__OPCSR__ csr_ecfg_t csr_xchg_ecfg(csr_ecfg_t ecfg) {
    return {LA64_CSR_SWAP(CSR_ECFG, ecfg.value)};
}

// 7.4.6 例外状态 (ESTAT)
union csr_estat_t {
    csr_t value;
    struct {
        uint64_t is0 : 2;      // [1:0] 软件中断SW10/SW11
        uint64_t is12_2 : 11;  // [12:2] 其他中断状态
        uint64_t rsvd0 : 1;    // [13]
        uint64_t msgint : 1;   // [14] 消息中断
        uint64_t rsvd1 : 1;    // [15]
        uint64_t ecode : 6;    // [21:16] 例外一级编码
        uint64_t esub : 9;     // [30:22] 例外二级编码
        uint64_t rsvd2 : 33;   // [63:31]
    } PACKED;
};
__OPCSR__ void csr_set_estat(csr_estat_t estat) {
    LA64_CSR_WRITE(CSR_ESTAT, estat.value);
}
__OPCSR__ csr_estat_t csr_get_estat() {
    return {LA64_CSR_READ(CSR_ESTAT)};
}
__OPCSR__ csr_estat_t csr_xchg_estat(csr_estat_t estat) {
    return {LA64_CSR_SWAP(CSR_ESTAT, estat.value)};
}

// 7.4.7 例外返回地址 (ERA) - 64位
union csr_era_t {
    csr_t value;
    struct {
        uint64_t pc : 64;
    } PACKED;
};
__OPCSR__ void csr_set_era(csr_era_t era) {
    LA64_CSR_WRITE(CSR_ERA, era.value);
}
__OPCSR__ csr_era_t csr_get_era() {
    return {LA64_CSR_READ(CSR_ERA)};
}
__OPCSR__ csr_era_t csr_xchg_era(csr_era_t era) {
    return {LA64_CSR_SWAP(CSR_ERA, era.value)};
}

// 7.4.8 出错虚地址 (BADV) - 64位
union csr_badv_t {
    csr_t value;
    struct {
        uint64_t vaddr : 64;
    } PACKED;
};
__OPCSR__ void csr_set_badv(csr_badv_t badv) {
    LA64_CSR_WRITE(CSR_BADV, badv.value);
}
__OPCSR__ csr_badv_t csr_get_badv() {
    return {LA64_CSR_READ(CSR_BADV)};
}
__OPCSR__ csr_badv_t csr_xchg_badv(csr_badv_t badv) {
    return {LA64_CSR_SWAP(CSR_BADV, badv.value)};
}

// 7.4.9 出错指令 (BADI) - 32位有效
union csr_badi_t {
    csr_t value;
    struct {
        uint64_t inst : 32;
        uint64_t rsvd : 32;
    } PACKED;
};
__OPCSR__ void csr_set_badi(csr_badi_t badi) {
    LA64_CSR_WRITE(CSR_BADI, badi.value);
}
__OPCSR__ csr_badi_t csr_get_badi() {
    return {LA64_CSR_READ(CSR_BADI)};
}
__OPCSR__ csr_badi_t csr_xchg_badi(csr_badi_t badi) {
    return {LA64_CSR_SWAP(CSR_BADI, badi.value)};
}

// 7.4.10 例外入口地址 (EENTRY) - 页号
union csr_eentry_t {
    csr_t value;
    struct {
        uint64_t rsvd0 : 12;  // [11:0]
        uint64_t vpn : 52;    // [63:12] 页号
    } PACKED;
};
__OPCSR__ void csr_set_eentry(csr_eentry_t eentry) {
    LA64_CSR_WRITE(CSR_EENTRY, eentry.value);
}
__OPCSR__ csr_eentry_t csr_get_eentry() {
    return {LA64_CSR_READ(CSR_EENTRY)};
}
__OPCSR__ csr_eentry_t csr_xchg_eentry(csr_eentry_t eentry) {
    return {LA64_CSR_SWAP(CSR_EENTRY, eentry.value)};
}

// 7.5.1 TLB索引 (TLBIDX)
union csr_tlbidx_t {
    csr_t value;
    struct {
        uint64_t index : 16;  // [15:0] 索引（实际位宽依实现）
        uint64_t rsvd0 : 8;   // [23:16]
        uint64_t ps : 6;      // [29:24] 页大小
        uint64_t rsvd1 : 1;   // [30]
        uint64_t n : 1;       // [31] 无效表项指示
        uint64_t rsvd2 : 32;  // [63:32]
    } PACKED;
};
__OPCSR__ void csr_set_tlbidx(csr_tlbidx_t t) {
    LA64_CSR_WRITE(CSR_TLBIDX, t.value);
}
__OPCSR__ csr_tlbidx_t csr_get_tlbidx() {
    return {LA64_CSR_READ(CSR_TLBIDX)};
}
__OPCSR__ csr_tlbidx_t csr_xchg_tlbidx(csr_tlbidx_t t) {
    return {LA64_CSR_SWAP(CSR_TLBIDX, t.value)};
}

// 7.5.2 TLB表项高位 (TLBEHI) - LA64，假设VALEN=48
union csr_tlbehi_t {
    csr_t value;
    struct {
        uint64_t rsvd0 : 13;  // [12:0]
        uint64_t vppn : 35;   // [47:13] 虚页号 (VALEN-1:13)
        uint64_t sext : 16;   // [63:48] 符号扩展（写忽略）
    } PACKED;
};
__OPCSR__ void csr_set_tlbehi(csr_tlbehi_t t) {
    LA64_CSR_WRITE(CSR_TLBEHI, t.value);
}
__OPCSR__ csr_tlbehi_t csr_get_tlbehi() {
    return {LA64_CSR_READ(CSR_TLBEHI)};
}
__OPCSR__ csr_tlbehi_t csr_xchg_tlbehi(csr_tlbehi_t t) {
    return {LA64_CSR_SWAP(CSR_TLBEHI, t.value)};
}

// 7.5.3 TLB表项低位 (TLBELO0/1) - LA64，假设PALEN=48
union csr_tlbelo_t {
    csr_t value;
    struct {
        uint64_t v : 1;       // [0]
        uint64_t d : 1;       // [1]
        uint64_t plv : 2;     // [3:2]
        uint64_t mat : 2;     // [5:4]
        uint64_t g : 1;       // [6]
        uint64_t rsvd0 : 5;   // [11:7]
        uint64_t ppn : 36;    // [47:12] 物理页号 (PALEN-1:12)
        uint64_t rsvd1 : 13;  // [60:48]
        uint64_t nr : 1;      // [61]
        uint64_t nx : 1;      // [62]
        uint64_t rplv : 1;    // [63]
    } PACKED;
};
__OPCSR__ void csr_set_tlbelo0(csr_tlbelo_t t) {
    LA64_CSR_WRITE(CSR_TLBELO0, t.value);
}
__OPCSR__ csr_tlbelo_t csr_get_tlbelo0() {
    return {LA64_CSR_READ(CSR_TLBELO0)};
}
__OPCSR__ csr_tlbelo_t csr_xchg_tlbelo0(csr_tlbelo_t t) {
    return {LA64_CSR_SWAP(CSR_TLBELO0, t.value)};
}
__OPCSR__ void csr_set_tlbelo1(csr_tlbelo_t t) {
    LA64_CSR_WRITE(CSR_TLBELO1, t.value);
}
__OPCSR__ csr_tlbelo_t csr_get_tlbelo1() {
    return {LA64_CSR_READ(CSR_TLBELO1)};
}
__OPCSR__ csr_tlbelo_t csr_xchg_tlbelo1(csr_tlbelo_t t) {
    return {LA64_CSR_SWAP(CSR_TLBELO1, t.value)};
}

// 7.5.4 地址空间标识符 (ASID)
union csr_asid_t {
    csr_t value;
    struct {
        uint64_t asid : 10;     // [9:0]
        uint64_t rsvd0 : 6;     // [15:10]
        uint64_t asidbits : 8;  // [23:16] ASID位宽
        uint64_t rsvd1 : 40;    // [63:24]
    } PACKED;
};
__OPCSR__ void csr_set_asid(csr_asid_t a) {
    LA64_CSR_WRITE(CSR_ASID, a.value);
}
__OPCSR__ csr_asid_t csr_get_asid() {
    return {LA64_CSR_READ(CSR_ASID)};
}
__OPCSR__ csr_asid_t csr_xchg_asid(csr_asid_t a) {
    return {LA64_CSR_SWAP(CSR_ASID, a.value)};
}

// 7.5.5/6/7 全局目录基址 (PGDL/PGDH/PGD)
// PGDL/PGDH 可读写，PGD只读
union csr_pgd_t {
    csr_t value;
    struct {
        uint64_t rsvd : 12;
        uint64_t base : 52;  // 基址页号
    } PACKED;
};
__OPCSR__ void csr_set_pgdl(csr_pgd_t p) {
    LA64_CSR_WRITE(CSR_PGDL, p.value);
}
__OPCSR__ csr_pgd_t csr_get_pgdl() {
    return {LA64_CSR_READ(CSR_PGDL)};
}
__OPCSR__ csr_pgd_t csr_xchg_pgdl(csr_pgd_t p) {
    return {LA64_CSR_SWAP(CSR_PGDL, p.value)};
}
__OPCSR__ void csr_set_pgdh(csr_pgd_t p) {
    LA64_CSR_WRITE(CSR_PGDH, p.value);
}
__OPCSR__ csr_pgd_t csr_get_pgdh() {
    return {LA64_CSR_READ(CSR_PGDH)};
}
__OPCSR__ csr_pgd_t csr_xchg_pgdh(csr_pgd_t p) {
    return {LA64_CSR_SWAP(CSR_PGDH, p.value)};
}
__OPCSR__ csr_pgd_t csr_get_pgd() {
    return {LA64_CSR_READ(CSR_PGD)};
}

// 7.5.8 页表遍历控制低半部分 (PWCL)
union csr_pwcl_t {
    csr_t value;
    struct {
        uint64_t ptbase : 5;    // [4:0]
        uint64_t ptw : 5;       // [9:5]
        uint64_t dir1base : 5;  // [14:10]
        uint64_t dir1w : 5;     // [19:15]
        uint64_t dir2base : 5;  // [24:20]
        uint64_t dir2w : 5;     // [29:25]
        uint64_t ptew : 2;      // [31:30]
        uint64_t rsvd : 32;     // [63:32]
    } PACKED;
};
__OPCSR__ void csr_set_pwcl(csr_pwcl_t p) {
    LA64_CSR_WRITE(CSR_PWCL, p.value);
}
__OPCSR__ csr_pwcl_t csr_get_pwcl() {
    return {LA64_CSR_READ(CSR_PWCL)};
}
__OPCSR__ csr_pwcl_t csr_xchg_pwcl(csr_pwcl_t p) {
    return {LA64_CSR_SWAP(CSR_PWCL, p.value)};
}

// 7.5.9 页表遍历控制高半部分 (PWCH)
union csr_pwch_t {
    csr_t value;
    struct {
        uint64_t dir3base : 6;  // [5:0]
        uint64_t dir3w : 6;     // [11:6]
        uint64_t dir4base : 6;  // [17:12]
        uint64_t dir4w : 6;     // [23:18]
        uint64_t
            hptw_en : 1;  // [24]
                          // 硬件页表遍历使能（若硬件不支持，写无效，读返回0）
        uint64_t rsvd : 39;  // [63:25] 保留
    } PACKED;
};

__OPCSR__ void csr_set_pwch(csr_pwch_t p) {
    LA64_CSR_WRITE(CSR_PWCH, p.value);
}
__OPCSR__ csr_pwch_t csr_get_pwch(void) {
    return {LA64_CSR_READ(CSR_PWCH)};
}
__OPCSR__ csr_pwch_t csr_xchg_pwch(csr_pwch_t p) {
    return {LA64_CSR_SWAP(CSR_PWCH, p.value)};
}

// 7.5.10 STLB页大小 (STLBPS)
union csr_stlbps_t {
    csr_t value;
    struct {
        uint64_t ps : 6;  // [5:0]
        uint64_t rsvd : 58;
    } PACKED;
};
__OPCSR__ void csr_set_stlbps(csr_stlbps_t s) {
    LA64_CSR_WRITE(CSR_STLBPS, s.value);
}
__OPCSR__ csr_stlbps_t csr_get_stlbps() {
    return {LA64_CSR_READ(CSR_STLBPS)};
}
__OPCSR__ csr_stlbps_t csr_xchg_stlbps(csr_stlbps_t s) {
    return {LA64_CSR_SWAP(CSR_STLBPS, s.value)};
}

// 7.4.11 缩减虚地址配置 (RVACFG)
union csr_rvacfg_t {
    csr_t value;
    struct {
        uint64_t rbit : 4;  // [3:0]
        uint64_t rsvd : 60;
    } PACKED;
};
__OPCSR__ void csr_set_rvacfg(csr_rvacfg_t r) {
    LA64_CSR_WRITE(CSR_RVACFG, r.value);
}
__OPCSR__ csr_rvacfg_t csr_get_rvacfg() {
    return {LA64_CSR_READ(CSR_RVACFG)};
}
__OPCSR__ csr_rvacfg_t csr_xchg_rvacfg(csr_rvacfg_t r) {
    return {LA64_CSR_SWAP(CSR_RVACFG, r.value)};
}

// 7.4.12 处理器编号 (CPUID)
union csr_cpuid_t {
    csr_t value;
    struct {
        uint64_t coreid : 9;  // [8:0]
        uint64_t rsvd : 55;
    } PACKED;
};
__OPCSR__ csr_cpuid_t csr_get_cpuid() {
    return {LA64_CSR_READ(CSR_CPUID)};
}

// 7.4.13 特权资源配置信息1 (PRCFG1)
union csr_prcfg1_t {
    csr_t value;
    struct {
        uint64_t savenum : 4;    // [3:0]
        uint64_t timerbits : 8;  // [11:4]
        uint64_t vsmax : 3;      // [14:12]
        uint64_t rsvd : 49;
    } PACKED;
};
__OPCSR__ csr_prcfg1_t csr_get_prcfg1() {
    return {LA64_CSR_READ(CSR_PRCFG1)};
}

// 7.4.14 特权资源配置信息2 (PRCFG2)
union csr_prcfg2_t {
    csr_t value;
    struct {
        uint64_t psavl : 64;  // 每一位表示支持一种页大小
    } PACKED;
};
__OPCSR__ csr_prcfg2_t csr_get_prcfg2() {
    return {LA64_CSR_READ(CSR_PRCFG2)};
}

// 7.4.15 PRCFG3 - 仅保留
union csr_prcfg3_t {
    csr_t value;
    struct {
        uint64_t rsvd : 64;
    } PACKED;
};
__OPCSR__ csr_prcfg3_t csr_get_prcfg3() {
    return {LA64_CSR_READ(CSR_PRCFG3)};
}

// 数据保存寄存器
using csr_save_t = csr_t;

__OPCSR__ void csr_set_save0(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE0, v);
}
__OPCSR__ csr_save_t csr_get_save0() {
    return LA64_CSR_READ(CSR_SAVE0);
}
__OPCSR__ csr_save_t csr_xchg_save0(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE0, v);
}

__OPCSR__ void csr_set_save1(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE1, v);
}
__OPCSR__ csr_save_t csr_get_save1() {
    return LA64_CSR_READ(CSR_SAVE1);
}
__OPCSR__ csr_save_t csr_xchg_save1(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE1, v);
}

__OPCSR__ void csr_set_save2(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE2, v);
}
__OPCSR__ csr_save_t csr_get_save2() {
    return LA64_CSR_READ(CSR_SAVE2);
}
__OPCSR__ csr_save_t csr_xchg_save2(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE2, v);
}

__OPCSR__ void csr_set_save3(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE3, v);
}
__OPCSR__ csr_save_t csr_get_save3() {
    return LA64_CSR_READ(CSR_SAVE3);
}
__OPCSR__ csr_save_t csr_xchg_save3(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE3, v);
}

__OPCSR__ void csr_set_save4(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE4, v);
}
__OPCSR__ csr_save_t csr_get_save4() {
    return LA64_CSR_READ(CSR_SAVE4);
}
__OPCSR__ csr_save_t csr_xchg_save4(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE4, v);
}

__OPCSR__ void csr_set_save5(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE5, v);
}
__OPCSR__ csr_save_t csr_get_save5() {
    return LA64_CSR_READ(CSR_SAVE5);
}
__OPCSR__ csr_save_t csr_xchg_save5(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE5, v);
}

__OPCSR__ void csr_set_save6(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE6, v);
}
__OPCSR__ csr_save_t csr_get_save6() {
    return LA64_CSR_READ(CSR_SAVE6);
}
__OPCSR__ csr_save_t csr_xchg_save6(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE6, v);
}

__OPCSR__ void csr_set_save7(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE7, v);
}
__OPCSR__ csr_save_t csr_get_save7() {
    return LA64_CSR_READ(CSR_SAVE7);
}
__OPCSR__ csr_save_t csr_xchg_save7(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE7, v);
}

__OPCSR__ void csr_set_save8(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE8, v);
}
__OPCSR__ csr_save_t csr_get_save8() {
    return LA64_CSR_READ(CSR_SAVE8);
}
__OPCSR__ csr_save_t csr_xchg_save8(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE8, v);
}

__OPCSR__ void csr_set_save9(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE9, v);
}
__OPCSR__ csr_save_t csr_get_save9() {
    return LA64_CSR_READ(CSR_SAVE9);
}
__OPCSR__ csr_save_t csr_xchg_save9(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE9, v);
}

__OPCSR__ void csr_set_save10(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE10, v);
}
__OPCSR__ csr_save_t csr_get_save10() {
    return LA64_CSR_READ(CSR_SAVE10);
}
__OPCSR__ csr_save_t csr_xchg_save10(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE10, v);
}

__OPCSR__ void csr_set_save11(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE11, v);
}
__OPCSR__ csr_save_t csr_get_save11() {
    return LA64_CSR_READ(CSR_SAVE11);
}
__OPCSR__ csr_save_t csr_xchg_save11(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE11, v);
}

__OPCSR__ void csr_set_save12(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE12, v);
}
__OPCSR__ csr_save_t csr_get_save12() {
    return LA64_CSR_READ(CSR_SAVE12);
}
__OPCSR__ csr_save_t csr_xchg_save12(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE12, v);
}

__OPCSR__ void csr_set_save13(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE13, v);
}
__OPCSR__ csr_save_t csr_get_save13() {
    return LA64_CSR_READ(CSR_SAVE13);
}
__OPCSR__ csr_save_t csr_xchg_save13(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE13, v);
}

__OPCSR__ void csr_set_save14(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE14, v);
}
__OPCSR__ csr_save_t csr_get_save14() {
    return LA64_CSR_READ(CSR_SAVE14);
}
__OPCSR__ csr_save_t csr_xchg_save14(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE14, v);
}

__OPCSR__ void csr_set_save15(csr_save_t v) {
    LA64_CSR_WRITE(CSR_SAVE15, v);
}
__OPCSR__ csr_save_t csr_get_save15() {
    return LA64_CSR_READ(CSR_SAVE15);
}
__OPCSR__ csr_save_t csr_xchg_save15(csr_save_t v) {
    return LA64_CSR_SWAP(CSR_SAVE15, v);
}

// 7.6 定时器相关
// TID (0x40)
union csr_tid_t {
    csr_t value;
    struct {
        uint64_t tid : 32;
        uint64_t rsvd : 32;
    } PACKED;
};
__OPCSR__ void csr_set_tid(csr_tid_t t) {
    LA64_CSR_WRITE(CSR_TID, t.value);
}
__OPCSR__ csr_tid_t csr_get_tid() {
    return {LA64_CSR_READ(CSR_TID)};
}
__OPCSR__ csr_tid_t csr_xchg_tid(csr_tid_t t) {
    return {LA64_CSR_SWAP(CSR_TID, t.value)};
}

// TCFG (0x41) - n由实现决定，假设最大32位
union csr_tcfg_t {
    csr_t value;
    struct {
        uint64_t en : 1;        // [0]
        uint64_t periodic : 1;  // [1]
        uint64_t initval : 30;  // [31:2] 假设n≤32
        uint64_t rsvd : 32;     // [63:32]
    } PACKED;
};
__OPCSR__ void csr_set_tcfg(csr_tcfg_t t) {
    LA64_CSR_WRITE(CSR_TCFG, t.value);
}
__OPCSR__ csr_tcfg_t csr_get_tcfg() {
    return {LA64_CSR_READ(CSR_TCFG)};
}
__OPCSR__ csr_tcfg_t csr_xchg_tcfg(csr_tcfg_t t) {
    return {LA64_CSR_SWAP(CSR_TCFG, t.value)};
}

// TVAL (0x42)
union csr_tval_t {
    csr_t value;
    struct {
        uint64_t timeval : 32;  // 假设有效位数
        uint64_t rsvd : 32;
    } PACKED;
};
__OPCSR__ csr_tval_t csr_get_tval() {
    return {LA64_CSR_READ(CSR_TVAL)};
}

// CNTC (0x43)
using csr_cntc_t = csr_t;

__OPCSR__ void csr_set_cntc(csr_cntc_t c) {
    LA64_CSR_WRITE(CSR_CNTC, c);
}
__OPCSR__ csr_cntc_t csr_get_cntc() {
    return LA64_CSR_READ(CSR_CNTC);
}
__OPCSR__ csr_cntc_t csr_xchg_cntc(csr_cntc_t c) {
    return LA64_CSR_SWAP(CSR_CNTC, c);
}

// TICLR (0x44) - 写1清中断
union csr_ticlr_t {
    csr_t value;
    struct {
        uint64_t clr : 1;  // [0] 写1清除
        uint64_t rsvd : 63;
    } PACKED;
};

__OPCSR__ void csr_set_ticlr(csr_ticlr_t t) {
    LA64_CSR_WRITE(CSR_TICLR, t.value);
}

// 7.7 RAS相关
// MERRCTL (0x90)
union csr_merrctl_t {
    csr_t value;
    struct {
        uint64_t ismerr : 1;        // [0]
        uint64_t isrepairable : 1;  // [1]
        uint64_t pplv : 2;          // [3:2]
        uint64_t pie : 1;           // [4]
        uint64_t rsvd0 : 1;         // [5]
        uint64_t pwe : 1;           // [6]
        uint64_t pda : 1;           // [7]
        uint64_t ppg : 1;           // [8]
        uint64_t pdatf : 2;         // [10:9]
        uint64_t pdatm : 2;         // [12:11]
        uint64_t rsvd1 : 3;         // [15:13]
        uint64_t cause : 8;         // [23:16]
        uint64_t rsvd2 : 40;        // [63:24]
    } PACKED;
};
__OPCSR__ csr_merrctl_t csr_get_merrctl() {
    return {LA64_CSR_READ(CSR_MERRCTL)};
}
__OPCSR__ void csr_set_merrctl(csr_merrctl_t m) {
    LA64_CSR_WRITE(CSR_MERRCTL, m.value);
}
__OPCSR__ csr_merrctl_t csr_xchg_merrctl(csr_merrctl_t m) {
    return {LA64_CSR_SWAP(CSR_MERRCTL, m.value)};
}

// MERRINFO1/2 - 实现相关，简单64位
using csr_merrinfo_t = csr_t;

__OPCSR__ csr_merrinfo_t csr_get_merrinfo1() {
    return LA64_CSR_READ(CSR_MERRINFO1);
}
__OPCSR__ csr_merrinfo_t csr_get_merrinfo2() {
    return LA64_CSR_READ(CSR_MERRINFO2);
}

// MERRENTRY (0x93) - 类似TLBRENTRY
union csr_merr_entry_t {
    csr_t value;
    struct {
        uint64_t rsvd0 : 12;
        uint64_t ppn : 36;  // 假设PALEN=48
        uint64_t rsvd1 : 16;
    } PACKED;
};
__OPCSR__ void csr_set_merrentry(csr_merr_entry_t m) {
    LA64_CSR_WRITE(CSR_MERRENTRY, m.value);
}
__OPCSR__ csr_merr_entry_t csr_get_merrentry() {
    return {LA64_CSR_READ(CSR_MERRENTRY)};
}
__OPCSR__ csr_merr_entry_t csr_xchg_merrentry(csr_merr_entry_t m) {
    return {LA64_CSR_SWAP(CSR_MERRENTRY, m.value)};
}

// MERRERA (0x94) - 64位PC
using csr_merrera_t = csr_t;
__OPCSR__ void csr_set_merrera(csr_merrera_t m) {
    LA64_CSR_WRITE(CSR_MERRERA, m);
}
__OPCSR__ csr_merrera_t csr_get_merrera() {
    return LA64_CSR_READ(CSR_MERRERA);
}
__OPCSR__ csr_merrera_t csr_xchg_merrera(csr_merrera_t m) {
    return LA64_CSR_SWAP(CSR_MERRERA, m);
}

// MERRSAVE (0x95)
using csr_merrsave_t = csr_t;
__OPCSR__ void csr_set_merrsave(csr_merrsave_t m) {
    LA64_CSR_WRITE(CSR_MERRSAVE, m);
}
__OPCSR__ csr_merrsave_t csr_get_merrsave() {
    return LA64_CSR_READ(CSR_MERRSAVE);
}
__OPCSR__ csr_merrsave_t csr_xchg_merrsave(csr_merrsave_t m) {
    return LA64_CSR_SWAP(CSR_MERRSAVE, m);
}

// 7.5.11 TLB重填例外入口地址 (TLBRENTRY)
union csr_tlbr_entry_t {
    csr_t value;
    struct {
        uint64_t rsvd0 : 12;
        uint64_t ppn : 36;  // 假设PALEN=48
        uint64_t rsvd1 : 16;
    } PACKED;
};
__OPCSR__ void csr_set_tlbr_entry(csr_tlbr_entry_t t) {
    LA64_CSR_WRITE(CSR_TLBRENTRY, t.value);
}
__OPCSR__ csr_tlbr_entry_t csr_get_tlbr_entry() {
    return {LA64_CSR_READ(CSR_TLBRENTRY)};
}
__OPCSR__ csr_tlbr_entry_t csr_xchg_tlbr_entry(csr_tlbr_entry_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRENTRY, t.value)};
}

// TLBRBADV (0x89)
using csr_tlbr_badv_t = csr_t;
__OPCSR__ csr_tlbr_badv_t csr_get_tlbr_badv() {
    return LA64_CSR_READ(CSR_TLBRBADV);
}

// TLBRERA (0x8a)
union csr_tlbr_era_t {
    csr_t value;
    struct {
        uint64_t istlbr : 1;  // [0]
        uint64_t rsvd0 : 1;   // [1]
        uint64_t pc : 62;     // [63:2] 返回地址
    } PACKED;
};
__OPCSR__ void csr_set_tlbr_era(csr_tlbr_era_t t) {
    LA64_CSR_WRITE(CSR_TLBRERA, t.value);
}
__OPCSR__ csr_tlbr_era_t csr_get_tlbr_era() {
    return {LA64_CSR_READ(CSR_TLBRERA)};
}
__OPCSR__ csr_tlbr_era_t csr_xchg_tlbr_era(csr_tlbr_era_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRERA, t.value)};
}

// TLBRSAVE (0x8b)
using csr_tlbr_save_t = csr_t;
__OPCSR__ void csr_set_tlbr_save(csr_tlbr_save_t t) {
    LA64_CSR_WRITE(CSR_TLBRSAVE, t);
}
__OPCSR__ csr_tlbr_save_t csr_get_tlbr_save() {
    return LA64_CSR_READ(CSR_TLBRSAVE);
}
__OPCSR__ csr_tlbr_save_t csr_xchg_tlbr_save(csr_tlbr_save_t t) {
    return LA64_CSR_SWAP(CSR_TLBRSAVE, t);
}

// TLBRLO0/1 - 与TLBELO0/1结构相同，但访问不同CSR地址
using csr_tlbrlo_t = csr_tlbelo_t;
__OPCSR__ void csr_set_tlbrlo0(csr_tlbrlo_t t) {
    LA64_CSR_WRITE(CSR_TLBRLO0, t.value);
}
__OPCSR__ csr_tlbrlo_t csr_get_tlbrlo0() {
    return {LA64_CSR_READ(CSR_TLBRLO0)};
}
__OPCSR__ csr_tlbrlo_t csr_xchg_tlbrlo0(csr_tlbrlo_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRLO0, t.value)};
}
__OPCSR__ void csr_set_tlbrlo1(csr_tlbrlo_t t) {
    LA64_CSR_WRITE(CSR_TLBRLO1, t.value);
}
__OPCSR__ csr_tlbrlo_t csr_get_tlbrlo1() {
    return {LA64_CSR_READ(CSR_TLBRLO1)};
}
__OPCSR__ csr_tlbrlo_t csr_xchg_tlbrlo1(csr_tlbrlo_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRLO1, t.value)};
}

// TLBRHI (0x8e) - 类似TLBEHI但含PS域
union csr_tlbrhi_t {
    csr_t value;
    struct {
        uint64_t ps : 6;     // [5:0]
        uint64_t rsvd0 : 7;  // [12:6]
        uint64_t vppn : 35;  // [47:13]
        uint64_t sext : 16;  // [63:48]
    } PACKED;
};
__OPCSR__ void csr_set_tlbrhi(csr_tlbrhi_t t) {
    LA64_CSR_WRITE(CSR_TLBRHI, t.value);
}
__OPCSR__ csr_tlbrhi_t csr_get_tlbrhi() {
    return {LA64_CSR_READ(CSR_TLBRHI)};
}
__OPCSR__ csr_tlbrhi_t csr_xchg_tlbrhi(csr_tlbrhi_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRHI, t.value)};
}

// TLBRPRMD (0x8f) - 与PRMD类似但多了保留位3?
union csr_tlbrprmd_t {
    csr_t value;
    struct {
        uint64_t pplv : 2;   // [1:0]
        uint64_t pie : 1;    // [2]
        uint64_t rsvd0 : 1;  // [3] (若未实现虚拟化)
        uint64_t pwe : 1;    // [4]
        uint64_t rsvd : 59;
    } PACKED;
};
__OPCSR__ void csr_set_tlbrprmd(csr_tlbrprmd_t t) {
    LA64_CSR_WRITE(CSR_TLBRPRMD, t.value);
}
__OPCSR__ csr_tlbrprmd_t csr_get_tlbrprmd() {
    return {LA64_CSR_READ(CSR_TLBRPRMD)};
}
__OPCSR__ csr_tlbrprmd_t csr_xchg_tlbrprmd(csr_tlbrprmd_t t) {
    return {LA64_CSR_SWAP(CSR_TLBRPRMD, t.value)};
}

// 7.8 性能监测相关（只提供简单结构）
// PMCFG和PMCNT地址连续，此处不展开，可按需定义。

// 7.9 监视点相关 - 结构复杂，此处给出MWPC/MWPS示例，其余类似。
union csr_mwpc_t {
    csr_t value;
    struct {
        uint64_t num : 6;  // [5:0]
        uint64_t rsvd : 58;
    } PACKED;
};
__OPCSR__ csr_mwpc_t csr_get_mwpc() {
    return {LA64_CSR_READ(CSR_MWPC)};
}

union csr_mwps_t {
    csr_t value;
    struct {
        uint64_t status : 14;  // 假设最多14个监视点
        uint64_t rsvd0 : 2;    // [15:14]
        uint64_t skip : 1;     // [16]
        uint64_t rsvd1 : 47;
    } PACKED;
};
__OPCSR__ csr_mwps_t csr_get_mwps() {
    return {LA64_CSR_READ(CSR_MWPS)};
}
__OPCSR__ void csr_set_mwps(csr_mwps_t m) {
    LA64_CSR_WRITE(CSR_MWPS, m.value);
}
__OPCSR__ csr_mwps_t csr_xchg_mwps(csr_mwps_t m) {
    return {LA64_CSR_SWAP(CSR_MWPS, m.value)};
}

// 监视点配置寄存器结构（适用于MWPnCFG1~4）
union csr_mwpcfg1_t {
    csr_t value;
    struct {
        uint64_t vaddr : 64;
    } PACKED;
};
union csr_mwpcfg2_t {
    csr_t value;
    struct {
        uint64_t mask : 64;
    } PACKED;
};
union csr_mwpcfg3_t {
    csr_t value;
    struct {
        uint64_t dsonly : 1;
        uint64_t plv0 : 1;
        uint64_t plv1 : 1;
        uint64_t plv2 : 1;
        uint64_t plv3 : 1;
        uint64_t rsvd0 : 2;
        uint64_t lcl : 1;
        uint64_t loaden : 1;
        uint64_t storen : 1;
        uint64_t size : 2;
        uint64_t rsvd1 : 52;
    } PACKED;
};
union csr_mwpcfg4_t {
    csr_t value;
    struct {
        uint64_t asid : 10;
        uint64_t rsvd : 54;
    } PACKED;
};

// 由于监视点有多个，这里不逐个定义函数，可按需。

// 7.10 调试相关
// DBG (0x500)
union csr_dbg_t {
    csr_t value;
    struct {
        uint64_t ds : 1;      // [0]
        uint64_t drev : 7;    // [7:1]
        uint64_t dei : 1;     // [8]
        uint64_t dcl : 1;     // [9]
        uint64_t dfw : 1;     // [10]
        uint64_t dmw : 1;     // [11]
        uint64_t rsvd0 : 4;   // [15:12]
        uint64_t ecode : 6;   // [21:16]
        uint64_t rsvd1 : 42;  // [63:22]
    } PACKED;
};
__OPCSR__ csr_dbg_t csr_get_dbg() {
    return {LA64_CSR_READ(CSR_DBG)};
}
__OPCSR__ void csr_set_dbg(csr_dbg_t d) {
    LA64_CSR_WRITE(CSR_DBG, d.value);
}
__OPCSR__ csr_dbg_t csr_xchg_dbg(csr_dbg_t d) {
    return {LA64_CSR_SWAP(CSR_DBG, d.value)};
}

// DERA (0x501)
using csr_dera_t = csr_t;
__OPCSR__ void csr_set_dera(csr_dera_t d) {
    LA64_CSR_WRITE(CSR_DERA, d);
}
__OPCSR__ csr_dera_t csr_get_dera() {
    return LA64_CSR_READ(CSR_DERA);
}
__OPCSR__ csr_dera_t csr_xchg_dera(csr_dera_t d) {
    return LA64_CSR_SWAP(CSR_DERA, d);
}

// DSAVE (0x502)
using csr_dsave_t = csr_t;
__OPCSR__ void csr_set_dsave(csr_dsave_t d) {
    LA64_CSR_WRITE(CSR_DSAVE, d);
}
__OPCSR__ csr_dsave_t csr_get_dsave() {
    return LA64_CSR_READ(CSR_DSAVE);
}
__OPCSR__ csr_dsave_t csr_xchg_dsave(csr_dsave_t d) {
    return LA64_CSR_SWAP(CSR_DSAVE, d);
}

// 7.11 消息中断
// MSGIS0~3 - 64位状态
using csr_msgis_t = csr_t;
__OPCSR__ csr_msgis_t csr_get_msgis0() {
    return LA64_CSR_READ(CSR_MSGIS0);
}
__OPCSR__ csr_msgis_t csr_get_msgis1() {
    return LA64_CSR_READ(CSR_MSGIS1);
}
__OPCSR__ csr_msgis_t csr_get_msgis2() {
    return LA64_CSR_READ(CSR_MSGIS2);
}
__OPCSR__ csr_msgis_t csr_get_msgis3() {
    return LA64_CSR_READ(CSR_MSGIS3);
}

// MSGIR (0x9c)
union csr_msgir_t {
    csr_t value;
    struct {
        uint64_t intnum : 8;  // [7:0]
        uint64_t rsvd0 : 23;  // [30:8]
        uint64_t null : 1;    // [31]
        uint64_t rsvd1 : 32;  // [63:32]
    } PACKED;
};
__OPCSR__ csr_msgir_t csr_get_msgir() {
    return {LA64_CSR_READ(CSR_MSGIR)};
}

// MSGIE (0x9d)
union csr_msgie_t {
    csr_t value;
    struct {
        uint64_t pt : 8;  // [7:0] Priority Threshold
        uint64_t rsvd : 56;
    } PACKED;
};
__OPCSR__ void csr_set_msgie(csr_msgie_t m) {
    LA64_CSR_WRITE(CSR_MSGIE, m.value);
}
__OPCSR__ csr_msgie_t csr_get_msgie() {
    return {LA64_CSR_READ(CSR_MSGIE)};
}
__OPCSR__ csr_msgie_t csr_xchg_msgie(csr_msgie_t m) {
    return {LA64_CSR_SWAP(CSR_MSGIE, m.value)};
}

// 7.5.18 直接映射配置窗口 (DMW0~3)
union csr_dmw_t {
    csr_t value;
    struct {
        uint64_t plv0 : 1;   // [0]
        uint64_t plv1 : 1;   // [1]
        uint64_t plv2 : 1;   // [2]
        uint64_t plv3 : 1;   // [3]
        uint64_t mat : 2;    // [5:4]
        uint64_t rsvd : 54;  // [59:6]
        uint64_t vseg : 4;   // [63:60] 虚地址段
    } PACKED;
};
__OPCSR__ void csr_set_dmw0(csr_dmw_t d) {
    LA64_CSR_WRITE(CSR_DMWIN0, d.value);
}
__OPCSR__ csr_dmw_t csr_get_dmw0() {
    return {LA64_CSR_READ(CSR_DMWIN0)};
}
__OPCSR__ csr_dmw_t csr_xchg_dmw0(csr_dmw_t d) {
    return {LA64_CSR_SWAP(CSR_DMWIN0, d.value)};
}

__OPCSR__ void csr_set_dmw1(csr_dmw_t d) {
    LA64_CSR_WRITE(CSR_DMWIN1, d.value);
}
__OPCSR__ csr_dmw_t csr_get_dmw1() {
    return {LA64_CSR_READ(CSR_DMWIN1)};
}
__OPCSR__ csr_dmw_t csr_xchg_dmw1(csr_dmw_t d) {
    return {LA64_CSR_SWAP(CSR_DMWIN1, d.value)};
}

__OPCSR__ void csr_set_dmw2(csr_dmw_t d) {
    LA64_CSR_WRITE(CSR_DMWIN2, d.value);
}
__OPCSR__ csr_dmw_t csr_get_dmw2() {
    return {LA64_CSR_READ(CSR_DMWIN2)};
}
__OPCSR__ csr_dmw_t csr_xchg_dmw2(csr_dmw_t d) {
    return {LA64_CSR_SWAP(CSR_DMWIN2, d.value)};
}

__OPCSR__ void csr_set_dmw3(csr_dmw_t d) {
    LA64_CSR_WRITE(CSR_DMWIN3, d.value);
}
__OPCSR__ csr_dmw_t csr_get_dmw3() {
    return {LA64_CSR_READ(CSR_DMWIN3)};
}
__OPCSR__ csr_dmw_t csr_xchg_dmw3(csr_dmw_t d) {
    return {LA64_CSR_SWAP(CSR_DMWIN3, d.value)};
}

#undef __OPCSR__