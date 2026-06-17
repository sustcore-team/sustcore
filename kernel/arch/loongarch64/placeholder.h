#pragma once

#include <arch/trait.h>
#include <sustcore/addr.h>

namespace la64 {
    template <typename T>
    constexpr umb_t rwx_cast(T rwx) {
        return static_cast<umb_t>(rwx);
    }

    class PageMan {
    public:
        enum class RWX : umb_t {
            P    = 0,
            R    = 1,
            W    = 2,
            X    = 4,
            RO   = R,
            RW   = R | W,
            RX   = R | X,
            RWX  = R | W | X,
            NONE = 0,
        };

        enum class Modifier : umb_t {
            NONE = 0,
            R    = 1,
            W    = 2,
            X    = 4,
            U    = 8,
            G    = 16,
            P    = 32,
            RWX  = R | W | X,
            ALL  = R | W | X | U | G | P,
        };
        enum class PageSize { _NULL, _4K };

        struct PageFlags {
            RWX rwx;
            bool u;
            bool g;
            bool p;
        };

        union PTE {
            umb_t value;
            umb_t rwx;
        };

        struct QueryResult {
            PTE *pte;
            PageSize size;
        };

        static constexpr RWX rwx(bool r, bool w, bool x) {
            return r ? (w ? (x ? RWX::RWX : RWX::RW) : (x ? RWX::RX : RWX::RO))
                     : RWX::P;
        }
        static constexpr bool is_readable(RWX rwx) {
            return rwx == RWX::RO || rwx == RWX::RW || rwx == RWX::RX ||
                   rwx == RWX::RWX;
        }
        static constexpr bool is_writable(RWX rwx) {
            return rwx == RWX::RW || rwx == RWX::RWX;
        }
        static constexpr bool is_executable(RWX rwx) {
            return rwx == RWX::RX || rwx == RWX::RWX;
        }
        static constexpr size_t psize(PageSize) {
            return PAGESIZE;
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
        static constexpr Modifier make_mask(bool r, bool w, bool x, bool u,
                                            bool g, bool p) {
            return static_cast<Modifier>((r ? 1 : 0) | (w ? 2 : 0) |
                                         (x ? 4 : 0) | (u ? 8 : 0) |
                                         (g ? 16 : 0) | (p ? 32 : 0));
        }
        static constexpr Modifier make_mask(b64 mask) {
            return static_cast<Modifier>(mask);
        }
        static constexpr RWX rwx(PTE) {
            return RWX::P;
        }
        static constexpr bool is_present(PTE) {
            return false;
        }
        static constexpr bool is_user_accessible(PTE) {
            return false;
        }
        static constexpr bool is_global(PTE) {
            return false;
        }
        static constexpr bool is_valid(PTE) {
            return false;
        }
        static constexpr PhyAddr get_physical_address(PTE) {
            return PhyAddr::null;
        }
        static constexpr bool is_dirty(PTE) {
            return false;
        }
        static constexpr bool is_cow(PTE) {
            return false;
        }
        static constexpr RWX without_write(RWX rwx) {
            return rwx;
        }
        static void set_cow(PTE *, bool);
        static void set_paddr(PTE *, PhyAddr);
        static PhyAddr read_root();
        static void init();
        static void make_root(PhyAddr root);
        static void __switch_root(PhyAddr root);
        static void flush_tlb();

        static constexpr size_t PTE_CNT = 512;

        explicit constexpr PageMan(PhyAddr root) : __root(root) {}

        [[nodiscard]]
        Result<QueryResult> query_page(VirAddr);

        template <PageSize size>
        void map_page(VirAddr, PhyAddr, PageFlags) {
            static_assert(size == PageSize::_4K);
        }

        void unmap_page(VirAddr) {}

        template <bool use_hugepage>
        void map_range(VirAddr, PhyAddr, size_t, PageFlags) {
            static_assert(!use_hugepage || use_hugepage);
        }

        void unmap_range(VirAddr, size_t) {}

        template <Modifier modifier>
        static void modify_pte(PTE *, PageFlags) {
            static_assert(modifier == Modifier::NONE ||
                          modifier != Modifier::NONE);
        }

        template <Modifier modifier>
        void modify_flags(VirAddr, PageFlags) {
            static_assert(modifier == Modifier::NONE ||
                          modifier != Modifier::NONE);
        }

        template <Modifier modifier>
        void modify_range_flags(VirAddr, size_t, PageFlags) {
            static_assert(modifier == Modifier::NONE ||
                          modifier != Modifier::NONE);
        }

        [[nodiscard]]
        Result<void> clone_mapping_from(PageMan &, VirAddr) noexcept;

        [[nodiscard]]
        Result<void> merge_from(PageMan &) noexcept;

        void switch_root() {
            __switch_root(__root);
        }

        constexpr PhyAddr get_root() {
            return __root;
        }

    private:
        PhyAddr __root;
    };

    static_assert(ArchPageManTrait<PageMan>);
}  // namespace la64
