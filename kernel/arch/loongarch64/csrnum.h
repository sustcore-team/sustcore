/**
 * @file csrnum.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSR 相关
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#define CSR_CRMD       0x00   // 当前模式信息
#define CSR_PRMD       0x01   // 例外模式信息
#define CSR_EUEN       0x02   // 扩展部件使能
#define CSR_MISC       0x03   // 余项控制
#define CSR_ECFG       0x04   // 例外配置
#define CSR_ESTAT      0x05   // 例外状态
#define CSR_ERA        0x06   // 例外返回地址
#define CSR_BADV       0x07   // 出错虚地址
#define CSR_BAD        0x08   // 出错指令
#define CSR_EENTRY     0x0c   // 例外入口地址
#define CSR_TLBIX      0x0d   // TLB索引
#define CSR_TLBEHI     0x0e   // TLB表项高位
#define CSR_TLBEH0     0x0f   // TLB表项低位0
#define CSR_TLBEH1     0x10   // TLB表项低位1
#define CSR_ASID       0x11   // 地址空间标识符
#define CSR_PGDL       0x19   // 低地址空间全局目录基址
#define CSR_PGDH       0x1a   // 高地址空间全局目录基址
#define CSR_PGD        0x1b   // 全局目录基址
#define CSR_PWCTL0     0x1c   // 页表遍历控制低半部分
#define CSR_PWCTL1     0x1d   // 页表遍历控制高半部分
#define CSR_STLBPGSIZE 0x1e   // STLB页大小
#define CSR_RVACFG     0x1f   // 缺页地址陷分配
#define CSR_CPUID      0x20   // 处理器号
#define CSR_PRCFG1     0x21   // 特权资源配置信息1
#define CSR_PRCFG2     0x22   // 特权资源配置信息2
#define CSR_PRCFG3     0x23   // 特权资源配置信息3
#define CSR_SAVE0      0x30   // 数据保存0
#define CSR_SAVE1      0x31   // 数据保存1
#define CSR_SAVE2      0x32   // 数据保存2
#define CSR_SAVE3      0x33   // 数据保存3
#define CSR_SAVE4      0x34   // 数据保存4
#define CSR_SAVE5      0x35   // 数据保存5
#define CSR_SAVE6      0x36   // 数据保存6
#define CSR_SAVE7      0x37   // 数据保存7
#define CSR_SAVE8      0x38   // 数据保存8
#define CSR_SAVE9      0x39   // 数据保存9
#define CSR_SAVE10     0x3a   // 数据保存10
#define CSR_SAVE11     0x3b   // 数据保存11
#define CSR_SAVE12     0x3c   // 数据保存12
#define CSR_SAVE13     0x3d   // 数据保存13
#define CSR_SAVE14     0x3e   // 数据保存14
#define CSR_SAVE15     0x3f   // 数据保存15
#define CSR_TID        0x40   // 定时器号
#define CSR_TCFG       0x41   // 定时器配置
#define CSR_TVAL       0x42   // 定时器值
#define CSR_CNTC       0x43   // 定时器补偿
#define CSR_TICLR      0x44   // 定时中断清除
#define CSR_LLBCTL     0x60   // LLBit控制
#define CSR_IMPCTL1    0x80   // 实现相关控制1
#define CSR_IMPCTL2    0x81   // 实现相关控制2
#define CSR_TLBRENTRY  0x88   // TLB重填例外入口地址
#define CSR_TLBRBADV   0x89   // TLB重填例外出错地址
#define CSR_TLBRERA    0x8a   // TLB重填例外返回地址
#define CSR_TLBRSAVE   0x8b   // TLB重填例外数据保存
#define CSR_TLBRBEH0   0x8c   // TLB重填表项低位0
#define CSR_TLBRBEH1   0x8d   // TLB重填表项低位1
#define CSR_TLBRBEHI   0x8e   // TLB重填表项高位
#define CSR_TLBRPRMD   0x8f   // TLB重填表项模式信息
#define CSR_MERRCTL    0x90   // 机器错误控制
#define CSR_MERRINFO1  0x91   // 机器错误信息1
#define CSR_MERRINFO2  0x92   // 机器错误信息2
#define CSR_MERRENTRY  0x93   // 机器错误例外入口地址
#define CSR_MERRERA    0x94   // 机器错误例外返回地址
#define CSR_MERRSAVE   0x95   // 机器错误例外数据保存
#define CSR_CTAG       0x96   // 高速缓存标签
#define CSR_MSCIS0     0x98   // 消息中断状态0
#define CSR_MSCIS1     0x99   // 消息中断状态1
#define CSR_MSCIS2     0x9a   // 消息中断状态2
#define CSR_MSCIS3     0x9b   // 消息中断状态3
#define CSR_MSCIREQ    0x9c   // 消息中断请求
#define CSR_MSCIE      0x9d   // 消息中断使能
#define CSR_DMWIN0     0x180  // 直接映射配置窗口0
#define CSR_DMWIN1     0x181  // 直接映射配置窗口1
#define CSR_DMWIN2     0x182  // 直接映射配置窗口2
#define CSR_DMWIN3     0x183  // 直接映射配置窗口3
#define CSR_MWPC       0x300  // Jndstree监视点整体控制
#define CSR_MWPS       0x301  // Jndstree监视点整体状态
#define CSR_MWPCFG10   0x308  // Jndstree监视点0配置1
#define CSR_MWPCFG11   0x309  // Jndstree监视点1配置1
#define CSR_MWPCFG12   0x30a  // Jndstree监视点2配置1
#define CSR_MWPCFG13   0x30b  // Jndstree监视点3配置1
#define CSR_MWPCFG20   0x30c  // Jndstree监视点0配置2
#define CSR_MWPCFG21   0x30d  // Jndstree监视点1配置2
#define CSR_MWPCFG22   0x30e  // Jndstree监视点2配置2
#define CSR_MWPCFG23   0x30f  // Jndstree监视点3配置2
#define CSR_MWPCFG30   0x310  // Jndstree监视点0配置3
#define CSR_MWPCFG31   0x311  // Jndstree监视点1配置3
#define CSR_MWPCFG32   0x312  // Jndstree监视点2配置3
#define CSR_MWPCFG33   0x313  // Jndstree监视点3配置3
#define CSR_MWPCFG40   0x314  // Jndstree监视点0配置4
#define CSR_MWPCFG41   0x315  // Jndstree监视点1配置4
#define CSR_MWPCFG42   0x316  // Jndstree监视点2配置4
#define CSR_MWPCFG43   0x317  // Jndstree监视点3配置4
#define CSR_FWPC       0x380  // 取指监视点整体控制
#define CSR_FWPS       0x381  // 取指监视点整体状态
#define CSR_FWPCFG10   0x388  // 取指监视点0配置1
#define CSR_FWPCFG11   0x389  // 取指监视点1配置1
#define CSR_FWPCFG12   0x38a  // 取指监视点2配置1
#define CSR_FWPCFG13   0x38b  // 取指监视点3配置1
#define CSR_FWPCFG20   0x38c  // 取指监视点0配置2
#define CSR_FWPCFG21   0x38d  // 取指监视点1配置2
#define CSR_FWPCFG22   0x38e  // 取指监视点2配置2
#define CSR_FWPCFG23   0x38f  // 取指监视点3配置2
#define CSR_FWPCFG30   0x390  // 取指监视点0配置3
#define CSR_FWPCFG31   0x391  // 取指监视点1配置3
#define CSR_FWPCFG32   0x392  // 取指监视点2配置3
#define CSR_FWPCFG33   0x393  // 取指监视点3配置3
#define CSR_FWPCFG40   0x394  // 取指监视点0配置4
#define CSR_FWPCFG41   0x395  // 取指监视点1配置4
#define CSR_FWPCFG42   0x396  // 取指监视点2配置4
#define CSR_FWPCFG43   0x397  // 取指监视点3配置4
#define CSR_DRE        0x500  // 调试寄存器
#define CSR_DERA       0x501  // 调试例外返回地址
#define CSR_DSAVE      0x502  // 调试数据保存

#define CRMD_PLV_MASK 0x3UL
#define CRMD_IE       (1UL << 2)
#define CRMD_DA       (1UL << 3)
#define CRMD_PG       (1UL << 4)

#define PWCTL0_4LEVEL                                                       \
    ((9UL << 25) | (30UL << 20) | (9UL << 15) | (21UL << 10) | (9UL << 5) | \
     12UL)
#define PWCTL1_4LEVEL (39UL | (9UL << 6))
#define STLBPGSIZE_4K 12UL

#define DMW_PLV0    (1UL << 0)
#define DMW_MAT_CC  (1UL << 4)
#define DMW0_BASE   0x8000000000000000ULL
#define DMW0_CONFIG (DMW0_BASE | DMW_MAT_CC | DMW_PLV0)

#define EUEN_FPE  (1UL << 0)
#define EUEN_SXE  (1UL << 1)
#define EUEN_ASXE (1UL << 2)

#define ESTAT_IS_SHIFT    0
#define ESTAT_IS_MASK     0x1fffUL
#define ESTAT_ECODE_SHIFT 16
#define ESTAT_ECODE_MASK  0x3fUL
#define ECFG_VS_MASK      (0x7UL << 16)

#define ECODE_INT  0
#define ECODE_SYS  0xb
#define ECODE_FPD  0xf
#define ECODE_SXD  0x10
#define ECODE_ASXD 0x11

#define INT_TIMER  11
#define ECFG_TIMER (1UL << INT_TIMER)

#define TCFG_EN            (1UL << 0)
#define TCFG_PERIODIC      (1UL << 1)
#define TCFG_INITVAL_SHIFT 2

#define TICLR_CLR (1UL << 0)

#define PRMD_PPLV_MASK 0x3UL
#define PRMD_PIE       (1UL << 2)

#define PLV_KERNEL 0UL
#define PLV_USER   3UL
#define PRMD_USER  (PLV_USER | PRMD_PIE)

#define ECODE_PIL 0x1
#define ECODE_PIS 0x2
#define ECODE_PIF 0x3
#define ECODE_PME 0x4
#define ECODE_PNR 0x5
#define ECODE_PNX 0x6
#define ECODE_PPI 0x7

#define IOCSR_IPI_STATUS 0x1000
#define IOCSR_IPI_EN     0x1004
#define IOCSR_IPI_SET    0x1008
#define IOCSR_IPI_CLEAR  0x100c
#define IOCSR_MBUF0      0x1020
#define IOCSR_MBUF1      0x1028
#define IOCSR_MBUF2      0x1030
#define IOCSR_MBUF3      0x1038
#define IOCSR_IPI_SEND   0x1040
#define IOCSR_MBUF_SEND  0x1048

#define IOCSR_IPI_SEND_IP_SHIFT   0
#define IOCSR_IPI_SEND_CPU_SHIFT  16
#define IOCSR_IPI_SEND_BLOCKING   (1UL << 31)
#define IOCSR_MBUF_SEND_BLOCKING  (1ULL << 31)
#define IOCSR_MBUF_SEND_BOX_SHIFT 2
#define IOCSR_MBUF_SEND_CPU_SHIFT 16