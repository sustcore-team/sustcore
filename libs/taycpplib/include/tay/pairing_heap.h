/**
 * @file pairing_heap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供非拥有式侵入 pairing heap。
 * @version 0.1.0-dev.1
 * @date 2026-08-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/panic.h>
#include <tay/utility.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace tay {
    /**
     * @brief 由调用方嵌入对象的 pairing heap hook。
     * @tparam T 包含该 hook 的对象类型。
     * @note hook 只保存借用指针，不拥有或延长任何对象的生命周期。
     */
    template <typename T>
    struct intrusive_pairing_heap_hook {
        T *owner         = nullptr;
        T *parent        = nullptr;
        T *first_child   = nullptr;
        T *previous      = nullptr;
        T *next          = nullptr;
        const void *heap = nullptr;
        bool linked      = false;
    };

    namespace detail {
        struct intrusive_pairing_heap_locate_tag {};
        struct intrusive_pairing_heap_compare_tag {};

        constexpr void intrusive_pairing_heap_require(bool condition,
                                                      const char *message) noexcept {
            if (!condition) {
                tay::panic(message);
            }
        }
    }  // namespace detail

    /**
     * @brief 非拥有、无分配且保持节点地址稳定的 intrusive pairing heap。
     *
     * `Compare(left, right)` 为真表示 `left` 应排在 `right` 前面，因此默认
     * `std::ranges::less` 构成 min-priority queue。`Locate` 将对象映射到其内嵌 hook；
     * 同一对象可借助多个独立 hook 参加多个容器。
     *
     * `push()` 和 `top()` 为 O(1)，`pop_min()` 与 `remove()` 为摊销 O(log n)，
     * `clear()` 为 O(n)。容器不移动、分配或销毁节点；节点在链接期间不得移动或析构，
     * 参与比较的 key 也不得以破坏堆序的方式修改。容器析构会执行 `clear()`，因此所有
     * 已链接节点必须至少存活到容器析构完成。
     *
     * @tparam T 节点对象类型。
     * @tparam Locate 返回对象内嵌 `intrusive_pairing_heap_hook<T>` 引用的 locator。
     * @tparam Compare 满足严格弱序且不抛异常的比较器。
     */
    template <typename T, typename Locate, typename Compare = std::ranges::less>
    class intrusive_pairing_heap
        : private composition<detail::intrusive_pairing_heap_locate_tag, Locate>,
          private composition<detail::intrusive_pairing_heap_compare_tag, Compare> {
    private:
        using locate_base  = composition<detail::intrusive_pairing_heap_locate_tag, Locate>;
        using compare_base = composition<detail::intrusive_pairing_heap_compare_tag, Compare>;

    public:
        using value_type   = T;
        using locate_type  = Locate;
        using compare_type = Compare;
        using hook_type    = intrusive_pairing_heap_hook<T>;

    private:
        using located_hook = std::remove_reference_t<std::invoke_result_t<Locate &, T &>>;

        static_assert(std::is_same_v<located_hook, hook_type>,
                      "intrusive_pairing_heap locator must return its hook type");
        static_assert(std::is_nothrow_invocable_r_v<hook_type &, Locate &, T &>,
                      "intrusive_pairing_heap locator must not throw");
        static_assert(std::is_nothrow_invocable_r_v<const hook_type &, const Locate &, const T &>,
                      "intrusive_pairing_heap const locator must not throw");
        static_assert(std::is_nothrow_invocable_r_v<bool, Compare &, const T &, const T &>,
                      "intrusive_pairing_heap comparator must not throw");

        [[nodiscard]] constexpr Locate &locate() noexcept {
            return get<detail::intrusive_pairing_heap_locate_tag>(this);
        }

        [[nodiscard]] constexpr const Locate &locate() const noexcept {
            return get<detail::intrusive_pairing_heap_locate_tag>(this);
        }

        [[nodiscard]] constexpr Compare &compare() noexcept {
            return get<detail::intrusive_pairing_heap_compare_tag>(this);
        }

        [[nodiscard]] constexpr hook_type &hook_of(T &node) noexcept {
            return locate()(node);
        }

        [[nodiscard]] constexpr const hook_type &hook_of(const T &node) const noexcept {
            return locate()(node);
        }

        static constexpr void require(bool condition, const char *message) noexcept {
            detail::intrusive_pairing_heap_require(condition, message);
        }

        constexpr void require_detached(const T &node) const noexcept {
            const auto &hook = hook_of(node);
            require(!hook.linked, "intrusive_pairing_heap element is already linked");
            require(hook.owner == nullptr, "intrusive_pairing_heap hook has a stale owner");
            require(hook.heap == nullptr, "intrusive_pairing_heap hook has a stale heap");
            require(hook.parent == nullptr, "intrusive_pairing_heap hook has a stale parent");
            require(hook.first_child == nullptr, "intrusive_pairing_heap hook has a stale child");
            require(hook.previous == nullptr,
                    "intrusive_pairing_heap hook has a stale previous link");
            require(hook.next == nullptr, "intrusive_pairing_heap hook has a stale next link");
        }

        constexpr void detach_root_links(T &node) noexcept {
            auto &hook    = hook_of(node);
            hook.parent   = nullptr;
            hook.previous = nullptr;
            hook.next     = nullptr;
        }

        [[nodiscard]] constexpr T *meld(T *left, T *right) noexcept {
            if (left == nullptr) {
                return right;
            }
            if (right == nullptr) {
                return left;
            }
            if (compare()(*right, *left)) {
                T *temporary = left;
                left         = right;
                right        = temporary;
            }

            auto &left_hook     = hook_of(*left);
            auto &right_hook    = hook_of(*right);
            right_hook.parent   = left;
            right_hook.previous = nullptr;
            right_hook.next     = left_hook.first_child;
            if (left_hook.first_child != nullptr) {
                hook_of(*left_hook.first_child).previous = right;
            }
            left_hook.first_child = right;
            return left;
        }

        [[nodiscard]] constexpr T *combine_children(T &node) noexcept {
            T *child                  = hook_of(node).first_child;
            hook_of(node).first_child = nullptr;
            T *pair_tail              = nullptr;

            while (child != nullptr) {
                T *first  = child;
                T *second = hook_of(*first).next;
                T *next   = second == nullptr ? nullptr : hook_of(*second).next;
                detach_root_links(*first);
                if (second != nullptr) {
                    detach_root_links(*second);
                }

                T *paired            = second == nullptr ? first : meld(first, second);
                auto &paired_hook    = hook_of(*paired);
                paired_hook.previous = pair_tail;
                paired_hook.next     = nullptr;
                if (pair_tail != nullptr) {
                    hook_of(*pair_tail).next = paired;
                }
                pair_tail = paired;
                child     = next;
            }

            T *result  = nullptr;
            T *current = pair_tail;
            while (current != nullptr) {
                T *previous = hook_of(*current).previous;
                detach_root_links(*current);
                result  = meld(current, result);
                current = previous;
            }
            return result;
        }

        constexpr void release(T &node) noexcept {
            auto &hook       = hook_of(node);
            hook.owner       = nullptr;
            hook.parent      = nullptr;
            hook.first_child = nullptr;
            hook.previous    = nullptr;
            hook.next        = nullptr;
            hook.heap        = nullptr;
            hook.linked      = false;
        }

    public:
        constexpr intrusive_pairing_heap() noexcept
            requires(std::is_nothrow_default_constructible_v<Locate> &&
                     std::is_nothrow_default_constructible_v<Compare>)
        = default;

        constexpr explicit intrusive_pairing_heap(
            Locate locate,
            Compare compare = Compare{}) noexcept(std::is_nothrow_move_constructible_v<Locate> &&
                                                  std::is_nothrow_move_constructible_v<Compare>)
            : locate_base(std::move(locate)), compare_base(std::move(compare)) {}

        intrusive_pairing_heap(const intrusive_pairing_heap &)            = delete;
        intrusive_pairing_heap &operator=(const intrusive_pairing_heap &) = delete;
        intrusive_pairing_heap(intrusive_pairing_heap &&)                 = delete;
        intrusive_pairing_heap &operator=(intrusive_pairing_heap &&)      = delete;

        constexpr ~intrusive_pairing_heap() noexcept {
            clear();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return root_ == nullptr;
        }

        [[nodiscard]] constexpr size_t size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr T *top() noexcept {
            return root_;
        }

        [[nodiscard]] constexpr const T *top() const noexcept {
            return root_;
        }

        [[nodiscard]] constexpr bool linked(const T &node) const noexcept {
            const auto &hook = hook_of(node);
            return hook.linked && hook.heap == this;
        }

        [[nodiscard]] constexpr bool linked(const hook_type &hook) const noexcept {
            return hook.linked && hook.heap == this;
        }

        constexpr void push(T &node) noexcept {
            require_detached(node);
            auto &hook  = hook_of(node);
            hook.owner  = &node;
            hook.heap   = this;
            hook.linked = true;
            root_       = meld(root_, &node);
            ++size_;
        }

        constexpr void push(T *node) noexcept {
            require(node != nullptr, "intrusive_pairing_heap cannot insert a null pointer");
            push(*node);
        }

        [[nodiscard]] constexpr T *pop_min() noexcept {
            require(root_ != nullptr, "intrusive_pairing_heap cannot pop from an empty heap");
            T *removed = root_;
            root_      = combine_children(*removed);
            if (root_ != nullptr) {
                detach_root_links(*root_);
            }
            release(*removed);
            --size_;
            return removed;
        }

        [[nodiscard]] constexpr T *remove(T &node) noexcept {
            auto &hook = hook_of(node);
            require(hook.linked, "intrusive_pairing_heap element is not linked");
            require(hook.heap == this, "intrusive_pairing_heap element belongs to another heap");
            require(hook.owner == &node, "intrusive_pairing_heap hook owner is inconsistent");
            if (&node == root_) {
                return pop_min();
            }

            T *parent = hook.parent;
            require(parent != nullptr, "intrusive_pairing_heap element has no parent");
            if (hook.previous == nullptr) {
                require(hook_of(*parent).first_child == &node,
                        "intrusive_pairing_heap parent has inconsistent children");
                hook_of(*parent).first_child = hook.next;
            } else {
                require(hook_of(*hook.previous).next == &node,
                        "intrusive_pairing_heap sibling links are inconsistent");
                hook_of(*hook.previous).next = hook.next;
            }
            if (hook.next != nullptr) {
                hook_of(*hook.next).previous = hook.previous;
            }

            detach_root_links(node);
            T *replacement = combine_children(node);
            root_          = meld(root_, replacement);
            release(node);
            --size_;
            return &node;
        }

        [[nodiscard]] constexpr T *remove(T *node) noexcept {
            require(node != nullptr, "intrusive_pairing_heap cannot remove a null pointer");
            return remove(*node);
        }

        [[nodiscard]] constexpr T *remove(hook_type &hook) noexcept {
            require(hook.linked, "intrusive_pairing_heap hook is not linked");
            require(hook.heap == this, "intrusive_pairing_heap hook belongs to another heap");
            require(hook.owner != nullptr, "intrusive_pairing_heap hook has no owner");
            require(&hook_of(*hook.owner) == &hook,
                    "intrusive_pairing_heap hook owner is inconsistent");
            return remove(*hook.owner);
        }

        constexpr void clear() noexcept {
            T *current = root_;
            while (current != nullptr) {
                auto &hook = hook_of(*current);
                if (hook.first_child != nullptr) {
                    current = hook.first_child;
                    continue;
                }

                T *next   = hook.next;
                T *parent = hook.parent;
                if (next == nullptr && parent != nullptr) {
                    hook_of(*parent).first_child = nullptr;
                }
                release(*current);
                current = next == nullptr ? parent : next;
            }
            root_ = nullptr;
            size_ = 0;
        }

    private:
        T *root_     = nullptr;
        size_t size_ = 0;
    };

    template <typename T, typename Locate, typename Compare = std::ranges::less>
    using intrusive_priority_queue = intrusive_pairing_heap<T, Locate, Compare>;
}  // namespace tay
