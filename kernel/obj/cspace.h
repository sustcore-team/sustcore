/**
 * @file cspace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 扁平 CSpace、CToken lookup 和基础 capability 操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cap/capability.h>
#include <obj/kobject.h>
#include <tay/counter.h>

#include <atomic>
#include <concepts>
#include <new>
#include <type_traits>
#include <utility>

namespace cap {
    /**
     * @brief typed resolve 返回的对象访问句柄。
     *
     * 句柄持有 `CPin`，因此 `object()` 的借用指针在句柄释放前保持有效。rights 和 badge
     * 是 resolve 成功时的快照，不会随原 Capability slot 的后续删除而变化。
     *
     * @tparam T 声明静态 `TYPE` 的具体 KObject 类型。
     */
    template <CTarget T>
    class CHandle final {
    public:
        /** @brief 接管已经完成类型检查的对象 pin。 */
        CHandle(CPin &&pin) noexcept : pin_(std::move(pin)) {}
        CHandle(const CHandle &)                = delete;
        CHandle &operator=(const CHandle &)     = delete;
        CHandle(CHandle &&) noexcept            = default;
        CHandle &operator=(CHandle &&) noexcept = default;

        /** @brief 返回 pin 生命周期内有效的具体对象借用指针。 */
        [[nodiscard]] T *object() const noexcept {
            return static_cast<T *>(pin_.get());
        }

        [[nodiscard]] u64_t rights() const noexcept {
            return pin_.rights();
        }

        [[nodiscard]] u64_t badge() const noexcept {
            return pin_.badge();
        }

        /**
         * @brief 检查权限快照是否包含全部请求位。
         * @param required 调用操作所需的权限位集合。
         */
        [[nodiscard]] bool allows(u64_t required) const noexcept {
            return (rights() & required) == required;
        }

        /** @brief 提前释放对象 pin。 */
        void reset() noexcept {
            pin_.reset();
        }

    private:
        CPin pin_;
    };

    /**
     * @brief 管理扁平 CNode 目录、token lookup 和 Capability Deriv Tree。
     *
     * 每个 CSpace 拥有独立 cookie 和 256 项 CNode 目录。`mutation_lock_` 串行化目录、
     * generation 分配和跨节点 CDT 修改；节点锁保护具体 slot。公开操作按照 node ID 排序
     * 获取多个节点锁，避免 cross-CNode copy/mint 形成锁环。
     *
     * CSpace 拥有所有成功 attach 且尚未 detach 的 CNode。析构会删除这些节点，并释放其中
     * 的 capability 强引用。
     */
    class CSpace final : public TypedKObject<CSpace, ObjectType::CSPACE> {
    public:
        static constexpr ObjectType TYPE = ObjectType::CSPACE;
        /**
         * @brief 创建包含 root CNode 的 CSpace。
         * @return 成功时返回独占 CSpace 指针；cookie、对象或 CNode 分配失败时返回错误。
         */
        [[nodiscard]] static tay::expected<CSpace *, CError> create() noexcept;

        CSpace(const CSpace &)            = delete;
        CSpace &operator=(const CSpace &) = delete;
        CSpace(CSpace &&)                 = delete;
        CSpace &operator=(CSpace &&)      = delete;
        ~CSpace() noexcept;

        [[nodiscard]] u16_t cookie() const noexcept {
            return cookie_;
        }

        /**
         * @brief 将未归属的 CNode 接入首个空闲目录项并转移所有权。
         * @param node 调用方拥有的空闲或已初始化节点。
         * @return 成功时返回目录 index；节点已有 owner 或目录已满时返回错误。
         * @note 成功后 CSpace 负责销毁节点，直至调用方通过 `detach()` 取回所有权。
         */
        [[nodiscard]] tay::expected<u8_t, CError> attach(CNode &node) noexcept;

        /**
         * @brief 从目录移除空的普通 CNode 并把所有权返还调用方。
         * @param index 非 root 的目录 index。
         * @return 成功时返回独占节点指针；节点缺失、非空或状态不可变时返回错误。
         */
        [[nodiscard]] tay::expected<CNode *, CError> detach(u8_t index) noexcept;

        /**
         * @brief 为存活对象创建一项无父节点的 Capability。
         * @param object 目标对象；成功发布后 slot 为其持有一个强引用。
         * @param rights 初始权限位。
         * @param badge 初始 badge。
         * @param cnode_index 目标 CNode 目录 index。
         * @param dst_slot 零表示自动分配，否则请求指定 slot。
         * @return 可解析 token，或目录、slot、generation 和对象状态错误。
         */
        [[nodiscard]] tay::expected<CToken, CError> install(KObject &object, u64_t rights,
                                                            u64_t badge      = 0,
                                                            u8_t cnode_index = ROOT_CNODE_INDEX,
                                                            u16_t dst_slot   = 0) noexcept;

        /**
         * @brief 校验 token 并取得对象 kernel pin。
         * @param token 待解析的 CSpace-local token。
         * @param expected_type `NONE` 表示不检查类型，否则必须与 slot 类型相同。
         * @param required_rights 必须全部存在于 Capability 中的权限位。
         * @return 成功时返回带权限和 badge 快照的 pin；校验失败时不暴露对象指针。
         * @note pin 在节点锁内建立，因此删除 slot 后也不会使已返回的访问句柄失效。
         */
        [[nodiscard]] tay::expected<CPin, CError> resolve(
            CToken token, ObjectType expected_type = ObjectType::NONE,
            u64_t required_rights = 0) noexcept;

        /**
         * @brief 执行带编译期目标类型的 resolve。
         * @tparam T 期望的具体对象类型。
         * @param token 待解析 token。
         * @param required_rights 操作所需权限。
         */
        template <CTarget T>
        [[nodiscard]] tay::expected<CHandle<T>, CError> resolve(
            CToken token, u64_t required_rights = 0) noexcept {
            return CHandle<T>(TAY_TRY(resolve(token, T::TYPE, required_rights)));
        }

        /**
         * @brief 派生保留 src_cap badge 的 Capability 副本。
         * @param src_cap 具备 `RIGHT_COPY` 的源 token。
         * @param new_rights 零表示保留源权限，否则必须是源权限子集。
         * @param dst_cnode 目标 CNode index。
         * @param dst_slot 零表示自动分配，否则请求指定 slot。
         * @return 新 token；失败时 src_cap 和目标 free list 保持有效。
         */
        [[nodiscard]] tay::expected<CToken, CError> copy(CToken src_cap, u64_t new_rights = 0,
                                                         u8_t dst_cnode = ROOT_CNODE_INDEX,
                                                         u16_t dst_slot = 0) noexcept;

        /**
         * @brief 派生具有权限子集和新 badge 的 Capability。
         * @param src_cap 具备 `RIGHT_MINT` 的源 token。
         * @param new_rights 新 Capability 的权限，必须是源权限子集。
         * @param badge 新 badge。
         * @param dst_cnode 目标 CNode index。
         * @param dst_slot 零表示自动分配，否则请求指定 slot。
         * @return 新 token；请求扩权或源 token 无效时返回错误。
         */
        [[nodiscard]] tay::expected<CToken, CError> mint(CToken src_cap, u64_t new_rights,
                                                         u64_t badge    = 0,
                                                         u8_t dst_cnode = ROOT_CNODE_INDEX,
                                                         u16_t dst_slot = 0) noexcept;

        /**
         * @brief 删除单个 Capability，并把直接派生项提升到其父节点。
         * @note src_cap slot 先失效，随后在节点锁外释放对象强引用。
         */
        [[nodiscard]] tay::expected<void, CError> delete_cap(CToken token) noexcept;

        /**
         * @brief 后序删除 src_cap 的全部 CDT 后代，但保留 src_cap 自身。
         * @param token 具备 `RIGHT_REVOKE` 的 src_cap token。
         * @return 撤销成功或 token、权限、树一致性错误。
         */
        [[nodiscard]] tay::expected<void, CError> revoke(CToken token) noexcept;

    private:
        enum class DeriveBadge : u8_t {
            INHERIT,
            REPLACE,
        };

        explicit CSpace(u16_t cookie) noexcept;

        [[nodiscard]] tay::expected<CToken, CError> derive_cap(
            CToken src_cap, u64_t new_rights, u64_t required_right, bool zero_rights_inherit,
            DeriveBadge badge_policy, u64_t badge, u8_t dst_cnode, u16_t dst_slot) noexcept;

        /** @pre 调用方持有 `mutation_lock_`。 */
        [[nodiscard]] tay::expected<u32_t, CError> next_gen_locked(u8_t cnode_index) noexcept;

        /** @pre `index` 已由 token 解码或目录遍历保证在 8-bit 范围内。 */
        [[nodiscard]] CNode *node_at(u8_t index) const noexcept;

        /**
         * @brief 在目录中定位 CDT 节点所属的 CNode 和 slot。
         * @pre 调用方持有 `mutation_lock_`，保证目录与树节点地址稳定。
         */
        [[nodiscard]] bool find_cap(Capability *capability, CNode *&node, u16_t &slot) noexcept;

        tay::spinlock mutation_lock_{};
        std::atomic<CNode *> nodes_[MAX_CNODES]{};
        tay::counter<u32_t> generation_counters_[MAX_CNODES]{};
        u16_t cookie_ = 0;
    };
}  // namespace cap
