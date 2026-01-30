/**
 * @file list.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 侵入式链表
 * @version alpha-1.0.0
 * @date 2026-01-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>

namespace util {
    // 辅助函数, 获取成员指针的类型信息
    template <typename T, typename U>
    constexpr auto get_member_value_type(U T::*) -> U;

    template <typename T, typename U>
    constexpr auto get_member_node_type(U T::*) -> T;

    // 侵入式链表节点
    template <typename Node, auto NextPtr = &Node::next,
              auto PrevPtr = &Node::prev>
    concept IntrusiveListNodeTrait =
        std::is_member_pointer_v<decltype(NextPtr)> &&
        std::is_member_pointer_v<decltype(PrevPtr)> &&
        std::is_same_v<decltype(get_member_value_type(NextPtr)),
                       decltype(get_member_value_type(PrevPtr))> &&
        std::is_same_v<decltype(get_member_node_type(NextPtr)),
                       decltype(get_member_node_type(PrevPtr))> &&
        std::is_pointer_v<decltype(get_member_value_type(NextPtr))> &&
        std::is_same_v<decltype(get_member_value_type(NextPtr)), Node*>&&
            requires()
    {
        {
            new Node()
        } -> std::same_as<Node*>;
    };

    template <typename Node, auto NextPtr = &Node::next,
              auto PrevPtr = &Node::prev>
        requires IntrusiveListNodeTrait<Node, NextPtr, PrevPtr>
    class IntrusiveList;

    // 迭代器
    template <typename Node, auto NextPtr = &Node::next,
              auto PrevPtr = &Node::prev>
    class IntrusiveListIterator {
    private:
        using NodeType          = Node;
        using NextMemberPtrType = decltype(NextPtr);
        using PrevMemberPtrType = decltype(PrevPtr);

        NodeType* D_current;

        static constexpr NodeType*& U_next(NodeType* node) noexcept {
            return node->*NextPtr;
        }

        static constexpr NodeType*& U_prev(NodeType* node) noexcept {
            return node->*PrevPtr;
        }

    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = NodeType;
        using difference_type   = std::ptrdiff_t;
        using pointer           = NodeType*;
        using reference         = NodeType&;

        constexpr IntrusiveListIterator() noexcept : D_current(nullptr) {}
        constexpr explicit IntrusiveListIterator(NodeType* node)
            : D_current(node) {}

        // 解引用
        constexpr reference operator*() const noexcept {
            return *D_current;
        }

        constexpr pointer operator->() const noexcept {
            return D_current;
        }

        // 迭代操作
        constexpr IntrusiveListIterator& operator++() noexcept {
            D_current = U_next(D_current);
            return *this;
        }

        constexpr IntrusiveListIterator operator++(int) noexcept {
            IntrusiveListIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr IntrusiveListIterator& operator--() noexcept {
            D_current = U_prev(D_current);
            return *this;
        }

        constexpr IntrusiveListIterator operator--(int) noexcept {
            IntrusiveListIterator tmp = *this;
            --(*this);
            return tmp;
        }

        // 比较
        constexpr bool operator==(
            const IntrusiveListIterator& other) const noexcept {
            return D_current == other.D_current;
        }

        constexpr bool operator!=(
            const IntrusiveListIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class IntrusiveList<Node, NextPtr, PrevPtr>;
    };

    // 常量迭代器
    template <typename Node, auto NextPtr = &Node::next,
              auto PrevPtr = &Node::prev>
    class IntrusiveListConstIterator {
    private:
        using NodeType          = Node;
        using NextMemberPtrType = decltype(NextPtr);
        using PrevMemberPtrType = decltype(PrevPtr);

        const NodeType* D_current;

        static constexpr const NodeType* U_next(const NodeType* node) noexcept {
            return node->*NextPtr;
        }

        static constexpr const NodeType* U_prev(const NodeType* node) noexcept {
            return node->*PrevPtr;
        }

    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = NodeType;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const NodeType*;
        using reference         = const NodeType&;

        constexpr IntrusiveListConstIterator() noexcept : D_current(nullptr) {}
        constexpr explicit IntrusiveListConstIterator(const NodeType* node)
            : D_current(node) {}
        constexpr IntrusiveListConstIterator(
            const IntrusiveListIterator<Node, NextPtr, PrevPtr>& it) noexcept
            : D_current(it.operator->()) {}

        // 解引用
        constexpr reference operator*() const noexcept {
            return *D_current;
        }
        constexpr pointer operator->() const noexcept {
            return D_current;
        }

        // 迭代操作
        constexpr IntrusiveListConstIterator& operator++() noexcept {
            D_current = U_next(D_current);
            return *this;
        }

        constexpr IntrusiveListConstIterator operator++(int) noexcept {
            IntrusiveListConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr IntrusiveListConstIterator& operator--() noexcept {
            D_current = U_prev(D_current);
            return *this;
        }

        constexpr IntrusiveListConstIterator operator--(int) noexcept {
            IntrusiveListConstIterator tmp = *this;
            --(*this);
            return tmp;
        }

        // 比较
        constexpr bool operator==(
            const IntrusiveListConstIterator& other) const noexcept {
            return D_current == other.D_current;
        }

        constexpr bool operator!=(
            const IntrusiveListConstIterator& other) const noexcept {
            return !(*this == other);
        }
    };

    template <typename Node, auto NextPtr, auto PrevPtr>
        requires IntrusiveListNodeTrait<Node, NextPtr, PrevPtr>
    class IntrusiveList {
    private:
        using NodeType          = Node;
        using NextMemberPtrType = decltype(NextPtr);
        using PrevMemberPtrType = decltype(PrevPtr);

        // 哨兵节点
        NodeType D_sentinel;

        std::size_t D_size;

        static constexpr NodeType*& U_next(NodeType* node) noexcept {
            return node->*NextPtr;
        }

        static constexpr NodeType*& U_prev(NodeType* node) noexcept {
            return node->*PrevPtr;
        }

        static constexpr void U_link(NodeType* prev, NodeType* next) noexcept {
            if (prev)
                U_next(prev) = next;
            if (next)
                U_prev(next) = prev;
        }

        static constexpr void U_init_sentinel(NodeType& sentinel) noexcept {
            // 环形哨兵节点
            U_prev(&sentinel) = U_next(&sentinel) = &sentinel;
        }

        static constexpr bool P_link(const NodeType* node) noexcept {
            return U_next(const_cast<NodeType*>(node)) != nullptr ||
                   U_prev(const_cast<NodeType*>(node)) != nullptr;
        }

    public:
        using iterator = IntrusiveListIterator<Node, NextPtr, PrevPtr>;
        using const_iterator =
            IntrusiveListConstIterator<Node, NextPtr, PrevPtr>;
        using size_type = std::size_t;

        constexpr IntrusiveList() noexcept : D_size(0) {
            U_init_sentinel(D_sentinel);
        }

        constexpr ~IntrusiveList() noexcept {
            clear();
        }

        NodeType &sentinel() {
            return D_sentinel;
        }

        std::size_t size() {
            return D_size;
        }

        // 禁止浅拷贝
        IntrusiveList(const IntrusiveList&)            = delete;
        IntrusiveList& operator=(const IntrusiveList&) = delete;

        // 移动语义
        constexpr IntrusiveList(IntrusiveList&& other) noexcept
            : D_size(other.D_size) {
            if (other.empty()) {
                U_init_sentinel(D_sentinel);
            } else {
                D_sentinel.next         = other.D_sentinel.next;
                D_sentinel.prev         = other.D_sentinel.prev;
                U_prev(D_sentinel.next) = &D_sentinel;
                U_next(D_sentinel.prev) = &D_sentinel;
                U_init_sentinel(other.D_sentinel);
                other.D_size = 0;
            }
        }

        constexpr IntrusiveList& operator=(IntrusiveList&& other) noexcept {
            if (this != &other) {
                clear();
                D_size = other.D_size;
                if (other.empty()) {
                    U_init_sentinel(D_sentinel);
                } else {
                    D_sentinel.next         = other.D_sentinel.next;
                    D_sentinel.prev         = other.D_sentinel.prev;
                    U_prev(D_sentinel.next) = &D_sentinel;
                    U_next(D_sentinel.prev) = &D_sentinel;
                    U_init_sentinel(other.D_sentinel);
                    other.D_size = 0;
                }
            }
            return *this;
        }

        // capacity
        constexpr bool empty() const noexcept {
            return D_size == 0;
        }
        constexpr size_type size() const noexcept {
            return D_size;
        }

        // iterators
        constexpr iterator begin() noexcept {
            return iterator(D_sentinel.next);
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(D_sentinel.next);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(D_sentinel.next);
        }
        constexpr iterator end() noexcept {
            return iterator(&D_sentinel);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(&D_sentinel);
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(&D_sentinel);
        }

        // 访问元素
        constexpr NodeType& front() noexcept {
            return *D_sentinel.next;
        }
        constexpr const NodeType& front() const noexcept {
            return *D_sentinel.next;
        }

        constexpr NodeType& back() noexcept {
            return *D_sentinel.prev;
        }
        constexpr const NodeType& back() const noexcept {
            return *D_sentinel.prev;
        }

        // insert & erase
        constexpr iterator insert(iterator pos, NodeType& node) noexcept {
            // 节点已在链表中，无法插入
            if (P_link(&node)) {
                return end();
            }
            NodeType* next = pos.D_current;
            NodeType* prev = U_prev(next);

            U_link(prev, &node);
            U_link(&node, next);

            ++D_size;
            return iterator(&node);
        }

        constexpr iterator erase(iterator pos) noexcept {
            NodeType* node = pos.D_current;
            NodeType* next = U_next(node);
            NodeType* prev = U_prev(node);

            U_link(prev, next);
            U_next(node) = U_prev(node) = nullptr;
            --D_size;
            return iterator(next);
        }

        constexpr void remove(NodeType& node) noexcept {
            // 在链表中查询
            for (auto it = begin(); it != end(); ++it) {
                if (&*it == &node) {
                    erase(it);
                    return;
                }
            }
        }

        // pop/push_front/back
        constexpr void push_front(NodeType& node) noexcept {
            insert(begin(), node);
        }

        constexpr void push_back(NodeType& node) noexcept {
            insert(end(), node);
        }

        constexpr void pop_front() noexcept {
            if (!empty()) {
                erase(begin());
            }
        }

        constexpr void pop_back() noexcept {
            if (!empty()) {
                iterator it = end();
                --it;
                erase(it);
            }
        }

        // clear
        constexpr void clear() noexcept {
            while (!empty()) {
                pop_front();
            }
        }

        constexpr bool contains(const NodeType& node) const noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (&*it == &node) {
                    return true;
                }
            }
            return false;
        }
    };
}  // namespace util