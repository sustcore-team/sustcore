#pragma once

#include <arch/trait.h>
#include <sustcore/addr.h>

enum class Loongarch64RWX : umb_t {
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

enum class Loongarch64ModifyMask : umb_t {
    NONE = 0,
    R    = 1,
    W    = 2,
    X    = 4,
    U    = 8,
    G    = 16,
    NP   = 32,
    RWX  = R | W | X,
    ALL  = R | W | X | U | G | NP,
};

class Loongarch64PageMan {
public:
    using RWX        = Loongarch64RWX;
    using ModifyMask = Loongarch64ModifyMask;

    enum class PageSize { _NULL, _4K };

    union PTE {
        umb_t value;
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
    static constexpr ModifyMask make_mask(bool r, bool w, bool x, bool u,
                                          bool g, bool np) {
        return static_cast<ModifyMask>((r ? 1 : 0) | (w ? 2 : 0) |
                                       (x ? 4 : 0) | (u ? 8 : 0) |
                                       (g ? 16 : 0) | (np ? 32 : 0));
    }
    static constexpr ModifyMask make_mask(b64 mask) {
        return static_cast<ModifyMask>(mask);
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

    explicit constexpr Loongarch64PageMan(PhyAddr root) : __root(root) {}

    [[nodiscard]]
    Result<QueryResult> query_page(VirAddr);

    template <PageSize size>
    void map_page(VirAddr, PhyAddr, RWX, bool, bool) {
        static_assert(size == PageSize::_4K);
    }

    void unmap_page(VirAddr) {}

    template <bool use_hugepage>
    void map_range(VirAddr, PhyAddr, size_t, RWX, bool, bool) {
        static_assert(!use_hugepage || use_hugepage);
    }

    void unmap_range(VirAddr, size_t) {}

    template <ModifyMask mask>
    void modify_flags(VirAddr, RWX, bool, bool) {
        static_assert(mask == ModifyMask::NONE || mask != ModifyMask::NONE);
    }

    template <ModifyMask mask>
    void modify_range_flags(VirAddr, size_t, RWX, bool, bool) {
        static_assert(mask == ModifyMask::NONE || mask != ModifyMask::NONE);
    }

    [[nodiscard]]
    Result<void> clone_mapping_from(Loongarch64PageMan &, VirAddr) noexcept;

    [[nodiscard]]
    Result<void> merge_from(Loongarch64PageMan &) noexcept;

    void switch_root() {
        __switch_root(__root);
    }

    constexpr PhyAddr get_root() {
        return __root;
    }

private:
    PhyAddr __root;
};

static_assert(ArchPageManTrait<Loongarch64PageMan>);
