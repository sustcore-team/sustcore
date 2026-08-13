/**
 * @file cspace.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 扁平 CSpace 的 token lookup、派生和撤销操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/cspace.h>
#include <synchronized.h>

#include <atomic>
#include <new>
#include <utility>

namespace cap {
    namespace {
        constinit tay::counter<u32_t> cspace_cookies{1};

        /** @brief 分配可编码到 CapToken 且本次启动不复用的 CSpace cookie。 */
        [[nodiscard]] tay::expected<u16_t, CapError> allocate_cookie() noexcept {
            u32_t value = 0;
            if (!cspace_cookies.try_next(static_cast<u32_t>(CAP_TOKEN_COOKIE_MASK), value))
                return tay::Err(CapError::INVALID_OPERATION);
            return static_cast<u16_t>(value);
        }

        [[nodiscard]] bool rights_contain(u64_t available, u64_t required) noexcept {
            return (available & required) == required;
        }
    }  // namespace

    tay::expected<CSpace *, CapError> CSpace::create() noexcept {
        auto cookie = allocate_cookie();
        if (!cookie)
            return tay::Err(cookie.error());
        auto *space = new (std::nothrow) CSpace(*cookie);
        if (space == nullptr)
            return tay::Err(CapError::OUT_OF_MEMORY);

        auto root = CNode::create_pages(CNODE_DEFAULT_PAGE_COUNT, CNodeKind::ROOT);
        if (!root) {
            delete space;
            return tay::Err(root.error());
        }
        auto attached = space->attach(**root);
        if (!attached) {
            delete *root;
            delete space;
            return tay::Err(attached.error());
        }
        return space;
    }

    CSpace::CSpace(u16_t cookie) noexcept : cookie_(cookie) {
        for (size_t index = 0; index < MAX_CNODES; ++index) {
            nodes_[index].store(nullptr, std::memory_order_relaxed);
            generation_counters_[index].reset(1);
        }
    }

    CSpace::~CSpace() noexcept {
        for (auto &node : nodes_) {
            auto *current = node.exchange(nullptr, std::memory_order_acq_rel);
            if (current != nullptr)
                delete current;
        }
    }

    CNode *CSpace::node_at(u8_t index) const noexcept {
        return nodes_[index].load(std::memory_order_acquire);
    }

    tay::expected<u8_t, CapError> CSpace::attach(CNode &node) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        if (node.metadata().owner != nullptr || node.state() == CNodeState::DESTROYING)
            return tay::Err(CapError::INVALID_OPERATION);

        for (u16_t index = 0; index < MAX_CNODES; ++index) {
            auto *expected = static_cast<CNode *>(nullptr);
            if (!nodes_[index].compare_exchange_strong(expected, &node, std::memory_order_release,
                                                       std::memory_order_relaxed))
            {
                continue;
            }
            node.metadata().owner = this;
            return static_cast<u8_t>(index);
        }
        return tay::Err(CapError::NO_SLOTS);
    }

    tay::expected<CNode *, CapError> CSpace::detach(u8_t index) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        if (index == ROOT_CNODE_INDEX)
            return tay::Err(CapError::INVALID_OPERATION);
        auto *node = node_at(index);
        if (node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);
        kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
        if (node->used_count() != 0 || node->state() != CNodeState::MUTABLE)
            return tay::Err(CapError::BUSY);
        nodes_[index].store(nullptr, std::memory_order_release);
        node->metadata().owner = nullptr;
        return node;
    }

    tay::expected<u32_t, CapError> CSpace::next_generation_locked(u8_t cnode_index) noexcept {
        u32_t generation = 0;
        if (!generation_counters_[cnode_index].try_next(CAP_TOKEN_MAX_GENERATION, generation))
            return tay::Err(CapError::INVALID_OPERATION);
        return generation;
    }

    tay::expected<CapToken, CapError> CSpace::install(KernelObject &object, u64_t rights,
                                                      u64_t badge, u8_t cnode_index,
                                                      u16_t requested_slot) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        auto *node = node_at(cnode_index);
        if (node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);
        if (object.state() != ObjectState::ALIVE)
            return tay::Err(CapError::INVALID_OPERATION);

        kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
        auto generation = next_generation_locked(cnode_index);
        if (!generation)
            return tay::Err(generation.error());
        auto slot = node->reserve_slot_locked(requested_slot);
        if (!slot)
            return tay::Err(slot.error());
        node->publish_locked(*slot, object, rights, badge, *generation, nullptr);
        return encode_token(cookie_, *generation, cnode_index, *slot);
    }

    tay::expected<CapPin, CapError> CSpace::resolve(CapToken token, ObjectType expected_type,
                                                    u64_t required_rights) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CapError::INVALID_TOKEN);
        auto *node = node_at(fields.cnode_index);
        if (node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);

        kernel::lock_guard<tay::spinlock> guard(node->lock_);
        if (!node->occupied(fields.slot_index))
            return tay::Err(CapError::INVALID_SLOT);
        auto &capability = node->cells_[fields.slot_index].capability;
        if (capability.generation() != fields.generation)
            return tay::Err(CapError::STALE_TOKEN);
        if (expected_type != ObjectType::NONE && capability.type() != expected_type)
            return tay::Err(CapError::TYPE_MISMATCH);
        if (!rights_contain(capability.rights, required_rights))
            return tay::Err(CapError::INSUFFICIENT_RIGHTS);
        if (!capability.object || capability.object->object_type() != capability.type())
            return tay::Err(CapError::INVALID_OPERATION);
        auto pin = tay::pin_guard<KernelObject>::try_pin(*capability.object);
        if (!pin)
            return tay::Err(CapError::INVALID_OPERATION);
        return CapPin(std::move(pin), capability.rights, capability.badge);
    }

    tay::expected<CapToken, CapError> CSpace::copy(CapToken source, u64_t requested_rights,
                                                   u8_t destination_cnode,
                                                   u16_t requested_slot) noexcept {
        const auto fields = decode_token(source);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CapError::INVALID_TOKEN);

        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *source_node = node_at(fields.cnode_index);
        auto *target_node = node_at(destination_cnode);
        if (source_node == nullptr || target_node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);

        tay::unique_lock first_lock(source_node->lock_, tay::defer_lock);
        tay::unique_lock second_lock(target_node->lock_, tay::defer_lock);
        if (source_node != target_node && source_node->node_id() > target_node->node_id()) {
            first_lock.swap(second_lock);
        }
        first_lock.lock();
        if (source_node != target_node)
            second_lock.lock();

        if (!source_node->occupied(fields.slot_index))
            return tay::Err(CapError::INVALID_SLOT);
        auto &source_cap = source_node->cells_[fields.slot_index].capability;
        if (source_cap.generation() != fields.generation)
            return tay::Err(CapError::STALE_TOKEN);
        if ((source_cap.rights & RIGHT_COPY) == 0)
            return tay::Err(CapError::INSUFFICIENT_RIGHTS);
        const auto rights = requested_rights == 0 ? source_cap.rights : requested_rights;
        if (!rights_contain(source_cap.rights, rights))
            return tay::Err(CapError::INSUFFICIENT_RIGHTS);

        auto generation = next_generation_locked(destination_cnode);
        if (!generation)
            return tay::Err(generation.error());
        auto slot = target_node->reserve_slot_locked(requested_slot);
        if (!slot)
            return tay::Err(slot.error());
        target_node->publish_locked(*slot, *source_cap.object, rights, source_cap.badge,
                                    *generation, &source_cap);
        return encode_token(cookie_, *generation, destination_cnode, *slot);
    }

    tay::expected<CapToken, CapError> CSpace::mint(CapToken source, u64_t requested_rights,
                                                   u64_t badge, u8_t destination_cnode,
                                                   u16_t requested_slot) noexcept {
        const auto fields = decode_token(source);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CapError::INVALID_TOKEN);

        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *source_node = node_at(fields.cnode_index);
        auto *target_node = node_at(destination_cnode);
        if (source_node == nullptr || target_node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);

        tay::unique_lock first_lock(source_node->lock_, tay::defer_lock);
        tay::unique_lock second_lock(target_node->lock_, tay::defer_lock);
        if (source_node != target_node && source_node->node_id() > target_node->node_id())
            first_lock.swap(second_lock);
        first_lock.lock();
        if (source_node != target_node)
            second_lock.lock();

        if (!source_node->occupied(fields.slot_index))
            return tay::Err(CapError::INVALID_SLOT);
        auto &source_cap = source_node->cells_[fields.slot_index].capability;
        if (source_cap.generation() != fields.generation)
            return tay::Err(CapError::STALE_TOKEN);
        if ((source_cap.rights & RIGHT_MINT) == 0)
            return tay::Err(CapError::INSUFFICIENT_RIGHTS);
        if (!rights_contain(source_cap.rights, requested_rights))
            return tay::Err(CapError::INSUFFICIENT_RIGHTS);

        auto generation = next_generation_locked(destination_cnode);
        if (!generation)
            return tay::Err(generation.error());
        auto slot = target_node->reserve_slot_locked(requested_slot);
        if (!slot)
            return tay::Err(slot.error());
        target_node->publish_locked(*slot, *source_cap.object, requested_rights, badge, *generation,
                                    &source_cap);
        return encode_token(cookie_, *generation, destination_cnode, *slot);
    }

    tay::expected<void, CapError> CSpace::delete_cap(CapToken token) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CapError::INVALID_TOKEN);
        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *node = node_at(fields.cnode_index);
        if (node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);
        ObjectRef<KernelObject> object{};
        {
            kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
            if (!node->occupied(fields.slot_index))
                return tay::Err(CapError::INVALID_SLOT);
            if (node->cells_[fields.slot_index].capability.generation() != fields.generation)
                return tay::Err(CapError::STALE_TOKEN);
            object = node->erase_locked(fields.slot_index, true);
        }
        return {};
    }

    bool CSpace::locate_capability(Capability *capability, CNode *&node, u16_t &slot) noexcept {
        for (auto &entry : nodes_) {
            auto *candidate = entry.load(std::memory_order_acquire);
            if (candidate == nullptr)
                continue;
            kernel::lock_guard<tay::spinlock> guard(candidate->lock_);
            if (!candidate->contains(capability))
                continue;
            const auto candidate_slot = candidate->slot_index(capability);
            if (!candidate->occupied(candidate_slot))
                return false;
            node = candidate;
            slot = candidate_slot;
            return true;
        }
        return false;
    }

    tay::expected<void, CapError> CSpace::revoke(CapToken token) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CapError::INVALID_TOKEN);
        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *source_node = node_at(fields.cnode_index);
        if (source_node == nullptr)
            return tay::Err(CapError::MISSING_CNODE);
        CapabilityDerivTree tree;
        auto *source = static_cast<Capability *>(nullptr);
        {
            kernel::lock_guard<tay::spinlock> guard(source_node->lock_);
            if (!source_node->occupied(fields.slot_index))
                return tay::Err(CapError::INVALID_SLOT);
            source = &source_node->cells_[fields.slot_index].capability;
            if (source->generation() != fields.generation)
                return tay::Err(CapError::STALE_TOKEN);
            if ((source->rights & RIGHT_REVOKE) == 0)
                return tay::Err(CapError::INSUFFICIENT_RIGHTS);
        }

        // mutation_lock_ 固定整棵 CDT；每轮沿首子链找到叶节点，后序回收避免悬空父指针。
        for (;;) {
            Capability *child = nullptr;
            {
                kernel::lock_guard<tay::spinlock> guard(source_node->lock_);
                auto children = tree.children(*source);
                if (children.begin() != children.end())
                    child = *children.begin();
            }
            if (child == nullptr)
                break;
            for (;;) {
                auto children = tree.children(*child);
                if (children.begin() == children.end())
                    break;
                child = *children.begin();
            }

            CNode *child_node = nullptr;
            u16_t child_slot  = 0;
            if (!locate_capability(child, child_node, child_slot))
                return tay::Err(CapError::INVALID_OPERATION);
            ObjectRef<KernelObject> object{};
            {
                kernel::lock_guard<tay::spinlock> guard(child_node->lock_);
                if (!child_node->occupied(child_slot) ||
                    &child_node->cells_[child_slot].capability != child)
                {
                    continue;
                }
                object = child_node->erase_locked(child_slot, false);
            }
        }
        return {};
    }
}  // namespace cap
