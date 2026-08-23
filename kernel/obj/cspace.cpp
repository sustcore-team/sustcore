/**
 * @file cspace.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 扁平 CSpace 的 token lookup、派生和撤销操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <obj/cspace.h>
#include <synchronized.h>
#include <tay/unique_ptr.h>

#include <atomic>
#include <new>
#include <utility>

namespace cap {
    namespace {
        constinit tay::counter<u32_t> cspace_cookies{1};

        /** @brief 分配可编码到 CToken 且本次启动不复用的 CSpace cookie。 */
        [[nodiscard]] tay::expected<u16_t, CError> allocate_cookie() noexcept {
            u32_t value = 0;
            if (!cspace_cookies.try_next(static_cast<u32_t>(CAP_TOKEN_COOKIE_MASK), value))
                return tay::Err(CError::OperationRejected(CError::Operation::COOKIE_EXHAUSTED));
            return static_cast<u16_t>(value);
        }

        [[nodiscard]] bool rights_contain(u64_t available, u64_t required) noexcept {
            return (available & required) == required;
        }

        class CNodeLocks final {
        public:
            CNodeLocks(tay::spinlock &source_lock, u64_t source_id, tay::spinlock &target_lock,
                       u64_t target_id, bool same_node) noexcept
                : first_(source_lock, tay::defer_lock),
                  second_(target_lock, tay::defer_lock),
                  same_node_(same_node) {
                if (!same_node_ && source_id > target_id)
                    first_.swap(second_);
                first_.lock();
                if (!same_node_)
                    second_.lock();
            }

            CNodeLocks(const CNodeLocks &)            = delete;
            CNodeLocks &operator=(const CNodeLocks &) = delete;

        private:
            tay::unique_lock<tay::spinlock> first_;
            tay::unique_lock<tay::spinlock> second_;
            bool same_node_ = false;
        };
    }  // namespace

    tay::expected<CSpace *, CError> CSpace::create() noexcept {
        const u16_t cookie = TAY_TRY(allocate_cookie());
        tay::unique_ptr<CSpace> space(new (std::nothrow) CSpace(cookie));
        if (!space)
            return tay::Err(CError::OutOfMemory());

        auto root = CNode::create_pages(CNODE_DEFAULT_PAGE_COUNT, CNodeKind::ROOT);
        if (!root)
            return TAY_ERR(root);
        tay::unique_ptr<CNode> root_owner(*root);
        auto attached = space->attach(**root);
        if (!attached)
            return TAY_ERR(attached);
        static_cast<void>(root_owner.release());
        return space.release();
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

    tay::expected<u8_t, CError> CSpace::attach(CNode &node) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        if (node.metadata().owner != nullptr)
            return tay::Err(CError::OperationRejected(CError::Operation::NODE_OWNED));
        if (node.state() == CNodeState::DESTROYING)
            return tay::Err(CError::OperationRejected(CError::Operation::NODE_DESTROYING));

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
        return tay::Err(CError::OperationRejected(CError::Operation::CNODE_DIRECTORY_FULL));
    }

    tay::expected<CNode *, CError> CSpace::detach(u8_t index) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        if (index == ROOT_CNODE_INDEX)
            return tay::Err(CError::OperationRejected(CError::Operation::ROOT_DETACH));
        auto *node = node_at(index);
        if (node == nullptr)
            return tay::Err(CError::MissingCNode({}, index));
        kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
        if (node->used_count() != 0 || node->state() != CNodeState::MUTABLE)
            return tay::Err(CError::Busy(index));
        nodes_[index].store(nullptr, std::memory_order_release);
        node->metadata().owner = nullptr;
        return node;
    }

    tay::expected<u32_t, CError> CSpace::next_gen_locked(u8_t cnode_index) noexcept {
        u32_t generation = 0;
        if (!generation_counters_[cnode_index].try_next(CAP_TOKEN_MAX_GENERATION, generation))
            return tay::Err(CError::OperationRejected(CError::Operation::GENERATION_EXHAUSTED));
        return generation;
    }

    tay::expected<CToken, CError> CSpace::install(KObject &object, u64_t rights, u64_t badge,
                                                  u8_t cnode_index, u16_t dst_slot) noexcept {
        kernel::lock_guard<tay::spinlock> guard(mutation_lock_);
        auto *node = node_at(cnode_index);
        if (node == nullptr)
            return tay::Err(CError::MissingCNode({}, cnode_index));
        if (object.state() != KObjectState::ALIVE)
            return tay::Err(CError::OperationRejected(CError::Operation::OBJECT_RETIRING));

        kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
        const u32_t generation = TAY_TRY(next_gen_locked(cnode_index));
        const u16_t slot       = TAY_TRY(node->reserve_locked(dst_slot, cnode_index));
        node->publish_locked(slot, object, rights, badge, generation, nullptr);
        return encode_token(cookie_, generation, cnode_index, slot);
    }

    tay::expected<CPin, CError> CSpace::resolve(CToken token, ObjectType expected_type,
                                                u64_t required_rights) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CError::InvalidToken(token));
        auto *node = node_at(fields.cnode_index);
        if (node == nullptr)
            return tay::Err(CError::MissingCNode(token, fields.cnode_index));

        kernel::lock_guard<tay::spinlock> guard(node->lock_);
        if (!node->occupied(fields.slot_index))
            return tay::Err(CError::InvalidSlot(token, fields.slot_index));
        auto &capability = node->cells_[fields.slot_index].capability;
        if (capability.generation() != fields.generation)
            return tay::Err(CError::StaleToken(token, capability.generation()));
        if (expected_type != ObjectType::NONE && capability.type() != expected_type)
            return tay::Err(CError::TypeMismatch(token, expected_type, capability.type()));
        if (!rights_contain(capability.rights, required_rights))
            return tay::Err(CError::InsufficientRights(token, required_rights, capability.rights));
        if (!capability.object || capability.object->type() != capability.type())
            kernel::log::panic("Capability object/type 关系损坏");
        auto pin = tay::pin_guard<KObject>::try_pin(*capability.object);
        if (!pin)
            return tay::Err(CError::OperationRejected(CError::Operation::PIN_FAILED));
        return CPin(std::move(pin), capability.rights, capability.badge);
    }

    tay::expected<CToken, CError> CSpace::copy(CToken src_cap, u64_t new_rights, u8_t dst_cnode,
                                               u16_t dst_slot) noexcept {
        return derive_cap(src_cap, new_rights, RIGHT_COPY, true, DeriveBadge::INHERIT, 0, dst_cnode,
                          dst_slot);
    }

    tay::expected<CToken, CError> CSpace::mint(CToken src_cap, u64_t new_rights, u64_t badge,
                                               u8_t dst_cnode, u16_t dst_slot) noexcept {
        return derive_cap(src_cap, new_rights, RIGHT_MINT, false, DeriveBadge::REPLACE, badge,
                          dst_cnode, dst_slot);
    }

    tay::expected<CToken, CError> CSpace::derive_cap(CToken src_cap, u64_t new_rights,
                                                     u64_t required_right, bool zero_rights_inherit,
                                                     DeriveBadge badge_policy, u64_t badge,
                                                     u8_t dst_cnode, u16_t dst_slot) noexcept {
        const auto fields = decode_token(src_cap);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CError::InvalidToken(src_cap));

        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *src_node = node_at(fields.cnode_index);
        auto *dst_node = node_at(dst_cnode);
        if (src_node == nullptr)
            return tay::Err(CError::MissingCNode(src_cap, fields.cnode_index));
        if (dst_node == nullptr)
            return tay::Err(CError::MissingCNode({}, dst_cnode));

        CNodeLocks node_locks(src_node->lock_, src_node->node_id(), dst_node->lock_,
                              dst_node->node_id(), src_node == dst_node);

        if (!src_node->occupied(fields.slot_index))
            return tay::Err(CError::InvalidSlot(src_cap, fields.slot_index));
        auto &src_entry = src_node->cells_[fields.slot_index].capability;
        if (src_entry.generation() != fields.generation)
            return tay::Err(CError::StaleToken(src_cap, src_entry.generation()));
        if ((src_entry.rights & required_right) == 0)
            return tay::Err(CError::InsufficientRights(src_cap, required_right, src_entry.rights));
        const auto rights = zero_rights_inherit && new_rights == 0 ? src_entry.rights : new_rights;
        if (!rights_contain(src_entry.rights, rights))
            return tay::Err(CError::InsufficientRights(src_cap, rights, src_entry.rights));

        const u32_t generation    = TAY_TRY(next_gen_locked(dst_cnode));
        const u16_t slot          = TAY_TRY(dst_node->reserve_locked(dst_slot, dst_cnode));
        const u64_t derived_badge = badge_policy == DeriveBadge::INHERIT ? src_entry.badge : badge;
        dst_node->publish_locked(slot, *src_entry.object, rights, derived_badge, generation,
                                 &src_entry);
        return encode_token(cookie_, generation, dst_cnode, slot);
    }

    tay::expected<void, CError> CSpace::delete_cap(CToken token) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CError::InvalidToken(token));
        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *node = node_at(fields.cnode_index);
        if (node == nullptr)
            return tay::Err(CError::MissingCNode(token, fields.cnode_index));
        KObjectRef<KObject> object{};
        {
            kernel::lock_guard<tay::spinlock> node_guard(node->lock_);
            if (!node->occupied(fields.slot_index))
                return tay::Err(CError::InvalidSlot(token, fields.slot_index));
            const auto observed = node->cells_[fields.slot_index].capability.generation();
            if (observed != fields.generation)
                return tay::Err(CError::StaleToken(token, observed));
            object = node->erase_locked(fields.slot_index, true);
        }
        return {};
    }

    bool CSpace::find_cap(Capability *capability, CNode *&node, u16_t &slot) noexcept {
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

    tay::expected<void, CError> CSpace::revoke(CToken token) noexcept {
        const auto fields = decode_token(token);
        if (!fields.valid || fields.cspace_cookie != cookie_)
            return tay::Err(CError::InvalidToken(token));
        kernel::lock_guard<tay::spinlock> cspace_guard(mutation_lock_);
        auto *src_node = node_at(fields.cnode_index);
        if (src_node == nullptr)
            return tay::Err(CError::MissingCNode(token, fields.cnode_index));
        CTree tree;
        auto *src_cap = static_cast<Capability *>(nullptr);
        {
            kernel::lock_guard<tay::spinlock> guard(src_node->lock_);
            if (!src_node->occupied(fields.slot_index))
                return tay::Err(CError::InvalidSlot(token, fields.slot_index));
            src_cap = &src_node->cells_[fields.slot_index].capability;
            if (src_cap->generation() != fields.generation)
                return tay::Err(CError::StaleToken(token, src_cap->generation()));
            if ((src_cap->rights & RIGHT_REVOKE) == 0)
                return tay::Err(CError::InsufficientRights(token, RIGHT_REVOKE, src_cap->rights));
        }

        // mutation_lock_ 固定整棵 CDT；每轮沿首子链找到叶节点，后序回收避免悬空父指针。
        while (true) {
            Capability *child = nullptr;
            {
                kernel::lock_guard<tay::spinlock> guard(src_node->lock_);
                auto children = tree.children(*src_cap);
                if (children.begin() != children.end())
                    child = *children.begin();
            }
            if (child == nullptr)
                break;
            while (true) {
                auto children = tree.children(*child);
                if (children.begin() == children.end())
                    break;
                child = *children.begin();
            }

            CNode *child_node = nullptr;
            u16_t child_slot  = 0;
            if (!find_cap(child, child_node, child_slot))
                return tay::Err(CError::OperationRejected(CError::Operation::CDT_INCONSISTENT));
            KObjectRef<KObject> object{};
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
