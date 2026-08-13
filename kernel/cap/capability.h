/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 固定布局 Capability cell 与 CNode 存储。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <obj/kernel_object.h>
#include <synchronized.h>
#include <tay/expected.h>
#include <tay/intrusive.h>
#include <tay/spinlock.h>
#include <tay/tree.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <atomic>
#include <cstddef>

namespace cap {
    class CSpace;

    /** @brief CNode 的存储形态和目录角色。 */
    enum class CNodeKind : u8_t {
        ROOT,
        REGULAR,
        SMALL,
    };

    /** @brief CNode 在本地修改和后续事务协议中的状态。 */
    enum class CNodeState : u8_t {
        MUTABLE,
        SEALED,
        IN_FLIGHT,
        ATTACHED,
        DESTROYING,
    };

    struct Capability;

    /**
     * @brief Capability Deriv Tree 使用的紧凑侵入式链接。
     * @note 链接不拥有节点；节点生命周期由对应 CNode slot 管理。
     */
    using DerivLink = tay::compact_intrusive_tree_hook<Capability>;

    enum class CellKind : u16_t {
        FREE_SLOT  = 0,
        CAPABILITY = 1,
        METADATA   = 2,
    };

    inline constexpr u16_t CELL_KIND_MASK = 0x3;
    [[nodiscard]] constexpr u16_t cell_kind_flags(CellKind kind) noexcept {
        return static_cast<u16_t>(kind);
    }
    inline constexpr u16_t CAPABILITY_FLAG_OCCUPIED = cell_kind_flags(CellKind::CAPABILITY);

    struct CellHeader {
        u16_t flags = cell_kind_flags(CellKind::FREE_SLOT);
        u16_t aux   = 0;
        u32_t data  = 0;
    };
    static_assert(sizeof(CellHeader) == sizeof(u64_t));

    /**
     * @brief CNode slot 中已发布 Capability 的固定 64-byte 表示。
     *
     * `object` 通过单指针 `ObjectRef` 对目标持有一个对象强引用。`deriv` 将副本和 mint 结果接入
     * Capability Deriv Tree；树只表达撤销关系，不拥有对象或 slot。
     *
     * @note 字段布局属于内核内部 ABI，修改时必须保持下方尺寸和对齐断言成立。
     */
    struct alignas(CAPABILITY_ALIGNMENT) Capability {
        Capability(KernelObject &object, u64_t rights, u64_t badge, u32_t generation) noexcept
            : header{.flags = cell_kind_flags(CellKind::CAPABILITY),
                     .aux   = static_cast<u16_t>(object.object_type()),
                     .data  = generation},
              object(object),
              rights(rights),
              badge(badge) {}

        CellHeader header{};
        ObjectRef<KernelObject> object{};
        u64_t rights = 0;
        u64_t badge  = 0;
        DerivLink deriv{};

        [[nodiscard]] u32_t generation() const noexcept {
            return header.data;
        }
        [[nodiscard]] ObjectType type() const noexcept {
            return static_cast<ObjectType>(header.aux);
        }
    };

    static_assert(sizeof(Capability) == CAPABILITY_SIZE);
    static_assert(alignof(Capability) == CAPABILITY_ALIGNMENT);

    using CapabilityDerivTree =
        tay::intrusive_tree<Capability,
                            tay::locate_member<Capability, DerivLink, &Capability::deriv>>;

    /**
     * @brief 占用 CNode cell 0 的固定布局元数据。
     * @note `owner` 仅为借用关系；CSpace attach/detach 在 mutation lock 下更新它。
     */
    struct alignas(CNODE_CELL_ALIGNMENT) CNodeMetadata {
        CNodeMetadata(u64_t node_id, u16_t capacity, u8_t page_count, CNodeKind kind) noexcept
            : header{.flags = cell_kind_flags(CellKind::METADATA),
                     .aux   = static_cast<u16_t>(kind),
                     .data  = 0},
              node_id(node_id),
              capacity(capacity),
              free_head(capacity == 0 ? 0 : 1),
              page_count(page_count),
              kind(kind) {}

        CellHeader header{};
        std::atomic<u32_t> lock_word{0};
        std::atomic<u32_t> state_version{0};
        CSpace *owner     = nullptr;
        u64_t node_id     = 0;
        u64_t transfer_id = 0;
        u16_t capacity    = 0;
        u16_t used_count  = 0;
        u16_t free_head   = 0;
        u8_t page_count   = 0;
        CNodeKind kind    = CNodeKind::REGULAR;

        [[nodiscard]] u32_t generation_seed() const noexcept {
            return header.data;
        }
    };

    /** @brief 空闲 slot 的侵入式 free-list 节点。 */
    struct alignas(CNODE_CELL_ALIGNMENT) FreeSlot {
        explicit FreeSlot(u16_t next_free = 0) noexcept
            : header{.flags = cell_kind_flags(CellKind::FREE_SLOT), .aux = next_free, .data = 0} {}

        CellHeader header{};
        std::byte reserved[CNODE_CELL_SIZE - sizeof(CellHeader)]{};

        [[nodiscard]] u16_t next_free() const noexcept {
            return header.aux;
        }
        void set_next_free(u16_t value) noexcept {
            header.aux = value;
        }
    };

    /**
     * @brief 在元数据、Capability 和空闲节点之间复用的 64-byte CNode cell。
     * @note 活跃 union member 由 CNode 在持锁状态下显式构造和析构。
     */
    union alignas(CNODE_CELL_ALIGNMENT) CNodeCell {
        CNodeMetadata metadata;
        Capability capability;
        FreeSlot free_slot;

        CNodeCell() noexcept {}
        ~CNodeCell() noexcept {}
    };

