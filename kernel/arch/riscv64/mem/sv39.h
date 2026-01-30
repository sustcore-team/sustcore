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
#include <mem/kaddr.h>
#include <mem/gfp.h>
#include <sus/types.h>
#include <kio.h>
#include <cstring>
#include <basecpp/logger.h>

enum class Riscv64SV39RWX : umb_t {
    // 基本权限
    P   = 0b000,
    R   = 0b001,
    W   = 0b010,
    X   = 0b100,
    // 组合权限
    RO  = R,
    RW  = R | W,
    RX  = R | X,
    RWX = R | W | X,
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

template <PageFrameAllocatorTrait GFP>
class Riscv64SV39PageMan {
public:
    // 前初始化与后初始化
    static void pre_init(void){}
    static void post_init(void){}

    using RWX = Riscv64SV39RWX;

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

    enum class PageSize { SIZE_NULL, SIZE_4K, SIZE_2M, SIZE_1G };

    // 获得页大小
    static constexpr size_t get_size(PageSize size) {
        switch (size) {
            case PageSize::SIZE_4K: return 0x1000;
            case PageSize::SIZE_2M: return 0x200000;
            case PageSize::SIZE_1G: return 0x40000000;
            default:                return 0;
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
        switch (pte->rwx) {
        case 0b000: return RWX::P;
        case 0b001: return RWX::R;
        case 0b110: return RWX::NONE;
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

    static void *from_ppn(umb_t ppn) {
        umb_t addr = ppn << 12;
        return (void *)(addr);
    }

    static umb_t to_ppn(void *addr) {
        umb_t addr_val = (umb_t)(addr);
        return addr_val >> 12;
    }

    static void *get_physical_address(PTE pte) {
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
        return (PTE *)PA2KPA(from_ppn(root_ppn));
    }

    static PTE *make_root(void) {
        PTE *root = (PTE *)PA2KPA(GFP::alloc_frame());
        memset(root, 0, PAGESIZE);
        return root;
    }

private:
    PTE *_root;
    PTE *root() { return (PTE *)PA2KPA(_root); }
public:
    Riscv64SV39PageMan() : _root(make_root()) {}
    explicit Riscv64SV39PageMan(PTE *__root) : _root(__root) {}

    // 查询页
    ExtendedPTE query_page(void *vaddr) {
        // 将vaddr拆分为三级索引
        umb_t vpn[3];
        umb_t va = (umb_t)vaddr;
        vpn[0]   = (va >> 12) & 0x1FF;  // VPN[0]: bits 12-20
        vpn[1]   = (va >> 21) & 0x1FF;  // VPN[1]: bits 21-29
        vpn[2]   = (va >> 30) & 0x1FF;  // VPN[2]: bits 30-38

        // TODO: check _root not null
        PTE *pte = &(root()[vpn[2]]);
        for (int level = 2; level > 0; level--) {
            if (!pte->v) {
                return {nullptr, PageSize::SIZE_NULL};
            }
            if (pte->rwx != RWX::P) {
                // 大页映射
                PageSize size =
                    (level == 2) ? PageSize::SIZE_1G : PageSize::SIZE_2M;
                return {pte, size};
            }

            // 否则, 取下一级页表
            PTE *pt = (PTE *)PA2KPA(from_ppn(pte->ppn));
            // TODO: check pt not null
            pte     = &pt[vpn[level - 1]];
            // assert(level - 1 >= 0);
        }
        return {pte, PageSize::SIZE_4K};
    }

    static constexpr int size_to_level(PageSize size) {
        switch (size) {
            case PageSize::SIZE_1G: return 1;
            case PageSize::SIZE_2M: return 2;
            case PageSize::SIZE_4K: return 3;
            default:                return 0;
        }
    }

    template <PageSize size>
    void map_page(void *vaddr, void *paddr, RWX rwx, bool u, bool g) {
        // do nothing when size is SIZE_NULL
        // in fact, it should be an error
        static_assert(size != PageSize::SIZE_NULL, "Cannot map page with SIZE_NULL");
        // check rwx != RWX_MODE_P
        constexpr int tot_levels = size_to_level(size);  // SV39有3级页表

        // 将vaddr拆分为三级索引
        umb_t vpn[tot_levels];
        umb_t va = (umb_t)vaddr;

        vpn[0] = (va >> 30) & 0x1FF;  // VPN[0]: bits 30-38
        if constexpr (tot_levels >= 2) {
            static_assert(size == PageSize::SIZE_4K ||
                          size == PageSize::SIZE_2M);
            vpn[1] = (va >> 21) & 0x1FF;  // VPN[1]: bits 21-29
        }
        if constexpr (tot_levels == 3) {
            static_assert(size == PageSize::SIZE_4K);
            vpn[2] = (va >> 12) & 0x1FF;  // VPN[2]: bits 12-20
        }

        static_assert(tot_levels - 1 >= 0);
        PTE *pte = &(root()[vpn[0]]);
        for (int i = 1; i < tot_levels; i++) {
            if (!pte->v) {
                // 分配下一级页表
                PTE *new_pt = (PTE *)GFP::alloc_frame();
                // check new_pt not null
                memset((PTE *)PA2KPA(new_pt), 0, PAGESIZE);
                pte->ppn = to_ppn(KPA2PA(new_pt));
                pte->v   = true;
                pte->rwx = rwx_cast(RWX::P);  // 标记为非叶子节点
                pte->u   = u;
                pte->g   = g;
                PAGING::DEBUG("VPN[%d] = %d 处未存在, 构造为 pte->rwx = %d, pte = %p, &pte = %p", i - 1, vpn[i - 1], pte->rwx, pte->value, &pte);
            } else if (pte->rwx != RWX::P) {
                PAGING::DEBUG("VPN[%d] = %d 处已有大页映射! pte->rwx = %d, pte = %p, &pte = %p", i - 1, vpn[i - 1], pte->rwx, pte->value);
                return;
            } else if (pte->np) {
                // 非存在页，do sth...
                return;
            }

            // 检查pte属性
            // 如果pte->u/g与u/g不符，则无法继续
            if ((pte->u ^ u) || (pte->g ^ g)) {
                return;
            }

            PTE *pt = (PTE *)PA2KPA(from_ppn(pte->ppn));
            // check pt not null
            pte     = &pt[vpn[i]];
        }

        // 填充最终页表项
        // 首先清空
        pte->value = 0;
        // 设置各个位
        pte->v     = true;
        pte->rwx   = rwx_cast(rwx);
        pte->u     = u;
        pte->g     = g;
        pte->ppn   = to_ppn(paddr);
    }

    void unmap_page(void *vaddr) {
        // TODO: implement unmap_page
    }

    template <bool use_hugepage>
    void map_range(void *vstart, void *pstart, size_t size, RWX rwx, bool u,
                   bool g) {
        // 将 vaddr 与 paddr 向下对齐到4K
        const umb_t _vs    = page_align_down((umb_t)vstart);
        const umb_t _ps    = page_align_down((umb_t)pstart);
        // 将 size 向上对齐到4K
        const size_t _size = (size_t)page_align_up((umb_t)size);
        const size_t _cnt  = _size / PAGESIZE;

        if constexpr (!use_hugepage) {
            // 逐个小页映射
            for (size_t i = 0; i < _cnt; i++) {
                void *vaddr = (void *)(_vs + i * PAGESIZE);
                void *paddr = (void *)(_ps + i * PAGESIZE);
                map_page<PageSize::SIZE_4K>(vaddr, paddr, rwx, u, g);
            }
        } else {
            // 尝试使用大页映射
            constexpr size_t cnt_1g = get_size(PageSize::SIZE_1G) / PAGESIZE;
            constexpr size_t cnt_2m = get_size(PageSize::SIZE_2M) / PAGESIZE;

            size_t cnt_remaining = _cnt;
            umb_t _va            = _vs;
            umb_t _pa            = _ps;

            // 寻找合适的大页映射
            // 优先使用1G大页，其次2M大页，最后4K小页
            // 且使用时保证地址对齐
            while (cnt_remaining > 0) {
                if ((cnt_remaining >= cnt_1g) &&
                    ((_va % get_size(PageSize::SIZE_1G)) == 0) &&
                    ((_pa % get_size(PageSize::SIZE_1G)) == 0))
                {
                    // 使用1G大页映射
                    map_page<PageSize::SIZE_1G>((void *)_va, (void *)_pa, rwx,
                                                u, g);
                    _va           += get_size(PageSize::SIZE_1G);
                    _pa           += get_size(PageSize::SIZE_1G);
                    cnt_remaining -= cnt_1g;
                } else if ((cnt_remaining >= cnt_2m) &&
                           ((_va % get_size(PageSize::SIZE_2M)) == 0) &&
                           ((_pa % get_size(PageSize::SIZE_2M)) == 0))
                {
                    // 使用2M大页映射
                    map_page<PageSize::SIZE_2M>((void *)_va, (void *)_pa, rwx,
                                                u, g);
                    _va           += get_size(PageSize::SIZE_2M);
                    _pa           += get_size(PageSize::SIZE_2M);
                    cnt_remaining -= cnt_2m;
                } else {
                    // 使用4K小页映射
                    map_page<PageSize::SIZE_4K>((void *)_va, (void *)_pa, rwx,
                                                u, g);
                    _va           += PAGESIZE;
                    _pa           += PAGESIZE;
                    cnt_remaining -= 1;
                }
            }
        }
    }

    void unmap_range(void *vstart, size_t size) {
        // TODO: implement unmap_range
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
    PageSize __modify_flags(void *vaddr, RWX rwx, bool u, bool g) {
        ExtendedPTE ext_pte = query_page(vaddr);
        if (ext_pte.pte == nullptr || ext_pte.size == PageSize::SIZE_NULL) {
            // 错误, 页面不存在
            return PageSize::SIZE_NULL;
        }

        PTE *pte = ext_pte.pte;

        constexpr umb_t rwx_mask = to_rwx_mask(mask);
        if constexpr (rwx_mask != 0b000) {
            umb_t rwx_bits = rwx_cast(rwx);
            umb_t old_bits = rwx_cast(pte->rwx);
            // 将 old_bits, rwx_bits 按 rwx_mask 进行修改
            // 我们考虑按位计算, 那么对于单个位 o(old_bits), n(rwx_bits), m(rwx_mask)
            // 我们有 f(o, n, 0) = o, f(o, n, 1) = n
            // 因此 f(o, n, m) = (o & ~m) | (n & m)
            umb_t new_bits = (old_bits & ~rwx_mask) | (rwx_bits & rwx_mask);
            pte->rwx        = new_bits;
        }
        if constexpr (mask & ModifyMask::U) {
            pte->u = u;
        }
        if constexpr (mask & ModifyMask::G) {
            pte->g = g;
        }

        return ext_pte.size;
    }

    template<ModifyMask mask>
    void modify_flags(void *vaddr, RWX rwx, bool u, bool g) {
        __modify_flags<mask>(vaddr, rwx, u, g);
    }

    template<ModifyMask mask>
    void modify_range_flags(void *vstart, size_t size, RWX rwx, bool u, bool g) {
        // 将 vaddr 向下对齐到4K
        const umb_t _vs    = page_align_down((umb_t)vstart);
        // 将 size 向上对齐到4K
        const size_t _size = (size_t)page_align_up((umb_t)size);
        const size_t _cnt  = _size / PAGESIZE;
        // 逐页修改, 直至完成
        // 中间可能存在大页
        size_t cnt_remaining = _cnt;
        umb_t _va            = _vs;
        while (cnt_remaining > 0) {
            PageSize psize = __modify_flags<mask>((void *)_va, rwx, u, g);
            if (psize == PageSize::SIZE_NULL) {
                // 页面不存在, 无法修改
                return;
            }
            // assert(get_size(psize) >= PAGESIZE)
            _va += get_size(psize);
            cnt_remaining -= get_size(psize) / PAGESIZE;
        }
    }

    // 更换页表根
    inline static void __switch_root(PTE *__root) {
        csr_satp_t new_satp;
        new_satp.mode = SATPMode::SV39;
        new_satp.asid = 0;  // TODO: ASID支持
        new_satp.ppn  = Riscv64SV39PageMan::to_ppn(__root);
        csr_set_satp(new_satp);
    }

    void switch_root() {
        __switch_root(_root);
    }

    // 获得页表根
    PTE *&get_root() {
        return _root;
    }

    // 刷新TLB
    inline static void flush_tlb() {
        asm volatile("sfence.vma");
    }
};

static_assert(ArchPageManTrait<Riscv64SV39PageMan<LinearGrowGFP>>);