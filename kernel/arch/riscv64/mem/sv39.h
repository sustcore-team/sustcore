/**
 * @file sv39.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SV39页表管理实现
 * @version alpha-1.0.0
 * @date 2026-01-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/csr.h>
#include <arch/trait.h>
#include <kio.h>
#include <mem/addr.h>
#include <mem/gfp.h>
#include <sus/logger.h>
#include <sus/types.h>

#include <cassert>
#include <cstring>

enum class Riscv64SV39RWX : umb_t {
    // 基本权限
    P    = 0b000,
    R    = 0b001,
    W    = 0b010,
    X    = 0b100,
    // 组合权限
    RO   = R,
    RW   = R | W,
    RX   = R | X,
    RWX  = R | W | X,
    // 空
    NONE = 0b000,
};

constexpr bool operator&(Riscv64SV39RWX lhs, Riscv64SV39RWX rhs) {
    return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}

constexpr Riscv64SV39RWX operator|(Riscv64SV39RWX lhs, Riscv64SV39RWX rhs) {
    return static_cast<Riscv64SV39RWX>(static_cast<uint8_t>(lhs) |
                                       static_cast<uint8_t>(rhs));
}

constexpr umb_t rwx_cast(Riscv64SV39RWX rwx) {
    switch (rwx) {
        case Riscv64SV39RWX::P:   return 0b000;
        case Riscv64SV39RWX::R:   return 0b001;
        case Riscv64SV39RWX::W:   return 0b010;
        case Riscv64SV39RWX::RW:  return 0b011;
        case Riscv64SV39RWX::X:   return 0b100;
        case Riscv64SV39RWX::RX:  return 0b101;
        case Riscv64SV39RWX::RWX: return 0b111;
        default:                  return 0b000;  // 非法组合，设置为P
    }
}

constexpr bool operator==(Riscv64SV39RWX lhs, umb_t rhs) {
    return rwx_cast(lhs) == rhs;
}

constexpr bool operator==(umb_t lhs, Riscv64SV39RWX rhs) {
    return rwx_cast(rhs) == lhs;
}

enum class Riscv64SV39ModifyMask : umb_t {
    // 基本修改掩码
    NONE = 0b000000,
    R    = 0b000001,
    W    = 0b000010,
    X    = 0b000100,
    U    = 0b001000,
    G    = 0b010000,
    NP   = 0b100000,
    // 常用组合
    RWX  = R | W | X,
    ALL  = R | W | X | U | G | NP,
};

constexpr bool operator&(Riscv64SV39ModifyMask lhs, Riscv64SV39ModifyMask rhs) {
    return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}

constexpr Riscv64SV39ModifyMask operator|(Riscv64SV39ModifyMask lhs,
                                          Riscv64SV39ModifyMask rhs) {
    return static_cast<Riscv64SV39ModifyMask>(static_cast<uint8_t>(lhs) |
                                              static_cast<uint8_t>(rhs));
}

template <KernelStage Stage>
class Riscv64SV39PageMan {
public:
    using RWX       = Riscv64SV39RWX;
    using StageAddr = _StageAddr<Stage>;
    using StageGFP = _GFP<Stage>;

    // 前初始化与后初始化
    static void init(void);

    // PhyAddr -> StageAddr
    static StageAddr _convert(PhyAddr paddr) {
        return convert<StageAddr>(paddr);
    }

    template <typename T>
    static T *_as(PhyAddr paddr) {
        return _convert(paddr).template as<T>();
    }

    // 构造RWX
    static constexpr RWX rwx(bool r, bool w, bool x) {
        if (r && w && x)
            return RWX::RWX;
        else if (r && w)
            return RWX::RW;
        else if (r && x)
            return RWX::RX;
        else if (r)
            return RWX::RO;
        else
            return RWX::P;
    }

    // RWX信息萃取
    static constexpr bool is_readable(RWX rwx) {
        return rwx & Riscv64SV39RWX::R;
    }

    static constexpr bool is_writable(RWX rwx) {
        return rwx & Riscv64SV39RWX::W;
    }

    static constexpr bool is_executable(RWX rwx) {
        return rwx & Riscv64SV39RWX::X;
    }

    enum class PageSize { _NULL, _4K, _2M, _1G };

    // 获得页大小
    static constexpr size_t psize(PageSize size) {
        switch (size) {
            case PageSize::_4K: return 0x1000;
            case PageSize::_2M: return 0x200000;
            case PageSize::_1G: return 0x40000000;
            default:            return 0;
        }
    }

    static constexpr int level(PageSize size) {
        switch (size) {
            case PageSize::_1G: return 1;
            case PageSize::_2M: return 2;
            case PageSize::_4K: return 3;
            default:            return 0;
        }
    }

    // 页表项结构
    union PTE {
        umb_t value;
        struct {
            bool v : 1;      // [0] Valid 有效位
            umb_t rwx : 3;   // [1:3] RWX位
            bool u : 1;      // [4] User 用户态可访问位
            bool g : 1;      // [5] Global 全局页位
            bool a : 1;      // [6] Accessed 访问位
            bool d : 1;      // [7] Dirty 脏位
            umb_t rsw : 2;   // [8:9] Reserved for Software 软件保留位
            umb_t ppn : 44;  // [10:53] Physical Page Number 物理页号
            umb_t rsvd : 7;  // [54:60] 保留位
            umb_t pbmt : 2;  // [61:62] Page-Based Memory Type 基于页的内存类型
            bool np : 1;  // [63] Not Present 非存在位
        } PACKED;
    };

    // 每个页表包含的PTE数量
    static constexpr size_t PTE_CNT = PAGESIZE / sizeof(PTE);
    static_assert(PAGESIZE == 4096);
    static_assert(sizeof(PTE) == 8);

    // 扩展页表项(加入页大小信息)
    struct ExtendedPTE {
        PTE *pte;
        PageSize size;
    };

    // PTE信息解析
    static constexpr RWX rwx(PTE pte) {
        switch (pte.rwx) {
            case 0b000: return RWX::P;
            case 0b001: return RWX::R;
            case 0b010: return RWX::NONE;  // W不能单独存在
            case 0b011: return RWX::RW;
            case 0b100: return RWX::X;
            case 0b101: return RWX::RX;
            case 0b110: return RWX::NONE;
            case 0b111: return RWX::RWX;
            default:    return RWX::NONE;
        }
    }

    static constexpr bool is_present(PTE pte) {
        return pte.v && !pte.np;
    }

    static constexpr bool is_user_accessible(PTE pte) {
        return pte.u;
    }

    static constexpr bool is_global(PTE pte) {
        return pte.g;
    }

    static constexpr bool is_valid(PTE pte) {
        return pte.v;
    }

    static inline PhyAddr from_ppn(umb_t ppn) {
        return PhyAddr(ppn << 12);
    }

    static inline umb_t to_ppn(PhyAddr addr) {
        return addr.arith() >> 12;
    }

    static inline PhyAddr get_physical_address(PTE pte) {
        return from_ppn(pte.ppn);
    }

    static constexpr bool is_dirty(PTE pte) {
        return pte.d;
    }

    // 修改掩码
    using ModifyMask = Riscv64SV39ModifyMask;

    static constexpr ModifyMask make_mask(bool r, bool w, bool x, bool u,
                                          bool g, bool np) {
        ModifyMask mask = ModifyMask::NONE;
        if (r)
            mask = mask | ModifyMask::R;
        if (w)
            mask = mask | ModifyMask::W;
        if (x)
            mask = mask | ModifyMask::X;
        if (u)
            mask = mask | ModifyMask::U;
        if (g)
            mask = mask | ModifyMask::G;
        if (np)
            mask = mask | ModifyMask::NP;
        return mask;
    }

    // 页表管理操作
    static PTE *read_root(void) {
        csr_satp_t satp = csr_get_satp();
        if (satp.mode != SATPMode::SV39) {
            return nullptr;
        }
        umb_t root_ppn = satp.ppn;
        return _as<PTE>(from_ppn(root_ppn));
    }
    static PhyAddr make_root(void) {
        PhyAddr __root = StageGFP::get_free_page();
        memset(_convert(__root).addr(), 0, PAGESIZE);
        return __root;
    }

private:
    PhyAddr __root;
    inline PTE *root() {
        return _as<PTE>(__root);
    }

public:
    explicit constexpr Riscv64SV39PageMan(PhyAddr root) : __root(root) {}
    Riscv64SV39PageMan() : Riscv64SV39PageMan(make_root()) {}

    template <PageSize size>
    inline static void make_vpn(VirAddr vaddr, umb_t vpn[level(size)]) {
        // check rwx != RWX_MODE_P
        constexpr int tot_levels = level(size);  // SV39有3级页表

        // 将vaddr拆分为三级索引
        umb_t va = vaddr.arith();

        vpn[0] = (va >> 30) & 0x1FF;  // VPN[0]: bits 30-38
        if constexpr (tot_levels >= 2) {
            static_assert(size == PageSize::_4K || size == PageSize::_2M);
            vpn[1] = (va >> 21) & 0x1FF;  // VPN[1]: bits 21-29
        }
        if constexpr (tot_levels == 3) {
            static_assert(size == PageSize::_4K);
            vpn[2] = (va >> 12) & 0x1FF;  // VPN[2]: bits 12-20
        }

        static_assert(tot_levels - 1 >= 0);
    }

    // 查询页
    ExtendedPTE query_page(VirAddr vaddr) {
        // 将vaddr拆分为三级索引
        umb_t vpn[3];
        make_vpn<PageSize::_4K>(vaddr, vpn);
        constexpr size_t total_levels = level(PageSize::_4K);

        // TODO: check _root not null
        PTE *pt = root();
        for (size_t level = 0; level < total_levels; level++) {
            // 查询当前级页表项
            PTE &pte = pt[vpn[level]];
            if (!pte.v) {
                return {nullptr, PageSize::_NULL};
            }

            // 不为P且有效，说明是叶子页表项
            if (pte.rwx != RWX::P) {
                // 大页映射
                PageSize size = PageSize::_4K;
                if (level == 0) {
                    size = PageSize::_1G;
                } else if (level == 1) {
                    size = PageSize::_2M;
                }
                return {&pte, size};
            }

            // 否则, 取下一级页表
            pt = _as<PTE>(from_ppn(pte.ppn));
        }

        // 到达此处, 当且仅当最后一级页表项的RWX也为RWX::P
        // 因此返回一个无效的页表项
        return {nullptr, PageSize::_NULL};  // 不应该到达这里
    }

    template <PageSize size>
    void map_page(VirAddr vaddr, PhyAddr paddr, RWX rwx, bool u, bool g) {
        // 当size为_NULL时, 无法映射任何页, 因此直接返回
        static_assert(size != PageSize::_NULL, "不能映射大小为0的页");

        // 得到该等级页的页表层数
        constexpr int tot_levels = level(size);
        static_assert(tot_levels - 1 >= 0);

        // 拆分vaddr
        umb_t vpn[tot_levels];
        make_vpn<size>(vaddr, vpn);

        // 进行遍历
        PTE *pt         = root();
        PTE *target_pte = nullptr;
        for (size_t level = 0; level < tot_levels; level++) {
            PTE &pte = pt[vpn[level]];

            // 已有映射, 无法继续
            if (pte.rwx != RWX::P) {
                PAGING::ERROR(
                    "VPN[%d] = %d 处已有映射! pte->rwx = %d, pte = %p, &pte = "
                    "%p",
                    level, vpn[level], pte.rwx, pte.value, &pte);
                break;
            }

            // 有效但不存在
            if (pte.v && pte.np) {
                PAGING::ERROR("VPN[%d] = %d 处页表项有效但不存在!");
                break;
            }

            if (!pte.v) {
                // 最后一级页表项, 携带着pte信息退出循环
                if (level == tot_levels - 1) {
                    target_pte = &pte;
                    break;
                }
                // 分配下一级页表
                PhyAddr new_pt = StageGFP::get_free_page();
                assert(new_pt.nonnull());
                // 初始化该页
                memset(_convert(new_pt).addr(), 0, PAGESIZE);
                // 添加到当前页表中
                pte.ppn = to_ppn(new_pt);
                pte.v   = true;
                pte.rwx = rwx_cast(RWX::P);
                pte.u   = u;
                pte.g   = g;
                PAGING::DEBUG(
                    "VPN[%d] = %d 处页表项 %p 不存在, 构造为 pte = %p", level,
                    vpn[level], &pte, pte.value);
            }

            // 确保 pte->u/g 与 u/g 参数匹配
            if ((pte.u ^ u) || (pte.g ^ g)) {
                PAGING::ERROR(
                    "VPN[%d] = %d 处页表项用户/全局属性不匹配! pte->u = %d, "
                    "pte->g = %d, 参数u = %d, 参数g = %d",
                    level, vpn[level], pte.u, pte.g, u, g);
                break;
            }

            pt = _as<PTE>(from_ppn(pte.ppn));
        }

        if (target_pte == nullptr) {
            PAGING::ERROR("未找到目标页表项!");
            return;
        }

        // 设置目标页表项
        target_pte->ppn = to_ppn(paddr);
        target_pte->v   = true;
        target_pte->rwx = rwx_cast(rwx);
        target_pte->u   = u;
        target_pte->g   = g;
        PAGING::DEBUG(
            "成功映射 vaddr = %p 到 paddr = %p, rwx = %d, u = %d, g = %d",
            vaddr.addr(), paddr.addr(), rwx_cast(rwx), u, g);
    }

    template <bool use_hugepage>
    void map_range(const VirAddr vstart, const PhyAddr pstart, size_t range_sz,
                   RWX rwx, bool u, bool g) {
        if constexpr (!use_hugepage) {
            // 将 vaddr 与 paddr 向下对齐到4K
            const VirAddr _vs  = vstart.page_align_down();
            const PhyAddr _ps  = pstart.page_align_down();
            // 将 size 向上对齐到4K
            const size_t _size = page_align_up(range_sz);
            const size_t _cnt  = _size / PAGESIZE;

            // 逐个小页映射
            for (size_t i = 0; i < _cnt; i++) {
                VirAddr vaddr = _vs + i * PAGESIZE;
                PhyAddr paddr = _ps + i * PAGESIZE;
                map_page<PageSize::_4K>(vaddr, paddr, rwx, u, g);
            }
        } else {
            // 将 vaddr 与 paddr 向下对齐到4K
            const VirAddr _vs  = vstart.page_align_down();
            const PhyAddr _ps  = pstart.page_align_down();
            // 将 size 向上对齐到4K
            const size_t _size = page_align_up(range_sz);
            const size_t _cnt  = _size / PAGESIZE;

            // 尝试使用大页映射
            constexpr size_t sz1g  = psize(PageSize::_1G);
            constexpr size_t sz2m  = psize(PageSize::_2M);
            constexpr size_t cnt1g = sz1g / PAGESIZE;
            constexpr size_t cnt2m = sz2m / PAGESIZE;

            size_t remcnt = _cnt;
            VirAddr _va   = _vs;
            PhyAddr _pa   = _ps;

            // 寻找合适的大页映射
            // 优先使用1G大页，其次2M大页，最后4K小页
            // 且使用时保证地址对齐
            while (remcnt > 0) {
                if ((remcnt >= cnt1g) && _va.aligned<sz1g>() &&
                    _pa.aligned<sz1g>())
                {
                    // 使用1G大页映射
                    map_page<PageSize::_1G>(_va, _pa, rwx, u, g);
                    _va    += sz1g;
                    _pa    += sz1g;
                    remcnt -= cnt1g;
                } else if ((remcnt >= cnt2m) && _va.aligned<sz2m>() &&
                           _pa.aligned<sz2m>())
                {
                    // 使用2M大页映射
                    map_page<PageSize::_2M>(_va, _pa, rwx, u, g);
                    _va    += sz2m;
                    _pa    += sz2m;
                    remcnt -= cnt2m;
                } else {
                    // 使用4K小页映射
                    map_page<PageSize::_4K>(_va, _pa, rwx, u, g);
                    _va    += PAGESIZE;
                    _pa    += PAGESIZE;
                    remcnt -= 1;
                }
            }
        }
    }

    void unmap_page(VirAddr vaddr) {
        // TODO: implement unmap_page
        PAGING::ERROR("unmap_page尚未实现");
    }

    void unmap_range(VirAddr vstart, size_t size) {
        // TODO: implement unmap_range
        PAGING::ERROR("unmap_range尚未实现");
    }

    static constexpr umb_t to_rwx_mask(ModifyMask mask) {
        umb_t rwx_m = 0b000;
        if (mask & ModifyMask::R)
            rwx_m |= 0b001;
        if (mask & ModifyMask::W)
            rwx_m |= 0b010;
        if (mask & ModifyMask::X)
            rwx_m |= 0b100;
        return rwx_m;
    }

    template <ModifyMask mask>
    PageSize __modify_flags(VirAddr vaddr, RWX rwx, bool u, bool g) {
        ExtendedPTE ext_pte = query_page(vaddr);
        if (ext_pte.pte == nullptr || ext_pte.size == PageSize::_NULL) {
            // 错误, 页面不存在
            return PageSize::_NULL;
        }

        PTE *pte = ext_pte.pte;

        constexpr umb_t rwx_mask = to_rwx_mask(mask);
        if constexpr (rwx_mask != 0b000) {
            umb_t rwx_bits = rwx_cast(rwx);
            umb_t old_bits = pte->rwx;
            // 将 old_bits, rwx_bits 按 rwx_mask 进行修改
            // 我们考虑按位计算, 那么对于单个位 o(old_bits), n(rwx_bits),
            // m(rwx_mask) 我们有 f(o, n, 0) = o, f(o, n, 1) = n 因此 f(o, n, m)
            // = (o & ~m) | (n & m)
            umb_t new_bits = (old_bits & ~rwx_mask) | (rwx_bits & rwx_mask);
            pte->rwx       = new_bits;
        }
        if constexpr (mask & ModifyMask::U) {
            pte->u = u;
        }
        if constexpr (mask & ModifyMask::G) {
            pte->g = g;
        }

        return ext_pte.size;
    }

    template <ModifyMask mask>
    void modify_flags(VirAddr vaddr, RWX rwx, bool u, bool g) {
        __modify_flags<mask>(vaddr, rwx, u, g);
    }

    template <ModifyMask mask>
    void modify_range_flags(VirAddr vstart, size_t size, RWX rwx, bool u,
                            bool g) {
        // 将 vaddr 向下对齐到4K
        const VirAddr _vs  = vstart.page_align_down();
        // 将 size 向上对齐到4K
        const size_t _size = page_align_up(size);
        const size_t _cnt  = _size / PAGESIZE;
        // 逐页修改, 直至完成
        // 中间可能存在大页
        size_t remcnt      = _cnt;
        VirAddr _va        = _vs;
        while (remcnt > 0) {
            PageSize size_type = __modify_flags<mask>(_va, rwx, u, g);
            if (size_type == PageSize::_NULL) {
                // 页面不存在, 无法修改
                return;
            }
            assert(psize(size_type) >= PAGESIZE);
            _va    += psize(size_type);
            remcnt -= psize(size_type) / PAGESIZE;
        }
    }

    // 更换页表根
    inline static void __switch_root(PhyAddr __root) {
        csr_satp_t new_satp;
        new_satp.mode = SATPMode::SV39;
        new_satp.asid = 0;  // TODO: ASID支持
        new_satp.ppn  = Riscv64SV39PageMan::to_ppn(__root);
        csr_set_satp(new_satp);
    }

    inline void switch_root() {
        __switch_root(__root);
    }

    // 获得页表根
    constexpr PhyAddr get_root() {
        return __root;
    }

    // 刷新TLB
    inline static void flush_tlb() {
        asm volatile("sfence.vma");
    }
};

static_assert(ArchPageManTrait<Riscv64SV39PageMan<KernelStage::PRE_INIT>>);
static_assert(ArchPageManTrait<Riscv64SV39PageMan<KernelStage::POST_INIT>>);