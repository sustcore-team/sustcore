#pragma once

#include <arch/trait.h>
#include <mem/page_types.h>
#include <sustcore/addr.h>

namespace la64 {
    template <typename T>
    constexpr umb_t rwx_cast(T rwx) {
        return static_cast<umb_t>(rwx);
    }

    class PageMan {
    public:
        using RWX      = PageRWX;
        using Modifier = PageModifier;
        enum class PageSize { _NULL, _4K };
        using PageFlags = ::PageFlags;

        union PTE {
            umb_t value;
            umb_t rwx;
        };

        struct QueryResult {
            PTE *pte;
            PageSize size;
        };

        static constexpr RWX rwx(bool r, bool w, bool x) {
            return page_rwx(r, w, x);
        }
        static constexpr bool is_readable(RWX rwx) {
            return page_is_readable(rwx);
        }
        static constexpr bool is_writable(RWX rwx) {
            return page_is_writable(rwx);
        }
        static constexpr bool is_executable(RWX rwx) {
            return page_is_executable(rwx);
        }
        static constexpr size_t psize(PageSize) {
            return PAGESIZE;
        }
        static constexpr PageFlags page_flags(RWX rwx, bool u, bool g,
                                              bool p = true) {
            return make_page_flags(rwx, u, g, p);
        }
        static constexpr Modifier make_mask(bool r, bool w, bool x, bool u,
                                            bool g, bool p) {
            return make_page_modifier(r, w, x, u, g, p);
        }
        static constexpr Modifier make_mask(b64 mask) {
            return make_page_modifier(mask);
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
            return ::without_write(rwx);
        }
        static void set_cow(PTE *, bool);
        static void protect_cow(PTE *, RWX) {}
        static void restore_from_cow(PTE *, PageFlags) {}
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
