/**
 * @file slub.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * theflysong(song_of_the_fly@163.com)
 * @brief SLUB Allocator
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <kio.h>
#include <mem/addr.h>
#include <mem/alloc_def.h>
#include <mem/gfp.h>
#include <sus/defer.h>
#include <sus/list.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace slub {
    // Forward declaration to force instantiation of the registrar when the
    // allocator is used.
    template <typename ObjType>
    class SlubCollection;

    constexpr size_t PAGES_PER_SLAB = 1;
    constexpr size_t SLAB_BYTES     = PAGESIZE * PAGES_PER_SLAB;
    constexpr size_t ALIGN          = 16;
    constexpr int SLAB_KMAX         = 2048;

    struct SlubStats {
        size_t total_slabs;
        size_t objects_inuse;
        size_t objects_total;
        size_t memory_usage_bytes;
    };

    void init_chrono_overhead();

    static constexpr uintptr_t align_down(uintptr_t addr, uintptr_t align) {
        return addr & ~(align - 1);
    }
    static constexpr uintptr_t align_up(uintptr_t addr, uintptr_t align) {
        return (addr + align - 1) & ~(align - 1);
    }

    struct SlabHeader {
        enum class SlabState { EMPTY, PARTIAL, FULL };
        util::ListHead<SlabHeader> list_head{};
        void *freelist{};
        size_t inuse{};
        size_t total{};
        SlabState state{};
        SlabHeader()
            : list_head({}),
              freelist(nullptr),
              inuse(0),
              total(0),
              state(SlabState::EMPTY) {}
    };

    static_assert(
        util::IntrusiveListNodeTrait<SlabHeader, &SlabHeader::list_head>,
        "SlabHeader fails to be a valid intrusive list node");

    template <typename ObjType>
    struct size_of_type : public std::size_constant<sizeof(ObjType)> {};

    template <typename ObjType>
    struct align_of_type : public std::size_constant<alignof(ObjType)> {};

    template <typename ObjType>
    concept HugeObjectType = (size_of_type<ObjType>::value >= SLAB_KMAX);

    template <typename ObjType>
    class SlubAllocator {
    protected:
        static constexpr size_t raw_obj_size_  = size_of_type<ObjType>::value;
        static constexpr size_t raw_obj_align_ = align_of_type<ObjType>::value;
        static constexpr bool is_pow2(size_t n) {
            return (n > 0) && ((n & (n - 1)) == 0);
        }
        static_assert(is_pow2(raw_obj_align_),
                      "raw_obj_align_ must be power-of-two");

        static constexpr size_t ptr_size_  = sizeof(void *);
        static constexpr size_t ptr_align_ = alignof(void *);
        // Round up n to multiple of align; align must be power-of-two.
        static constexpr size_t round_up_pow2(size_t n, size_t align) {
            return (n + align - 1) & ~(align - 1);
        }

        // Free-list next pointer is stored in object body, so size/alignment
        // must be at least pointer-sized/pointer-aligned.
        static constexpr size_t max(size_t x, size_t y) {
            return (x > y) ? x : y;
        }
        static constexpr size_t obj_align_ = max(raw_obj_align_, ptr_align_);
        static constexpr size_t obj_size_ =
            round_up_pow2(max(raw_obj_size_, ptr_size_), obj_align_);

        constexpr static size_t pages_      = PAGES_PER_SLAB;
        constexpr static size_t slab_bytes_ = SLAB_BYTES;

        static_assert(is_pow2(obj_align_), "obj_align_ must be power-of-two");

    public:
        inline static SlubAllocator &instance() {
            return SlubCollection<ObjType>::instance();
        }
        SlubAllocator();
        ObjType *alloc();
        void free(ObjType *ptr);

        SlubStats get_stats() const {
            size_t total_slabs   = partial.size() + full.size() + empty.size();
            size_t objects_total = 0;
            if (total_slabs > 0) {
                // All slabs have same capacity
                // We can get it from any slab, or calculate it.
                // Since we don't have a slab instance, we calculate it.
                auto base = (uintptr_t)0;
                auto cur  = align_up(base + sizeof(SlabHeader), obj_align_);
                auto end  = base + slab_bytes_;
                size_t per_slab = 0;
                while (cur + obj_size_ <= end) {
                    per_slab++;
                    cur += obj_size_;
                }
                objects_total = total_slabs * per_slab;
            }
            return {total_slabs, inuse_objects_, objects_total,
                    total_slabs * slab_bytes_};
        }

    private:
        util::IntrusiveList<SlabHeader> partial{};
        util::IntrusiveList<SlabHeader> full{};
        util::IntrusiveList<SlabHeader> empty{};
        size_t inuse_objects_ = 0;

        SlabHeader *new_slab();
        void init_slab_headers(SlabHeader *slab);
        SlabHeader *slab_of(void *p);

        void to_empty(SlabHeader *slab);
        void to_partial(SlabHeader *slab);
        void to_full(SlabHeader *slab);

        void inner_free(void *ptr);
    };

    template <HugeObjectType ObjType>
    class SlubAllocator<ObjType> {
    protected:
        static constexpr size_t obj_pages =
            page_align_up(sizeof(ObjType)) / PAGESIZE;

    public:
        inline static SlubAllocator &instance() {
            return SlubCollection<ObjType>::instance();
        }
        SlubAllocator() : inuse_objects_(0) {}
        ObjType *alloc() {
            PhyAddr p = GFP::get_free_page(obj_pages);
            if (p.nonnull())
                inuse_objects_++;
            return convert<KpaAddr>(p).as<ObjType>();
        }
        void free(ObjType *ptr) {
            if (!ptr) {
                SLUB::WARN("can't free null pointer");
                return;
            }

            KpaAddr p     = (KpaAddr)ptr;
            PhyAddr paddr = convert<PhyAddr>(p);

            GFP::put_page(paddr, obj_pages);
            inuse_objects_--;
        }
        SlubStats get_stats() const {
            return {
                inuse_objects_,  // Each huge object is effectively its own slab
                inuse_objects_, inuse_objects_,
                inuse_objects_ * obj_pages * PAGESIZE};
        }

    private:
        size_t inuse_objects_;
    };

    template <typename ObjType>
    class SlubCollection {
    protected:
        static util::Defer<SlubAllocator<ObjType>> _INSTANCE;
        static util::DeferEntry _SLUB_REGISTER;

    public:
        inline static SlubAllocator<ObjType> &instance() {
            (void)_SLUB_REGISTER;  // Ensure the defer entry is linked in
            return _INSTANCE.get();
        }
    };

    template <typename ObjType>
    util::Defer<SlubAllocator<ObjType>> SlubCollection<ObjType>::_INSTANCE;

    template <typename ObjType>
    POST_DEFER util::DeferEntry SlubCollection<ObjType>::_SLUB_REGISTER =
        SlubCollection<ObjType>::_INSTANCE.make_defer();

    template <typename ObjType>
    SlabHeader *SlubAllocator<ObjType>::slab_of(void *p) {
        auto ptr  = reinterpret_cast<uintptr_t>(p);
        auto base = align_down(ptr, slab_bytes_);
        return reinterpret_cast<SlabHeader *>(base);
    }

    template <typename ObjType>
    void SlubAllocator<ObjType>::init_slab_headers(SlabHeader *slab) {
        auto base       = reinterpret_cast<uintptr_t>(slab);
        auto slab_start = base + sizeof(SlabHeader);
        slab_start      = align_up(slab_start, obj_align_);

        constexpr size_t total =
            (slab_bytes_ - align_up(sizeof(SlabHeader), obj_align_)) /
            obj_size_;
        static_assert(total > 0, "每个 slab 至少应包含一个对象");

        slab->total = total;
        slab->inuse = 0;

        void *head = nullptr;

        // construct from end to start
        for (size_t i = total; i > 0; i--) {
            void *obj =
                reinterpret_cast<void *>(slab_start + (i - 1) * obj_size_);
            *reinterpret_cast<void **>(obj) = head;
            head                            = obj;
        }
        slab->freelist = head;
    }

    template <typename ObjType>
    SlabHeader *SlubAllocator<ObjType>::new_slab() {
        PhyAddr paddr = GFP::get_free_page(pages_);
        if (!paddr.nonnull()) {
            SLUB::ERROR("无法分配新的 slab 内存");
            return nullptr;
        }
        KpaAddr kpaddr   = convert<KpaAddr>(paddr);
        SlabHeader *slab = new (kpaddr.addr()) SlabHeader{};
        init_slab_headers(slab);
        return slab;
    }

    template <typename ObjType>
    void SlubAllocator<ObjType>::to_empty(SlabHeader *slab) {
        if (slab->state == SlabHeader::SlabState::PARTIAL) {
            partial.erase(typename decltype(partial)::iterator(slab));
        } else if (slab->state == SlabHeader::SlabState::FULL) {
            full.erase(typename decltype(full)::iterator(slab));
        }
        slab->state = SlabHeader::SlabState::EMPTY;
        empty.push_back(*slab);
    }

    template <typename ObjType>
    void SlubAllocator<ObjType>::to_partial(SlabHeader *slab) {
        if (slab->state == SlabHeader::SlabState::EMPTY) {
            empty.erase(typename decltype(empty)::iterator(slab));
        } else if (slab->state == SlabHeader::SlabState::FULL) {
            full.erase(typename decltype(full)::iterator(slab));
        }
        slab->state = SlabHeader::SlabState::PARTIAL;
        partial.push_back(*slab);
    }

    // to_full remains mostly same but for consistency safety
    template <typename ObjType>
    void SlubAllocator<ObjType>::to_full(SlabHeader *slab) {
        if (slab->state == SlabHeader::SlabState::PARTIAL) {
            partial.erase(typename decltype(partial)::iterator(slab));
        } else if (slab->state == SlabHeader::SlabState::EMPTY) {
            // Should not happen for to_full usually
            empty.erase(typename decltype(empty)::iterator(slab));
        }
        slab->state = SlabHeader::SlabState::FULL;
        full.push_back(*slab);
    }

    template <typename ObjType>
    ObjType *SlubAllocator<ObjType>::alloc() {
        SlabHeader *slab = nullptr;
        if (!partial.empty()) {
            slab = &partial.back();
            assert(slab != nullptr);
        } else if (!empty.empty()) {
            slab = &empty.back();
            assert(slab != nullptr);
            to_partial(slab);
        } else {
            slab = new_slab();
            if (slab == nullptr) {
                SLUB::ERROR("无法分配新的 slab");
                return nullptr;
            }
            slab->state = SlabHeader::SlabState::PARTIAL;
            partial.push_back(*slab);
        }

        assert(slab != nullptr);
        assert(slab->freelist != nullptr);
        void *obj      = slab->freelist;
        slab->freelist = *reinterpret_cast<void **>(obj);
        slab->inuse++;
        inuse_objects_++;

        if (slab->inuse == slab->total) {
            to_full(slab);
        }
        return (ObjType *)obj;
    }

    template <typename ObjType>
    void SlubAllocator<ObjType>::inner_free(void *ptr) {
        if (!ptr) {
            SLUB::WARN("can't free null pointer");
            return;
        }
        SlabHeader *slab_header         = slab_of(ptr);
        *reinterpret_cast<void **>(ptr) = slab_header->freelist;
        slab_header->freelist           = ptr;
        slab_header->inuse--;
        inuse_objects_--;
        if (slab_header->inuse == 0) {
            to_empty(slab_header);
        } else if (slab_header->inuse == slab_header->total - 1) {
            to_partial(slab_header);
        }
    }

    template <typename ObjType>
    void SlubAllocator<ObjType>::free(ObjType *ptr) {
        if (!ptr) {
            SLUB::WARN("can't free null pointer");
            return;
        }

        inner_free(ptr);
    }

    template <typename ObjType>
    SlubAllocator<ObjType>::SlubAllocator() {}

    template <size_t sz>
    class FixedSizeAllocator {
    public:
        static_assert(is_pow2(sz));

    private:
        class Object {
            char data[sz];
        };
        static util::Defer<slub::SlubAllocator<Object>> SLUB;

    public:
        static void *malloc() {
            return (void *)(SLUB->alloc());
        }
        static void free(void *ptr) {
            SLUB->free((Object *)ptr);
        }
        static void init() {
            // construct the slub object
            SLUB.construct();
        }
    };

    template <size_t sz>
    util::Defer<slub::SlubAllocator<typename FixedSizeAllocator<sz>::Object>>
        FixedSizeAllocator<sz>::SLUB;

    class MixedSizeAllocator {
    private:
        static constexpr size_t KMAX = 2048;
        static constexpr size_t KMIN = 8;

        using _sizes =
            std::index_sequence<8, 16, 32, 64, 128, 256, 512, 1024, 2048>;
        static_assert(KMIN == 8);
        static_assert(KMAX == 2048);

        template <typename T>
        class _Helper;

        template <size_t... sizes>
        class _Helper<std::index_sequence<sizes...>> {
        public:
            static void init() {
                (FixedSizeAllocator<sizes>::init(), ...);
            }

            static bool contains(size_t sz) {
                return ((sz == sizes) || ...);
            }
        };
        using Helper = _Helper<_sizes>;

        // 将sz向上取整到一个2的幂次方
        static size_t up2pow(size_t n) {
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            if constexpr (sizeof(size_t) == 8) {
                n |= n >> 32;
            }
            n++;
            return n;
        }

        struct AllocRecord {
            void *ptr;
            size_t size;
            // TODO: 改成用哈希表/红黑树?
            util::ListHead<AllocRecord> list_head;
            static util::Defer<SlubAllocator<AllocRecord>> SLUB;

            constexpr AllocRecord(void *ptr, size_t size)
                : ptr(ptr), size(size) {}
            constexpr AllocRecord() : AllocRecord(nullptr, 0) {}

            inline void *operator new(size_t sz) {
                assert(sz == sizeof(AllocRecord));
                return SLUB->alloc();
            }
            inline void operator delete(void *ptr) {
                SLUB->free(static_cast<AllocRecord *>(ptr));
            }
        };
        static util::Defer<util::IntrusiveList<AllocRecord>> alloc_records;

        static void add_record(void *ptr, size_t rsz) {
            AllocRecord *record = new AllocRecord(ptr, rsz);
            alloc_records->push_back(*record);
        }

        static AllocRecord *get_record(void *ptr) {
            for (auto &rec : alloc_records.get()) {
                if (rec.ptr == ptr) {
                    return &rec;
                }
            }
            return nullptr;
        }

        static void remove_record(AllocRecord *record) {
            alloc_records->remove(*record);
            delete record;
        }

        static void *large_malloc(size_t rsz) {
            MEMORY::DEBUG("转交到large_malloc途径分配");
            assert(is_pow2(rsz));
            size_t pages = rsz / PAGESIZE;
            assert(pages > 0);
            PhyAddr paddr = GFP::get_free_page(pages);
            if (!paddr.nonnull()) {
                SLUB::ERROR("无法分配大对象内存");
                return nullptr;
            }
            return convert<KpaAddr>(paddr).addr();
        }

        static void *small_malloc(size_t rsz) {
            assert(Helper::contains(rsz));
            switch (rsz) {
                case 8:    return FixedSizeAllocator<8>::malloc();
                case 16:   return FixedSizeAllocator<16>::malloc();
                case 32:   return FixedSizeAllocator<32>::malloc();
                case 64:   return FixedSizeAllocator<64>::malloc();
                case 128:  return FixedSizeAllocator<128>::malloc();
                case 256:  return FixedSizeAllocator<256>::malloc();
                case 512:  return FixedSizeAllocator<512>::malloc();
                case 1024: return FixedSizeAllocator<1024>::malloc();
                case 2048: return FixedSizeAllocator<2048>::malloc();
                default:
                    SLUB::ERROR("不支持的对象大小: %zu", rsz);
                    return nullptr;
            }
        }

        static void _free(void *ptr, size_t rsz) {
            if (rsz >= KMAX) {
                size_t pages   = rsz / PAGESIZE;
                KpaAddr kpaddr = (KpaAddr)ptr;
                GFP::put_page(convert<PhyAddr>(kpaddr), pages);
            } else {
                assert(Helper::contains(rsz));
                switch (rsz) {
                    case 8:    FixedSizeAllocator<8>::free(ptr); return;
                    case 16:   FixedSizeAllocator<16>::free(ptr); return;
                    case 32:   FixedSizeAllocator<32>::free(ptr); return;
                    case 64:   FixedSizeAllocator<64>::free(ptr); return;
                    case 128:  FixedSizeAllocator<128>::free(ptr); return;
                    case 256:  FixedSizeAllocator<256>::free(ptr); return;
                    case 512:  FixedSizeAllocator<512>::free(ptr); return;
                    case 1024: FixedSizeAllocator<1024>::free(ptr); return;
                    case 2048: FixedSizeAllocator<2048>::free(ptr); return;
                    default:   SLUB::ERROR("不支持的对象大小: %zu", rsz); return;
                }
            }
        }

    public:
        static void init() {
            // 构造AllocRecord的SLUB
            alloc_records.construct();
            AllocRecord::SLUB.construct();
            Helper::init();
        }

        static void *malloc(size_t sz) {
            const size_t rsz = std::max(up2pow(sz), KMIN);
            assert(Helper::contains(rsz) || rsz >= KMAX);
            void *ptr = (rsz >= KMAX) ? large_malloc(rsz) : small_malloc(rsz);
            if (ptr == nullptr) {
                MEMORY::ERROR("无法分配内存!");
                return nullptr;
            }
            MEMORY::DEBUG("分配了 %p, size = %d(实际大小为%d)", ptr, sz, rsz);
            add_record(ptr, rsz);
            return ptr;
        }

        static void free(void *ptr) {
            AllocRecord *record = get_record(ptr);
            if (record == nullptr) {
                MEMORY::ERROR("未查询到地址%p分配记录", ptr);
                return;
            }
            _free(ptr, record->size);
            remove_record(record);
        }
    };

    static_assert(KOPTrait<SlubAllocator<int>, int>, "KOP 不满足 KOPTrait");
}  // namespace slub
