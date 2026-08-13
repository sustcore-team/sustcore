/**
 * @file capability.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CNode cell 初始化、slot free list 和 CDT 节点操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <cap/capability.h>
#include <memory/slab/heap.h>
#include <tay/counter.h>
#include <tay/utility.h>

#include <cstddef>
#include <new>

namespace cap {
    namespace {
        constinit tay::counter<u64_t> node_ids{1};
    }  // namespace

    tay::expected<CNode *, CapError> CNode::create_pages(size_t page_count,
                                                         CNodeKind kind) noexcept {
        if (!is_supported_cnode_page_count(page_count))
            return tay::Err(CapError::INVALID_OPERATION);

        const auto capacity = static_cast<u16_t>(cnode_capacity_for_pages(page_count));
        const auto bytes    = (static_cast<size_t>(capacity) + 1) * CNODE_CELL_SIZE;
        auto storage        = memory::alloc(bytes, CNODE_PAGE_SIZE);
        if (!storage)
            return tay::Err(CapError::OUT_OF_MEMORY);

        auto *cells = static_cast<CNodeCell *>(*storage);
        auto *node  = new (std::nothrow)
            CNode(cells, capacity, static_cast<u8_t>(page_count), kind, node_ids.next());
        if (node == nullptr) {
            memory::dealloc(*storage);
            return tay::Err(CapError::OUT_OF_MEMORY);
        }
        return node;
    }

    tay::expected<CNode *, CapError> CNode::create_small() noexcept {
        auto storage = memory::alloc(SMALL_CNODE_SIZE, CNODE_CELL_ALIGNMENT);
        if (!storage)
            return tay::Err(CapError::OUT_OF_MEMORY);

        auto *cells = static_cast<CNodeCell *>(*storage);
        auto *node  = new (std::nothrow) CNode(cells, static_cast<u16_t>(SMALL_CNODE_CAPACITY), 0,
                                               CNodeKind::SMALL, node_ids.next());
        if (node == nullptr) {
            memory::dealloc(*storage);
            return tay::Err(CapError::OUT_OF_MEMORY);
        }
        return node;
    }

    CNode::CNode(CNodeCell *cells, u16_t capacity, u8_t page_count, CNodeKind kind,
                 u64_t node_id) noexcept
        : cells_(cells) {
        for (u16_t slot = 0; slot <= capacity; ++slot) new (&cells_[slot]) CNodeCell{};
        new (&cells_[0].metadata) CNodeMetadata(node_id, capacity, page_count, kind);
        for (u16_t slot = 1; slot <= capacity; ++slot) {
            new (&cells_[slot].free_slot) FreeSlot(slot == capacity ? 0 : slot + 1);
        }
    }

    CNode::~CNode() noexcept {
        state_.store(CNodeState::DESTROYING, std::memory_order_release);
        for (u16_t slot = 1; slot <= metadata().capacity; ++slot) {
            destroy_cell_locked(slot);
        }
        cells_[0].metadata.~CNodeMetadata();
        cells_[0].~CNodeCell();
        if (cells_ != nullptr)
            memory::dealloc(cells_);
    }

    bool CNode::occupied(u16_t slot) const noexcept {
        return slot != 0 && slot <= metadata().capacity &&
               (cell_flags(cells_[slot]) == cell_kind_flags(CellKind::CAPABILITY));
    }

    bool CNode::contains(const Capability *capability) const noexcept {
        if (capability == nullptr)
            return false;
        const auto address = reinterpret_cast<uintptr_t>(capability);
        const auto first   = reinterpret_cast<uintptr_t>(&cells_[1]);
        const auto end     = reinterpret_cast<uintptr_t>(cells_) +
                         (static_cast<size_t>(metadata().capacity) + 1) * CNODE_CELL_SIZE;
        return address >= first && address < end && (address - first) % CNODE_CELL_SIZE == 0;
    }

    u16_t CNode::slot_index(const Capability *capability) const noexcept {
        if (!contains(capability))
            return 0;
        const auto address = reinterpret_cast<uintptr_t>(capability);
        const auto base    = reinterpret_cast<uintptr_t>(cells_);
        return static_cast<u16_t>((address - base) / CNODE_CELL_SIZE);
    }

    tay::expected<u16_t, CapError> CNode::reserve_slot_locked(u16_t requested_slot) noexcept {
        u16_t selected = 0;
        if (requested_slot != 0) {
            if (requested_slot > metadata().capacity || occupied(requested_slot))
                return tay::Err(CapError::INVALID_SLOT);
            u16_t previous = 0;
            for (u16_t current = metadata().free_head; current != 0;
                 current       = cells_[current].free_slot.next_free())
            {
                if (current != requested_slot) {
                    previous = current;
                    continue;
                }
                selected        = current;
                const auto next = cells_[current].free_slot.next_free();
                if (previous == 0)
                    metadata().free_head = next;
                else
                    cells_[previous].free_slot.set_next_free(next);
                break;
            }
            if (selected == 0)
                return tay::Err(CapError::INVALID_SLOT);
        } else {
            selected = metadata().free_head;
            if (selected == 0)
                return tay::Err(CapError::NO_SLOTS);
            metadata().free_head = cells_[selected].free_slot.next_free();
        }
        ++metadata().used_count;
        return selected;
    }

    void CNode::publish_locked(u16_t slot, KernelObject &object, u64_t rights, u64_t badge,
                               u32_t generation, Capability *parent) noexcept {
        cells_[slot].free_slot.~FreeSlot();
        new (&cells_[slot].capability) Capability(object, rights, badge, generation);
        auto &capability = cells_[slot].capability;
        if (parent == nullptr)
            return;
        CapabilityDerivTree tree;
        tree.link_front(*parent, capability);
    }

    ObjectRef<KernelObject> CNode::erase_locked(u16_t slot, bool reparent_children) noexcept {
        if (!occupied(slot))
            return {};
        auto &capability = cells_[slot].capability;
        auto object      = std::move(capability.object);
        auto *parent     = capability.deriv.parent;
        CapabilityDerivTree tree;

        if (reparent_children && capability.deriv.first_child != nullptr) {
            while (capability.deriv.first_child != nullptr) {
                auto *child = *tree.children(capability).begin();
                if (parent != nullptr)
                    tree.reparent(*parent, *child);
                else
                    tree.unlink(*child);
            }
        }
        tree.unlink(capability);

        capability.~Capability();
        new (&cells_[slot].free_slot) FreeSlot(metadata().free_head);
        metadata().free_head = slot;
        --metadata().used_count;
        metadata().state_version.fetch_add(1, std::memory_order_release);
        return object;
    }

    CNode::Cell CNode::cell(u16_t slot) noexcept {
        if (slot > metadata().capacity)
            return {};
        switch (cell_flags(cells_[slot])) {
            case cell_kind_flags(CellKind::CAPABILITY): return &cells_[slot].capability;
            case cell_kind_flags(CellKind::METADATA):   return &cells_[slot].metadata;
            default:                                    return &cells_[slot].free_slot;
        }
    }

    CNode::CCell CNode::cell(u16_t slot) const noexcept {
        if (slot > metadata().capacity)
            return {};
        switch (cell_flags(cells_[slot])) {
            case cell_kind_flags(CellKind::CAPABILITY):
                return static_cast<const Capability *>(&cells_[slot].capability);
            case cell_kind_flags(CellKind::METADATA):
                return static_cast<const CNodeMetadata *>(&cells_[slot].metadata);
            default: return static_cast<const FreeSlot *>(&cells_[slot].free_slot);
        }
    }

    void CNode::destroy_cell_locked(u16_t slot) noexcept {
        if (slot == 0 || slot > metadata().capacity)
            return;
        auto current = cell(slot);
        current.visit(tay::overloaded{[](Capability *value) noexcept { value->~Capability(); },
                                      [](CNodeMetadata *) noexcept {},
                                      [](FreeSlot *value) noexcept { value->~FreeSlot(); }});
        cells_[slot].~CNodeCell();
    }
}  // namespace cap
