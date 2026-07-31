/**
 * @file tree.h
 * @brief Non-owning intrusive hierarchy tree.
 */

#pragma once

#include <tay/panic.h>
#include <tay/utility.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    template <class T>
    struct intrusive_tree_hook {
        T* parent = nullptr;
        T* first_child = nullptr;
        T* last_child = nullptr;
        T* previous_sibling = nullptr;
        T* next_sibling = nullptr;
    };

    namespace detail {
        struct intrusive_tree_locate_tag {};
    }

    template <class T, class Locate>
    class intrusive_tree
        : private composition<detail::intrusive_tree_locate_tag, Locate> {
        using hook_type = std::remove_reference_t<
            std::invoke_result_t<Locate&, T&>>;

        [[nodiscard]] constexpr Locate& locate() noexcept {
            return get<detail::intrusive_tree_locate_tag>(this);
        }
        [[nodiscard]] constexpr const Locate& locate() const noexcept {
            return get<detail::intrusive_tree_locate_tag>(this);
        }
        [[nodiscard]] constexpr hook_type& hook(T& node) noexcept {
            return locate()(node);
        }
        [[nodiscard]] constexpr const hook_type& hook(const T& node) const
            noexcept {
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
            T* current_ = nullptr;
            constexpr child_iterator(intrusive_tree* tree, T* current) noexcept
                : tree_(tree), current_(current) {}

        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = T*;
            using reference = T*;
            using pointer = T*;
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
                                             const child_iterator& right)
                noexcept {
                return left.tree_ == right.tree_ &&
                       left.current_ == right.current_;
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
            T* root_ = nullptr;
            T* current_ = nullptr;

            constexpr traversal_iterator(intrusive_tree* tree, T* root,
                                         T* current) noexcept
                : tree_(tree), root_(root), current_(current) {}

            [[nodiscard]] constexpr T* first_postorder(T* node) noexcept {
                while (tree_->hook(*node).first_child != nullptr) {
                    node = tree_->hook(*node).first_child;
                }
                return node;
            }

        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = T*;
            using reference = T*;
            using pointer = T*;
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
                        current_ = first_postorder(
                            tree_->hook(*current_).next_sibling);
                    } else {
                        current_ = tree_->hook(*current_).parent;
                    }
                } else {
                    if (tree_->hook(*current_).first_child != nullptr) {
                        current_ = tree_->hook(*current_).first_child;
                    } else {
                        while (current_ != root_ &&
                               tree_->hook(*current_).next_sibling == nullptr)
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
            friend constexpr bool operator==(
                const traversal_iterator& left,
                const traversal_iterator& right) noexcept {
                return left.tree_ == right.tree_ &&
                       left.current_ == right.current_;
            }
        };

        template <bool Postorder>
        struct traversal_range {
            traversal_iterator<Postorder> first;
            traversal_iterator<Postorder> last;
            [[nodiscard]] constexpr auto begin() const noexcept {
                return first;
            }
            [[nodiscard]] constexpr auto end() const noexcept { return last; }
        };

        constexpr intrusive_tree() noexcept
            requires std::is_nothrow_default_constructible_v<Locate>
        = default;
        constexpr explicit intrusive_tree(Locate locate) noexcept
            : composition<detail::intrusive_tree_locate_tag, Locate>(
                  std::move(locate)) {}

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
                                                 const T& descendant) const
            noexcept {
            const T* current = &descendant;
            while (current != nullptr) {
                if (current == &ancestor) {
                    return true;
                }
                current = hook(*current).parent;
            }
            return false;
        }
        [[nodiscard]] constexpr std::size_t depth(const T& node) const noexcept {
            std::size_t result = 0;
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
            std::size_t left_depth = depth(*left);
            std::size_t right_depth = depth(*right);
            while (left_depth > right_depth) {
                left = hook(*left).parent;
                --left_depth;
            }
            while (right_depth > left_depth) {
                right = hook(*right).parent;
                --right_depth;
            }
            while (left != right) {
                left = hook(*left).parent;
                right = hook(*right).parent;
            }
            return left;
        }

        constexpr void link_front(T& parent_node, T& child) {
            prepare_link(parent_node, child);
            auto& parent_hook = hook(parent_node);
            auto& child_hook = hook(child);
            child_hook.parent = &parent_node;
            child_hook.next_sibling = parent_hook.first_child;
            if (parent_hook.first_child != nullptr) {
                hook(*parent_hook.first_child).previous_sibling = &child;
            } else {
                parent_hook.last_child = &child;
            }
            parent_hook.first_child = &child;
        }
        constexpr void link_back(T& parent_node, T& child) {
            prepare_link(parent_node, child);
            auto& parent_hook = hook(parent_node);
            auto& child_hook = hook(child);
            child_hook.parent = &parent_node;
            child_hook.previous_sibling = parent_hook.last_child;
            if (parent_hook.last_child != nullptr) {
                hook(*parent_hook.last_child).next_sibling = &child;
            } else {
                parent_hook.first_child = &child;
            }
            parent_hook.last_child = &child;
        }
        constexpr void link_before(T& sibling, T& child) {
            T* parent_node = hook(sibling).parent;
            require(parent_node != nullptr,
                    "intrusive_tree sibling has no parent");
            prepare_link(*parent_node, child);
            auto& sibling_hook = hook(sibling);
            auto& child_hook = hook(child);
            child_hook.parent = parent_node;
            child_hook.previous_sibling = sibling_hook.previous_sibling;
            child_hook.next_sibling = &sibling;
            if (sibling_hook.previous_sibling != nullptr) {
                hook(*sibling_hook.previous_sibling).next_sibling = &child;
            } else {
                hook(*parent_node).first_child = &child;
            }
            sibling_hook.previous_sibling = &child;
        }
        constexpr void unlink(T& node) noexcept {
            auto& node_hook = hook(node);
            if (node_hook.parent == nullptr) {
                node_hook.previous_sibling = nullptr;
                node_hook.next_sibling = nullptr;
                return;
            }
            auto& parent_hook = hook(*node_hook.parent);
            if (node_hook.previous_sibling != nullptr) {
                hook(*node_hook.previous_sibling).next_sibling =
                    node_hook.next_sibling;
            } else {
                parent_hook.first_child = node_hook.next_sibling;
            }
            if (node_hook.next_sibling != nullptr) {
                hook(*node_hook.next_sibling).previous_sibling =
                    node_hook.previous_sibling;
            } else {
                parent_hook.last_child = node_hook.previous_sibling;
            }
            node_hook.parent = nullptr;
            node_hook.previous_sibling = nullptr;
            node_hook.next_sibling = nullptr;
        }
        constexpr void reparent(T& new_parent, T& node) {
            require(!is_ancestor(node, new_parent),
                    "intrusive_tree reparent would create a cycle");
            unlink(node);
            link_back(new_parent, node);
        }
        constexpr void clear_children(T& parent_node) noexcept {
            T* child = hook(parent_node).first_child;
            while (child != nullptr) {
                T* next = hook(*child).next_sibling;
                hook(*child).parent = nullptr;
                hook(*child).previous_sibling = nullptr;
                hook(*child).next_sibling = nullptr;
                child = next;
            }
            hook(parent_node).first_child = nullptr;
            hook(parent_node).last_child = nullptr;
        }

        [[nodiscard]] constexpr child_range children(T& node) noexcept {
            return {{this, hook(node).first_child}, {this, nullptr}};
        }
        [[nodiscard]] constexpr traversal_range<false> preorder(
            T& root) noexcept {
            return {{this, &root, &root}, {this, &root, nullptr}};
        }
        [[nodiscard]] constexpr traversal_range<true> postorder(
            T& root) noexcept {
            T* first = &root;
            while (hook(*first).first_child != nullptr) {
                first = hook(*first).first_child;
            }
            return {{this, &root, first}, {this, &root, nullptr}};
        }

        template <class F>
        constexpr void for_each_child(T& node, F&& function) {
            for (T* child = hook(node).first_child; child != nullptr;
                 child = hook(*child).next_sibling)
            {
                function(*child);
            }
        }
        template <class F>
        constexpr void for_each_preorder(T& node, F&& function) {
            function(node);
            for (T* child = hook(node).first_child; child != nullptr;
                 child = hook(*child).next_sibling)
            {
                for_each_preorder(*child, function);
            }
        }
        template <class F>
        constexpr void for_each_postorder(T& node, F&& function) {
            for (T* child = hook(node).first_child; child != nullptr;
                 child = hook(*child).next_sibling)
            {
                for_each_postorder(*child, function);
            }
            function(node);
        }

    private:
        constexpr void prepare_link(T& parent_node, T& child) {
            require(&parent_node != &child,
                    "intrusive_tree cannot link a node to itself");
            require(!linked(child),
                    "intrusive_tree child is already linked");
            require(!is_ancestor(child, parent_node),
                    "intrusive_tree link would create a cycle");
        }
    };
}  // namespace tay
