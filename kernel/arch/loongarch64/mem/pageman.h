/**
 * @file pageman.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 页表管理实现
 * @version alpha-1.0.0
 * @date 2026-06-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/trait.h>
#include <arch/loongarch64/mem/paging.h>
#include <logger.h>
#include <mem/gfp.h>
#include <sus/logger.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

#include <cassert>
#include <cstring>

namespace la64 {
    enum class LA64RWX : umb_t {
        P    = 0b000,
        R    = 0b001,
        W    = 0b010,
        X    = 0b100,
        RO   = R,
        RW   = R | W,
        RX   = R | X,
        RWX  = R | W | X,
        NONE = 0b000,
    };

    constexpr bool operator&(LA64RWX lhs, LA64RWX rhs) {
        return (static_cast<umb_t>(lhs) & static_cast<umb_t>(rhs)) != 0;
    }

    constexpr LA64RWX operator|(LA64RWX lhs, LA64RWX rhs) {
        return static_cast<LA64RWX>(static_cast<umb_t>(lhs) |
                                    static_cast<umb_t>(rhs));
    }

    constexpr umb_t rwx_cast(LA64RWX rwx) {
        return static_cast<umb_t>(rwx);
    }

    enum class LA64Modifier : umb_t {
        NONE = 0b000000,
        R    = 0b000001,
        W    = 0b000010,
        X    = 0b000100,
        U    = 0b001000,
        G    = 0b010000,
        P    = 0b100000,
        RWX  = R | W | X,
        ALL  = R | W | X | U | G | P,
    };

    constexpr bool operator&(LA64Modifier lhs, LA64Modifier rhs) {
        return (static_cast<umb_t>(lhs) & static_cast<umb_t>(rhs)) != 0;
    }

    constexpr LA64Modifier operator|(LA64Modifier lhs, LA64Modifier rhs) {
        return static_cast<LA64Modifier>(static_cast<umb_t>(lhs) |
                                         static_cast<umb_t>(rhs));
    }

    class PageMan {
    public:
        using RWX      = LA64RWX;
        using Modifier = LA64Modifier;

        struct PageFlags {
            RWX rwx;
            bool u;
            bool g;
            bool p;
        };

        enum class PageSize { _NULL, _4K, _2M, _1G };

        union PTE {
            umb_t value;
        };

        struct QueryResult {
            PTE *pte;
            PageSize size;
        };

        static constexpr size_t PAGE_LEVELS     = 4;
        static constexpr size_t PAGE_BITS       = 12;
        static constexpr size_t PAGE_INDEX_BITS = 9;
        static constexpr umb_t PAGE_ADDR_MASK   = LA_PPN_MASK;
        static constexpr umb_t PAGE_VALID       = LA_PAGE_VALID;
        static constexpr umb_t PAGE_DIRTY       = LA_PAGE_DIRTY;
        static constexpr umb_t PAGE_CACHE_CC    = LA_PAGE_CACHE_CC;
        static constexpr umb_t PAGE_GLOBAL      = LA_PAGE_GLOBAL;
        static constexpr umb_t PAGE_PRESENT     = LA_PAGE_PRESENT;
        static constexpr umb_t PAGE_WRITE       = LA_PAGE_WRITE;
        static constexpr umb_t PAGE_MODIFIED    = LA_PAGE_MODIFIED;
        static constexpr umb_t PAGE_USER        = 0xCU;
        static constexpr umb_t PAGE_NO_READ     = 1ULL << 61;
        static constexpr umb_t PAGE_NO_EXEC     = 1ULL << 62;
        static constexpr umb_t PAGE_COW         = 1ULL << 10;

        static constexpr size_t PTE_CNT = PAGESIZE / sizeof(PTE);

        static_assert(PAGESIZE == 4096);
        static_assert(sizeof(PTE) == 8);

        static Result<PhyAddr> new_page(void) {
            return GFP::get_free_page(1);
        }

        static void init(void);

        static VirAddr _convert(PhyAddr paddr);

        template <typename T>
        static T *_as(PhyAddr paddr) {
            return _convert(paddr).template as<T>();
        }

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

        static constexpr bool is_readable(RWX rwx) {
            return rwx & RWX::R;
        }

        static constexpr bool is_writable(RWX rwx) {
            return rwx & RWX::W;
        }

        static constexpr bool is_executable(RWX rwx) {
            return rwx & RWX::X;
        }

        static constexpr PageFlags page_flags(RWX rwx, bool u, bool g,
                                              bool p = true) {
            return PageFlags{
                .rwx = rwx,
                .u   = u,
                .g   = g,
                .p   = p,
            };
        }

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
                case PageSize::_1G: return 2;
                case PageSize::_2M: return 3;
                case PageSize::_4K: return 4;
                default:            return 0;
            }
        }

        static constexpr Modifier make_mask(bool r, bool w, bool x, bool u,
                                            bool g, bool p) {
            Modifier mask = Modifier::NONE;
            if (r)
                mask = mask | Modifier::R;
            if (w)
                mask = mask | Modifier::W;
            if (x)
                mask = mask | Modifier::X;
            if (u)
                mask = mask | Modifier::U;
            if (g)
                mask = mask | Modifier::G;
            if (p)
                mask = mask | Modifier::P;
            return mask;
        }

        static constexpr Modifier make_mask(b64 mask) {
            return make_mask(mask & 0b000001, mask & 0b000010, mask & 0b000100,
                             mask & 0b001000, mask & 0b010000, mask & 0b100000);
        }

        static constexpr bool pte_is_table(PTE pte) {
            return (pte.value & PAGE_ADDR_MASK) != 0 &&
                   (pte.value & PAGE_PRESENT) == 0;
        }

        static constexpr bool pte_is_large(PTE pte) {
            return (pte.value & (PAGE_PRESENT | PAGE_GLOBAL)) ==
                   (PAGE_PRESENT | PAGE_GLOBAL);
        }

        static constexpr bool pte_exists(PTE pte) {
            return pte.value != 0;
        }

        static constexpr RWX rwx(PTE pte) {
            if ((pte.value & PAGE_PRESENT) == 0) {
                return RWX::P;
            }
            bool r = (pte.value & PAGE_NO_READ) == 0;
            bool w = (pte.value & PAGE_WRITE) != 0;
            bool x = (pte.value & PAGE_NO_EXEC) == 0;
            return rwx(r, w, x);
        }

        static constexpr bool is_present(PTE pte) {
            return (pte.value & (PAGE_VALID | PAGE_PRESENT)) ==
                   (PAGE_VALID | PAGE_PRESENT);
        }

        static constexpr bool is_user_accessible(PTE pte) {
            return (pte.value & PAGE_USER) == PAGE_USER;
        }

        static constexpr bool is_global(PTE pte) {
            return (pte.value & PAGE_GLOBAL) != 0;
        }

        static constexpr bool is_valid(PTE pte) {
            return (pte.value & PAGE_VALID) != 0;
        }

        static inline PhyAddr get_physical_address(PTE pte) {
            return PhyAddr(pte.value & PAGE_ADDR_MASK);
        }

        static constexpr bool is_dirty(PTE pte) {
            return (pte.value & PAGE_DIRTY) != 0;
        }

        static constexpr bool is_cow(PTE pte) {
            return (pte.value & PAGE_COW) != 0;
        }

        static constexpr RWX without_write(RWX rwx) {
            switch (rwx) {
                case RWX::RW:  return RWX::RO;
                case RWX::RWX: return RWX::RX;
                default:       return rwx;
            }
        }

        static void set_cow(PTE *pte, bool cow);
        static void set_paddr(PTE *pte, PhyAddr paddr);
        static PhyAddr read_root();
        static void make_root(PhyAddr root);
        static void __switch_root(PhyAddr root);
        static void flush_tlb();

    private:
        PhyAddr __root;

        inline PTE *root() {
            return _as<PTE>(__root);
        }

        static constexpr umb_t level_shift(size_t level_index) {
            return PAGE_BITS +
                   PAGE_INDEX_BITS * (PAGE_LEVELS - 1 - level_index);
        }

        static constexpr umb_t index_mask() {
            return (1UL << PAGE_INDEX_BITS) - 1;
        }

        static constexpr umb_t base_leaf_flags() {
            return PAGE_VALID | PAGE_PRESENT | PAGE_CACHE_CC | PAGE_MODIFIED;
        }

        static constexpr umb_t flags_to_leaf_bits(PageFlags flags) {
            umb_t bits = base_leaf_flags();
            if (!is_readable(flags.rwx)) {
                bits |= PAGE_NO_READ;
            }
            if (is_writable(flags.rwx)) {
                bits |= PAGE_WRITE | PAGE_DIRTY;
            }
            if (!is_executable(flags.rwx)) {
                bits |= PAGE_NO_EXEC;
            }
            if (flags.u) {
                bits |= PAGE_USER;
            } else {
                bits |= PAGE_GLOBAL;
            }
            if (flags.g) {
                bits |= PAGE_GLOBAL;
            }
            if (!flags.p) {
                bits &= ~PAGE_PRESENT;
            }
            return bits;
        }

        static constexpr umb_t to_rwx_mask(Modifier modifier) {
            umb_t rwx_mask = 0;
            if (modifier & Modifier::R)
                rwx_mask |= 0b001;
            if (modifier & Modifier::W)
                rwx_mask |= 0b010;
            if (modifier & Modifier::X)
                rwx_mask |= 0b100;
            return rwx_mask;
        }

        template <PageSize size>
        static inline void make_vpn(VirAddr vaddr, umb_t vpn[level(size)]) {
            constexpr int total_levels = level(size);
            umb_t va                   = vaddr.arith();
            for (int i = 0; i < total_levels; ++i) {
                vpn[i] = (va >> level_shift(i)) & index_mask();
            }
        }

        template <Modifier modifier>
        PageSize __modify_flags(VirAddr vaddr, PageFlags flags) {
            auto query_res = query_page(vaddr);
            if (!query_res.has_value()) {
                return PageSize::_NULL;
            }
            modify_pte<modifier>(query_res.value().pte, flags);
            return query_res.value().size;
        }

    public:
        explicit constexpr PageMan(PhyAddr root) : __root(root) {}

        [[nodiscard]]
        Result<QueryResult> query_page(VirAddr vaddr);

        template <PageSize size>
        void map_page(VirAddr vaddr, PhyAddr paddr, PageFlags flags) {
            static_assert(size != PageSize::_NULL, "不能映射大小为0的页");

            constexpr int total_levels = level(size);
            umb_t vpn[total_levels];
            make_vpn<size>(vaddr, vpn);

            PTE *pt         = root();
            PTE *target_pte = nullptr;
            for (int level_index = 0; level_index < total_levels; ++level_index)
            {
                PTE &pte = pt[vpn[level_index]];

                if (level_index == total_levels - 1) {
                    if (pte_exists(pte)) {
                        loggers::PAGING::ERROR(
                            "LoongArch64 目标叶子页表项已存在");
                        return;
                    }
                    target_pte = &pte;
                    break;
                }

                if (!pte_exists(pte)) {
                    auto new_page_res = new_page();
                    if (!new_page_res.has_value()) {
                        loggers::PAGING::ERROR("LoongArch64 无法分配下级页表");
                        return;
                    }
                    PhyAddr new_pt = new_page_res.value();
                    make_root(new_pt);
                    pte.value = new_pt.arith() & PAGE_ADDR_MASK;
                } else if (!pte_is_table(pte)) {
                    loggers::PAGING::ERROR(
                        "LoongArch64 页表路径被叶子映射占用");
                    return;
                }

                pt = _as<PTE>(get_physical_address(pte));
            }

            if (target_pte == nullptr) {
                loggers::PAGING::ERROR("LoongArch64 未找到目标页表项");
                return;
            }

            target_pte->value = (paddr.arith() & PAGE_ADDR_MASK);
            modify_pte<Modifier::ALL>(target_pte, flags);
            loggers::PAGING::DEBUG(
                "LoongArch64 映射完成: va=%p pa=%p rwx=%lu u=%d g=%d p=%d",
                vaddr.addr(), paddr.addr(),
                static_cast<unsigned long>(rwx_cast(flags.rwx)), flags.u,
                flags.g, flags.p);
        }

        void unmap_page(VirAddr vaddr) {
            auto query_res = query_page(vaddr);
            if (!query_res.has_value()) {
                return;
            }
            query_res.value().pte->value = 0;
        }

        template <bool use_hugepage>
        void map_range(VirAddr vstart, PhyAddr pstart, size_t range_sz,
                       PageFlags flags) {
            VirAddr aligned_vstart = vstart.page_align_down();
            PhyAddr aligned_pstart = pstart.page_align_down();
            size_t aligned_size    = page_align_up(range_sz);
            if constexpr (!use_hugepage) {
                size_t page_count = aligned_size / PAGESIZE;
                for (size_t i = 0; i < page_count; ++i) {
                    map_page<PageSize::_4K>(aligned_vstart + i * PAGESIZE,
                                            aligned_pstart + i * PAGESIZE,
                                            flags);
                }
            } else {
                constexpr size_t size_1g = 0x40000000ULL;
                constexpr size_t size_2m = 0x200000ULL;
                size_t remaining         = aligned_size;
                VirAddr current_vaddr    = aligned_vstart;
                PhyAddr current_paddr    = aligned_pstart;

                while (remaining > 0) {
                    if (remaining >= size_1g &&
                        current_vaddr.aligned<size_1g>() &&
                        current_paddr.aligned<size_1g>())
                    {
                        map_page<PageSize::_1G>(current_vaddr, current_paddr,
                                                flags);
                        current_vaddr += size_1g;
                        current_paddr += size_1g;
                        remaining -= size_1g;
                    } else if (remaining >= size_2m &&
                               current_vaddr.aligned<size_2m>() &&
                               current_paddr.aligned<size_2m>())
                    {
                        map_page<PageSize::_2M>(current_vaddr, current_paddr,
                                                flags);
                        current_vaddr += size_2m;
                        current_paddr += size_2m;
                        remaining -= size_2m;
                    } else {
                        map_page<PageSize::_4K>(current_vaddr, current_paddr,
                                                flags);
                        current_vaddr += PAGESIZE;
                        current_paddr += PAGESIZE;
                        remaining -= PAGESIZE;
                    }
                }
            }
        }

        void unmap_range(VirAddr vstart, size_t size) {
            VirAddr aligned_vstart = vstart.page_align_down();
            size_t aligned_size    = page_align_up(size);
            size_t page_count      = aligned_size / PAGESIZE;
            for (size_t i = 0; i < page_count; ++i) {
                unmap_page(aligned_vstart + i * PAGESIZE);
            }
        }

        template <Modifier modifier>
        static void modify_pte(PTE *pte, PageFlags flags) {
            if (pte == nullptr) {
                return;
            }

            constexpr umb_t rwx_mask = to_rwx_mask(modifier);
            if constexpr (rwx_mask != 0) {
                RWX current_rwx = rwx(*pte);
                bool r          = is_readable(current_rwx);
                bool w          = is_writable(current_rwx);
                bool x          = is_executable(current_rwx);
                if constexpr (modifier & Modifier::R) {
                    r = is_readable(flags.rwx);
                }
                if constexpr (modifier & Modifier::W) {
                    w = is_writable(flags.rwx);
                }
                if constexpr (modifier & Modifier::X) {
                    x = is_executable(flags.rwx);
                }
                pte->value &=
                    ~(PAGE_NO_READ | PAGE_WRITE | PAGE_DIRTY | PAGE_NO_EXEC);
                if (!r) {
                    pte->value |= PAGE_NO_READ;
                }
                if (w) {
                    pte->value |= PAGE_WRITE | PAGE_DIRTY;
                }
                if (!x) {
                    pte->value |= PAGE_NO_EXEC;
                }
            }
            if constexpr (modifier & Modifier::U) {
                if (flags.u) {
                    pte->value |= PAGE_USER;
                } else {
                    pte->value &= ~PAGE_USER;
                }
            }
            if constexpr (modifier & Modifier::G) {
                if (flags.g) {
                    pte->value |= PAGE_GLOBAL;
                } else {
                    pte->value &= ~PAGE_GLOBAL;
                }
            }
            if constexpr (modifier & Modifier::P) {
                if (flags.p) {
                    pte->value |= PAGE_PRESENT;
                } else {
                    pte->value &= ~PAGE_PRESENT;
                }
            }
            pte->value |= PAGE_VALID | PAGE_CACHE_CC | PAGE_MODIFIED;
        }

        template <Modifier modifier>
        void modify_flags(VirAddr vaddr, PageFlags flags) {
            __modify_flags<modifier>(vaddr, flags);
        }

        template <Modifier modifier>
        void modify_range_flags(VirAddr vstart, size_t size, PageFlags flags) {
            VirAddr aligned_vstart = vstart.page_align_down();
            size_t aligned_size    = page_align_up(size);
            size_t page_count      = aligned_size / PAGESIZE;
            for (size_t i = 0; i < page_count; ++i) {
                if (__modify_flags<modifier>(aligned_vstart + i * PAGESIZE,
                                             flags) == PageSize::_NULL)
                {
                    return;
                }
            }
        }

        [[nodiscard]]
        Result<void> clone_mapping_from(PageMan &src, VirAddr vaddr) noexcept;

        [[nodiscard]]
        Result<void> merge_from(PageMan &src) noexcept;

        inline void switch_root() {
            __switch_root(__root);
        }

        constexpr PhyAddr get_root() {
            return __root;
        }
    };

    static_assert(ArchPageManTrait<PageMan>);
}  // namespace la64
