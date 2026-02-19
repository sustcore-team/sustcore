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
#include <mem/gfp.h>
#include <sus/list.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace slub {
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
        static constexpr size_t obj_pages = page_align_up(sizeof(ObjType)) / PAGESIZE;
    public:
        SlubAllocator() : inuse_objects_(0) {}
        ObjType *alloc() {
            PhyAddr p =
                GFP::get_free_page(obj_pages);
            if (p.nonnull())
                inuse_objects_++;
            return convert<KpaAddr>(p).as<ObjType>();
        }
        void free(ObjType *ptr) {
            if (!ptr) {
                SLUB::WARN("can't free null pointer");
                return;
            }

            KpaAddr p = (KpaAddr)ptr;
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
        if (! paddr.nonnull()) {
            SLUB::ERROR("无法分配新的 slab 内存");
            return nullptr;
        }
        KpaAddr kpaddr = convert<KpaAddr>(paddr);
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
}  // namespace slub