    [[nodiscard]] inline u16_t cell_flags(const CNodeCell &cell) noexcept {
        return cell.metadata.header.flags;
    }

    static_assert(sizeof(CNodeMetadata) == CNODE_METADATA_SIZE);
    static_assert(sizeof(FreeSlot) == CNODE_CELL_SIZE);
    static_assert(sizeof(CNodeCell) == CNODE_CELL_SIZE);
    static_assert(alignof(CNodeCell) == CNODE_CELL_ALIGNMENT);

    /**
     * @brief 拥有固定容量 slot 数组的 Capability 节点。
     *
     * cell 0 保存元数据，其余 cell 在 `Capability` 和 `FreeSlot` 之间切换。slot 分配使用
     * 内嵌 free list，不产生额外分配；对 slot、CDT 和元数据的修改由 `lock_` 串行化。
     *
     * @warning attach 后 CNode 由 CSpace 拥有，只能在成功 detach 后由调用方销毁。
     */
    class CNode final {
    public:
        /**
         * @brief 创建由整页 cell 存储支持的 CNode。
         * @param page_count 设计允许的页数。
         * @param kind 节点角色。
         * @return 成功时返回独占 CNode 指针；参数非法或分配失败时返回错误。
         */
        [[nodiscard]] static tay::expected<CNode *, CapError> create_pages(
            size_t page_count, CNodeKind kind = CNodeKind::REGULAR) noexcept;

        /**
         * @brief 创建固定小容量 CNode。
         * @return 成功时返回独占 CNode 指针；分配失败时返回 `OUT_OF_MEMORY`。
         */
        [[nodiscard]] static tay::expected<CNode *, CapError> create_small() noexcept;

        CNode(const CNode &)            = delete;
        CNode &operator=(const CNode &) = delete;
        CNode(CNode &&)                 = delete;
        CNode &operator=(CNode &&)      = delete;
        ~CNode() noexcept;

        [[nodiscard]] u16_t capacity() const noexcept {
            return metadata().capacity;
        }

        [[nodiscard]] u16_t used_count() const noexcept {
            return metadata().used_count;
        }

        [[nodiscard]] CNodeKind kind() const noexcept {
            return metadata().kind;
        }

        [[nodiscard]] CNodeState state() const noexcept {
            return state_.load(std::memory_order_acquire);
        }

        [[nodiscard]] u64_t node_id() const noexcept {
            return metadata().node_id;
        }

        using Cell  = tay::variant<Capability *, CNodeMetadata *, FreeSlot *>;
        using CCell = tay::variant<const Capability *, const CNodeMetadata *, const FreeSlot *>;

        [[nodiscard]] Cell cell(u16_t slot) noexcept;
        [[nodiscard]] CCell cell(u16_t slot) const noexcept;

        /** @pre 调用方已持有节点锁，且 slot 当前为 Capability。 */
        [[nodiscard]] Capability *capability_at(u16_t slot) noexcept {
            auto value         = cell(slot);
            Capability *result = nullptr;
            if (value) {
                value.visit(tay::overloaded{
                    [&](Capability *pointer) noexcept { result = pointer; },
                    [](CNodeMetadata *) noexcept {},
                    [](FreeSlot *) noexcept {},
                });
            }
            return result;
        }

    private:
        friend class CSpace;

        CNode(CNodeCell *cells, u16_t capacity, u8_t page_count, CNodeKind kind,
              u64_t node_id) noexcept;

        [[nodiscard]] CNodeMetadata &metadata() noexcept {
            return cells_[0].metadata;
        }

        void destroy_cell_locked(u16_t slot) noexcept;

        [[nodiscard]] const CNodeMetadata &metadata() const noexcept {
            return cells_[0].metadata;
        }

        /** @pre 调用方持有 `lock_`，或节点尚未对其他执行上下文可见。 */
        [[nodiscard]] bool occupied(u16_t slot) const noexcept;

        /** @pre CSpace mutation lock 保证目录和 cell 存储在检查期间稳定。 */
        [[nodiscard]] bool contains(const Capability *capability) const noexcept;

        /** @pre `capability` 指向当前 CNode 的已分配 cell。 */
        [[nodiscard]] u16_t slot_index(const Capability *capability) const noexcept;

        /**
         * @brief 从 free list 预留 slot。
         * @param requested_slot 零表示选择 free-list 首项，否则请求指定 slot。
         * @return 预留的 slot，或 `INVALID_SLOT`/`NO_SLOTS`。
         * @pre 调用方持有 `lock_`。
         */
        [[nodiscard]] tay::expected<u16_t, CapError> reserve_slot_locked(
            u16_t requested_slot = 0) noexcept;

        /**
         * @brief 在预留 slot 中发布 Capability 并建立对象引用和 CDT 链接。
         * @pre 调用方持有 `lock_`；跨节点派生时还持有 parent 所在节点的锁。
         */
        void publish_locked(u16_t slot, KernelObject &object, u64_t rights, u64_t badge,
                            u32_t generation, Capability *parent) noexcept;

        /**
         * @brief 从 slot 移除 Capability 并把 cell 归还 free list。
         * @param slot 要回收的已占用 slot。
         * @param reparent_children 为 true 时把直接子节点提升到当前父节点，否则要求调用方
         * 已按后序删除派生子树。
         * @return 从原 Capability 移出的对象强引用；其析构必须发生在 CNode 锁外。
         * @pre 调用方持有 `lock_` 和 CSpace mutation lock。
         */
        [[nodiscard]] ObjectRef<KernelObject> erase_locked(u16_t slot,
                                                           bool reparent_children) noexcept;

        CNodeCell *cells_ = nullptr;
        std::atomic<CNodeState> state_{CNodeState::MUTABLE};
        tay::spinlock lock_{};
    };
}  // namespace cap
