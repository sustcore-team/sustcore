/**
 * @file tree.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供非拥有式侵入层级树容器。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/panic.h>
#include <tay/utility.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    /**
     * @brief 缓存首尾子节点的非拥有式侵入树 hook。
     * @tparam T 包含该 hook 的节点类型。
     * @note hook 只保存借用指针；节点和树对象均不负责彼此的生命周期。
     */
    template <class T>
    struct intrusive_tree_hook {
        T* parent           = nullptr;
        T* first_child      = nullptr;
        T* last_child       = nullptr;
        T* previous_sibling = nullptr;
        T* next_sibling     = nullptr;
    };

    /**
     * @brief 不缓存尾子节点的紧凑侵入式树 hook。
     *
     * 该布局只保存父节点、首子节点和前后兄弟指针，适合需要把 hook 内嵌在固定大小
     * ABI 对象中的场景。对紧凑 hook 执行 `link_back()` 时会线性扫描兄弟链寻找尾节点；
     * 需要稳定 O(1) 尾插时应使用 `intrusive_tree_hook`。
     *
     * @tparam T 树节点类型。
     */
    template <class T>
    struct compact_intrusive_tree_hook {
        T* parent           = nullptr;
        T* first_child      = nullptr;
        T* previous_sibling = nullptr;
        T* next_sibling     = nullptr;
    };

    namespace detail {
        struct intrusive_tree_locate_tag {};
    }  // namespace detail

    /**
     * @brief 通过节点内嵌 hook 维护非拥有式多叉树。
     *
     * 容器本身不保存 root，也不分配或销毁节点。`Locate` 决定节点中 hook 的位置，因此同一
     * 节点类型可以参加多棵独立树。链接操作会检查重复链接和环；违反前置条件时调用
     * `tay::panic()`。
     *
     * `intrusive_tree_hook` 的尾插为 O(1)，`compact_intrusive_tree_hook` 的尾插为 O(k)，
     * 其中 k 是父节点当前直接子节点数；其余链接、移除和迭代操作语义相同。
     *
     * @tparam T 节点类型。
     * @tparam Locate 可调用的 hook locator，返回节点内嵌 hook 的引用。
     * @warning 节点析构前必须先从树中移除，并处理其全部直接子节点。
     */
    template <class T, class Locate>
    class intrusive_tree : private composition<detail::intrusive_tree_locate_tag, Locate> {
        using hook_type = std::remove_reference_t<std::invoke_result_t<Locate&, T&>>;

        [[nodiscard]] constexpr Locate& locate() noexcept {
            return get<detail::intrusive_tree_locate_tag>(this);
        }
        [[nodiscard]] constexpr const Locate& locate() const noexcept {
            return get<detail::intrusive_tree_locate_tag>(this);
        }
        [[nodiscard]] constexpr hook_type& hook(T& node) noexcept {
            return locate()(node);
        }
        [[nodiscard]] constexpr const hook_type& hook(const T& node) const noexcept {
            return locate()(node);
        }
        static constexpr void require(bool value, const char* message) {
            if (!value) {
                tay::panic(message);
            }
        }

    public:
        class child_iterator {
            friend class intrusive_tree;
            intrusive_tree* tree_ = nullptr;
            T* current_           = nullptr;
            constexpr child_iterator(intrusive_tree* tree, T* current) noexcept
                : tree_(tree), current_(current) {}

        public:
            using iterator_category             = std::forward_iterator_tag;
            using iterator_concept              = std::forward_iterator_tag;
            using difference_type               = std::ptrdiff_t;
            using value_type                    = T*;
            using reference                     = T*;
            using pointer                       = T*;
            constexpr child_iterator() noexcept = default;
            [[nodiscard]] constexpr T* operator*() const noexcept {
                return current_;
            }
            [[nodiscard]] constexpr T* operator->() const noexcept {
                return current_;
            }
            constexpr child_iterator& operator++() noexcept {
                current_ = tree_->hook(*current_).next_sibling;
                return *this;
            }
            friend constexpr bool operator==(const child_iterator& left,
                                             const child_iterator& right) noexcept {
                return left.tree_ == right.tree_ && left.current_ == right.current_;
            }
        };

        struct child_range {
            child_iterator first;
            child_iterator last;
            [[nodiscard]] constexpr child_iterator begin() const noexcept {
                return first;
            }
            [[nodiscard]] constexpr child_iterator end() const noexcept {
                return last;
            }
        };

        template <bool Postorder>
        class traversal_iterator {
            friend class intrusive_tree;
            intrusive_tree* tree_ = nullptr;
            T* root_              = nullptr;
            T* current_           = nullptr;

            constexpr traversal_iterator(intrusive_tree* tree, T* root, T* current) noexcept
                : tree_(tree), root_(root), current_(current) {}

            [[nodiscard]] constexpr T* first_postorder(T* node) noexcept {
                while (tree_->hook(*node).first_child != nullptr) {
                    node = tree_->hook(*node).first_child;
                }
                return node;
            }

        public:
            using iterator_category                 = std::forward_iterator_tag;
            using iterator_concept                  = std::forward_iterator_tag;
            using difference_type                   = std::ptrdiff_t;
            using value_type                        = T*;
            using reference                         = T*;
            using pointer                           = T*;
            constexpr traversal_iterator() noexcept = default;
            [[nodiscard]] constexpr T* operator*() const noexcept {
                return current_;
            }
            [[nodiscard]] constexpr T* operator->() const noexcept {
                return current_;
            }
            constexpr traversal_iterator& operator++() noexcept {
                if constexpr (Postorder) {
                    if (current_ == root_) {
                        current_ = nullptr;
                    } else if (tree_->hook(*current_).next_sibling != nullptr) {
                        current_ = first_postorder(tree_->hook(*current_).next_sibling);
                    } else {
                        current_ = tree_->hook(*current_).parent;
                    }
                } else {
                    if (tree_->hook(*current_).first_child != nullptr) {
                        current_ = tree_->hook(*current_).first_child;
                    } else {
                        while (current_ != root_ && tree_->hook(*current_).next_sibling == nullptr)
                        {
                            current_ = tree_->hook(*current_).parent;
                        }
                        if (current_ == root_) {
                            current_ = nullptr;
                        } else {
                            current_ = tree_->hook(*current_).next_sibling;
                        }
                    }
                }
                return *this;
            }
            friend constexpr bool operator==(const traversal_iterator& left,
                                             const traversal_iterator& right) noexcept {
                return left.tree_ == right.tree_ && left.current_ == right.current_;
            }
        };

        template <bool Postorder>
        struct traversal_range {
            traversal_iterator<Postorder> first;
            traversal_iterator<Postorder> last;
            [[nodiscard]] constexpr auto begin() const noexcept {
                return first;
            }
            [[nodiscard]] constexpr auto end() const noexcept {
                return last;
            }
        };

        constexpr intrusive_tree() noexcept
            requires std::is_nothrow_default_constructible_v<Locate>
        = default;
        constexpr explicit intrusive_tree(Locate locate) noexcept
            : composition<detail::intrusive_tree_locate_tag, Locate>(std::move(locate)) {}

        [[nodiscard]] constexpr bool linked(const T& node) const noexcept {
            const auto& h = hook(node);
            return h.parent != nullptr || h.previous_sibling != nullptr ||
                   h.next_sibling != nullptr;
        }
        [[nodiscard]] constexpr T* parent(T& node) noexcept {
            return hook(node).parent;
        }
        [[nodiscard]] constexpr const T* parent(const T& node) const noexcept {
            return hook(node).parent;
        }
        [[nodiscard]] constexpr bool is_root(const T& node) const noexcept {
            return hook(node).parent == nullptr;
        }
        [[nodiscard]] constexpr bool is_ancestor(const T& ancestor,
                                                 const T& descendant) const noexcept {
            const T* current = &descendant;
            while (current != nullptr) {
                if (current == &ancestor) {
                    return true;
                }
                current = hook(*current).parent;
            }
            return false;
        }
        [[nodiscard]] constexpr size_t depth(const T& node) const noexcept {
            size_t result    = 0;
            const T* current = hook(node).parent;
            while (current != nullptr) {
                ++result;
                current = hook(*current).parent;
            }
            return result;
        }
        [[nodiscard]] constexpr T* lca(T* left, T* right) noexcept {
            if (left == nullptr || right == nullptr) {
                return nullptr;
            }
            size_t left_depth  = depth(*left);
            size_t right_depth = depth(*right);
            while (left_depth > right_depth) {
                left = hook(*left).parent;
                --left_depth;
            }
            while (right_depth > left_depth) {
                right = hook(*right).parent;
                --right_depth;
            }
            while (left != right) {
                left  = hook(*left).parent;
                right = hook(*right).parent;
            }
            return left;
        }

        /**
         * @brief 把未链接节点插入父节点的直接子链表首部。
         * @pre parent 和 child 不是同一节点，child 尚未链接，操作不会形成环。
         */
        constexpr void link_front(T& parent_node, T& child) {
            prepare_link(parent_node, child);
            auto& parent_hook       = hook(parent_node);
            auto& child_hook        = hook(child);
            child_hook.parent       = &parent_node;
            child_hook.next_sibling = parent_hook.first_child;
            if (parent_hook.first_child != nullptr) {
                hook(*parent_hook.first_child).previous_sibling = &child;
            } else {
                set_last_child(parent_node, &child);
            }
            parent_hook.first_child = &child;
        }
        /**
         * @brief 把未链接节点插入父节点的直接子链表尾部。
         * @pre parent 和 child 不是同一节点，child 尚未链接，操作不会形成环。
         */
        constexpr void link_back(T& parent_node, T& child) {
            prepare_link(parent_node, child);
            auto& parent_hook           = hook(parent_node);
            auto& child_hook            = hook(child);
            child_hook.parent           = &parent_node;
            child_hook.previous_sibling = last_child(parent_node);
            if (child_hook.previous_sibling != nullptr) {
                hook(*child_hook.previous_sibling).next_sibling = &child;
            } else {
                parent_hook.first_child = &child;
            }
            set_last_child(parent_node, &child);
        }
        /**
         * @brief 把未链接节点插入指定 sibling 之前。
         * @pre sibling 已有父节点，child 尚未链接，操作不会形成环。
         */
        constexpr void link_before(T& sibling, T& child) {
            T* parent_node = hook(sibling).parent;
            require(parent_node != nullptr, "intrusive_tree sibling has no parent");
            prepare_link(*parent_node, child);
            auto& sibling_hook          = hook(sibling);
            auto& child_hook            = hook(child);
            child_hook.parent           = parent_node;
            child_hook.previous_sibling = sibling_hook.previous_sibling;
            child_hook.next_sibling     = &sibling;
            if (sibling_hook.previous_sibling != nullptr) {
                hook(*sibling_hook.previous_sibling).next_sibling = &child;
            } else {
                hook(*parent_node).first_child = &child;
            }
            sibling_hook.previous_sibling = &child;
        }
        /**
         * @brief 从父节点和兄弟链中移除节点。
         * @note 不修改 node 的子节点；移除后这些子节点仍以 node 为父节点。
         */
        constexpr void unlink(T& node) noexcept {
            auto& node_hook = hook(node);
            if (node_hook.parent == nullptr) {
                node_hook.previous_sibling = nullptr;
                node_hook.next_sibling     = nullptr;
                return;
            }
            auto& parent_hook = hook(*node_hook.parent);
            if (node_hook.previous_sibling != nullptr) {
                hook(*node_hook.previous_sibling).next_sibling = node_hook.next_sibling;
            } else {
                parent_hook.first_child = node_hook.next_sibling;
            }
            if (node_hook.next_sibling != nullptr) {
                hook(*node_hook.next_sibling).previous_sibling = node_hook.previous_sibling;
            } else {
                set_last_child(*node_hook.parent, node_hook.previous_sibling);
            }
            node_hook.parent           = nullptr;
            node_hook.previous_sibling = nullptr;
            node_hook.next_sibling     = nullptr;
        }
        /**
         * @brief 移除节点后将其追加为 new_parent 的直接子节点。
         * @pre new_parent 不在 node 的子树内。
         */
        constexpr void reparent(T& new_parent, T& node) {
            require(!is_ancestor(node, new_parent), "intrusive_tree reparent would create a cycle");
            unlink(node);
            link_back(new_parent, node);
        }
        /**
         * @brief 断开 parent 的全部直接子节点。
         * @note 每个子节点自己的子树保持不变。
         */
        constexpr void clear_children(T& parent_node) noexcept {
            T* child = hook(parent_node).first_child;
            while (child != nullptr) {
                T* next                       = hook(*child).next_sibling;
                hook(*child).parent           = nullptr;
                hook(*child).previous_sibling = nullptr;
                hook(*child).next_sibling     = nullptr;
                child                         = next;
            }
            hook(parent_node).first_child = nullptr;
            set_last_child(parent_node, nullptr);
        }

        /** @brief 返回按兄弟顺序访问直接子节点的非拥有式范围。 */
        [[nodiscard]] constexpr child_range children(T& node) noexcept {
            return {{this, hook(node).first_child}, {this, nullptr}};
        }
        /** @brief 返回从指定 root 开始的深度优先前序范围。 */
        [[nodiscard]] constexpr traversal_range<false> preorder(T& root) noexcept {
            return {{this, &root, &root}, {this, &root, nullptr}};
        }
        /** @brief 返回从指定 root 开始的深度优先后序范围。 */
        [[nodiscard]] constexpr traversal_range<true> postorder(T& root) noexcept {
            T* first = &root;
            while (hook(*first).first_child != nullptr) {
                first = hook(*first).first_child;
            }
            return {{this, &root, first}, {this, &root, nullptr}};
        }

        template <class F>
        constexpr void for_each_child(T& node, F&& function) {
            for (T* child = hook(node).first_child; child != nullptr;
                 child    = hook(*child).next_sibling)
            {
                function(*child);
            }
        }
        template <class F>
        constexpr void for_each_preorder(T& node, F&& function) {
            function(node);
            for (T* child = hook(node).first_child; child != nullptr;
                 child    = hook(*child).next_sibling)
            {
                for_each_preorder(*child, function);
            }
        }
        template <class F>
        constexpr void for_each_postorder(T& node, F&& function) {
            for (T* child = hook(node).first_child; child != nullptr;
                 child    = hook(*child).next_sibling)
            {
                for_each_postorder(*child, function);
            }
            function(node);
        }

    private:
        [[nodiscard]] constexpr T* last_child(T& node) noexcept {
            auto& node_hook = hook(node);
            if constexpr (requires { node_hook.last_child; }) {
                return node_hook.last_child;
            } else {
                T* child = node_hook.first_child;
                while (child != nullptr && hook(*child).next_sibling != nullptr)
                    child = hook(*child).next_sibling;
                return child;
            }
        }

        constexpr void set_last_child(T& node, T* child) noexcept {
            auto& node_hook = hook(node);
            if constexpr (requires { node_hook.last_child; })
                node_hook.last_child = child;
            else
                static_cast<void>(node_hook), static_cast<void>(child);
        }

        constexpr void prepare_link(T& parent_node, T& child) {
            require(&parent_node != &child, "intrusive_tree cannot link a node to itself");
            require(!linked(child), "intrusive_tree child is already linked");
            require(!is_ancestor(child, parent_node), "intrusive_tree link would create a cycle");
        }
    };
}  // namespace tay
