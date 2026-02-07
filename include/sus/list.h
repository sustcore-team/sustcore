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

#include <string.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <iterator>

namespace util {
    // 辅助函数, 获取成员指针的类型信息
    template <typename T, typename U>
    constexpr auto get_member_value_type(U T::*) -> U;

    template <typename T, typename U>
    constexpr auto get_member_node_type(U T::*) -> T;

    template <typename Node>
    struct ListHead {
        Node* prev;
        Node* next;
    };

    // 侵入式链表节点
    template <typename Node, auto Head>
    concept IntrusiveListNodeTrait = requires(Node* node) {
        {
            new Node()
        } -> std::same_as<Node*>;
        {
            node->*Head
        } -> std::same_as<ListHead<Node>&>;
    };

    template <typename Node, auto Head = &Node::list_head>
        requires IntrusiveListNodeTrait<Node, Head>
    class IntrusiveList;

    // 迭代器
    template <typename Node, auto Head = &Node::list_head>
    class IntrusiveListIterator {
    private:
        using NodeType = Node;

        NodeType* D_current;

        static constexpr ListHead<NodeType>& U_head(NodeType* node) noexcept {
            return node->*Head;
        }
        static constexpr const ListHead<NodeType> U_head(
            const NodeType* node) noexcept {
            return node->*Head;
        }

        static constexpr NodeType*& U_next(NodeType* node) noexcept {
            return U_head(node).next;
        }
        static constexpr const NodeType* U_next(const NodeType* node) noexcept {
            return U_head(node).next;
        }

        static constexpr NodeType*& U_prev(NodeType* node) noexcept {
            return U_head(node).prev;
        }
        static constexpr const NodeType* U_prev(const NodeType* node) noexcept {
            return U_head(node).prev;
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

        friend class IntrusiveList<Node, Head>;
    };

    // 常量迭代器
    template <typename Node, auto Head = &Node::list_head>
    class IntrusiveListConstIterator {
    private:
        using NodeType = Node;

        const NodeType* D_current;

        static constexpr ListHead<NodeType>& U_head(NodeType* node) noexcept {
            return node->*Head;
        }
        static constexpr const ListHead<NodeType> U_head(
            const NodeType* node) noexcept {
            return node->*Head;
        }

        static constexpr NodeType*& U_next(NodeType* node) noexcept {
            return U_head(node).next;
        }
        static constexpr const NodeType* U_next(const NodeType* node) noexcept {
            return U_head(node).next;
        }

        static constexpr NodeType*& U_prev(NodeType* node) noexcept {
            return U_head(node).prev;
        }
        static constexpr const NodeType* U_prev(const NodeType* node) noexcept {
            return U_head(node).prev;
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
            const IntrusiveListIterator<Node, Head>& it) noexcept
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

        friend class IntrusiveList<Node, Head>;
    };

    template <typename Node, auto Head>
        requires IntrusiveListNodeTrait<Node, Head>
    class IntrusiveList {
    public:
        using NodeType       = Node;
        using iterator       = IntrusiveListIterator<Node, Head>;
        using const_iterator = IntrusiveListConstIterator<Node, Head>;
        using size_type      = std::size_t;

    private:
        // 哨兵节点
        NodeType D_sentinel;
        size_type D_size;

        static constexpr ListHead<NodeType>& U_head(NodeType* node) noexcept {
            return node->*Head;
        }
        static constexpr const ListHead<NodeType> U_head(
            const NodeType* node) noexcept {
            return node->*Head;
        }

        static constexpr NodeType*& U_next(NodeType* node) noexcept {
            return U_head(node).next;
        }
        static constexpr const NodeType* U_next(const NodeType* node) noexcept {
            return U_head(node).next;
        }

        static constexpr NodeType*& U_prev(NodeType* node) noexcept {
            return U_head(node).prev;
        }
        static constexpr const NodeType* U_prev(const NodeType* node) noexcept {
            return U_head(node).prev;
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
        constexpr IntrusiveList() noexcept : D_size(0) {
            U_init_sentinel(D_sentinel);
        }

        constexpr ~IntrusiveList() noexcept {
            clear();
        }

        constexpr NodeType& sentinel() noexcept {
            return D_sentinel;
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
                U_next(&D_sentinel)         = U_next(&other.D_sentinel);
                U_prev(&D_sentinel)         = U_prev(&other.D_sentinel);
                U_prev(U_next(&D_sentinel)) = &D_sentinel;
                U_next(U_prev(&D_sentinel)) = &D_sentinel;
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
                    U_next(&D_sentinel)         = U_next(&D_sentinel);
                    U_prev(&D_sentinel)         = U_prev(&D_sentinel);
                    U_prev(U_next(&D_sentinel)) = &D_sentinel;
                    U_next(U_prev(&D_sentinel)) = &D_sentinel;
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
            return iterator(U_next(&D_sentinel));
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(U_next(&D_sentinel));
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(U_next(&D_sentinel));
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
            return *U_next(&D_sentinel);
        }
        constexpr const NodeType& front() const noexcept {
            return *U_next(&D_sentinel);
        }

        constexpr NodeType& back() noexcept {
            return *U_prev(&D_sentinel);
        }
        constexpr const NodeType& back() const noexcept {
            return *U_prev(&D_sentinel);
        }

        // insert & erase
        iterator insert(iterator pos, NodeType& node) noexcept {
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

        iterator erase(iterator pos) noexcept {
            NodeType* node = pos.D_current;
            NodeType* next = U_next(node);
            NodeType* prev = U_prev(node);

            U_link(prev, next);
            U_next(node) = U_prev(node) = nullptr;
            --D_size;
            return iterator(next);
        }

        void remove(NodeType& node) noexcept {
            // 在链表中查询
            for (auto it = begin(); it != end(); ++it) {
                if (&*it == &node) {
                    erase(it);
                    return;
                }
            }
        }

        // pop/push_front/back
        void push_front(NodeType& node) noexcept {
            insert(begin(), node);
        }

        void push_back(NodeType& node) noexcept {
            insert(end(), node);
        }

        void pop_front() noexcept {
            if (!empty()) {
                erase(begin());
            }
        }

        void pop_back() noexcept {
            if (!empty()) {
                iterator it = end();
                --it;
                erase(it);
            }
        }

        // clear
        void clear() noexcept {
            while (!empty()) {
                pop_front();
            }
        }

        bool contains(const NodeType& node) const noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (&*it == &node) {
                    return true;
                }
            }
            return false;
        }
    };

    // 普通的双向链表
    template <typename _Tp>
    class LinkedList;

    // 迭代器
    template <typename _Tp>
    class LinkedListIterator {
    public:
        using NodeType = typename LinkedList<_Tp>::Node;

    private:
        NodeType* D_current;

    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = _Tp;
        using difference_type   = std::ptrdiff_t;
        using pointer           = _Tp*;
        using reference         = _Tp&;

        constexpr LinkedListIterator() noexcept : D_current(nullptr) {}
        constexpr explicit LinkedListIterator(NodeType* node)
            : D_current(node) {}

        // 解引用
        constexpr reference operator*() noexcept {
            return D_current->data;
        }

        constexpr pointer operator->() noexcept {
            return &(D_current->data);
        }

        // 迭代操作
        constexpr LinkedListIterator& operator++() noexcept {
            D_current = D_current->next;
            return *this;
        }

        constexpr LinkedListIterator operator++(int) noexcept {
            LinkedListIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr LinkedListIterator& operator--() noexcept {
            D_current = D_current->prev;
            return *this;
        }

        constexpr LinkedListIterator operator--(int) noexcept {
            LinkedListIterator tmp = *this;
            --(*this);
            return tmp;
        }

        // 比较
        constexpr bool operator==(
            const LinkedListIterator& other) const noexcept {
            return D_current == other.D_current;
        }

        constexpr bool operator!=(
            const LinkedListIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class LinkedList<_Tp>;
    };

    template <typename _Tp>
    class LinkedListConstIterator {
    public:
        using NodeType = typename LinkedList<_Tp>::Node;

    private:
        NodeType* D_current;

    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = _Tp;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const _Tp*;
        using reference         = const _Tp&;

        constexpr LinkedListConstIterator() noexcept : D_current(nullptr) {}
        constexpr explicit LinkedListConstIterator(const NodeType* node)
            : D_current(node) {}
        constexpr LinkedListConstIterator(
            const LinkedListIterator<_Tp>& it) noexcept
            : D_current(it.operator->()) {}

        // 解引用
        constexpr reference operator*() const noexcept {
            return D_current->data;
        }

        constexpr pointer operator->() const noexcept {
            return &(D_current->data);
        }

        // 迭代操作
        constexpr LinkedListConstIterator& operator++() noexcept {
            D_current = D_current->next;
            return *this;
        }

        constexpr LinkedListConstIterator operator++(int) noexcept {
            LinkedListConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr LinkedListConstIterator& operator--() noexcept {
            D_current = D_current->prev;
            return *this;
        }

        constexpr LinkedListConstIterator operator--(int) noexcept {
            LinkedListConstIterator tmp = *this;
            --(*this);
            return tmp;
        }

        // 比较
        constexpr bool operator==(
            const LinkedListConstIterator& other) const noexcept {
            return D_current == other.D_current;
        }

        constexpr bool operator!=(
            const LinkedListConstIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class LinkedList<_Tp>;
    };

    template <typename _Tp>
    class LinkedList {
    public:
        struct Node {
            _Tp data;
            Node* prev;
            Node* next;

            Node(const _Tp& value)
                : data(value), prev(nullptr), next(nullptr) {}
        };
        using NodeType       = Node;
        using iterator       = LinkedListIterator<_Tp>;
        using const_iterator = LinkedListConstIterator<_Tp>;
        using size_type      = std::size_t;

    private:
        NodeType* D_head;
        NodeType* D_tail;
        size_type D_size;
        static constexpr bool P_link(const NodeType* node) noexcept {
            return node->next != nullptr || node->prev != nullptr;
        }

        static constexpr void U_link(NodeType* prev, NodeType* next) noexcept {
            if (prev)
                prev->next = next;
            if (next)
                next->prev = prev;
        }

    public:
        constexpr LinkedList() : D_head(nullptr), D_tail(nullptr), D_size(0) {}
        constexpr ~LinkedList() {
            clear();
        }

        // 浅拷贝
        LinkedList(const LinkedList& other) {
            D_head = D_tail = nullptr;
            D_size          = 0;
            for (const auto& value : other) {
                push_back(value);
            }
        }
        LinkedList& operator=(const LinkedList& other) {
            if (this != &other) {
                clear();
                for (const auto& value : other) {
                    push_back(value);
                }
            }
            return *this;
        }

        // 移动语义
        constexpr LinkedList(LinkedList&& other) noexcept
            : D_head(other.D_head), D_tail(other.D_tail), D_size(other.D_size) {
            other.D_head = other.D_tail = nullptr;
            other.D_size                = 0;
        }

        constexpr LinkedList& operator=(LinkedList&& other) noexcept {
            if (this != &other) {
                clear();
                D_head       = other.D_head;
                D_tail       = other.D_tail;
                D_size       = other.D_size;
                other.D_head = other.D_tail = nullptr;
                other.D_size                = 0;
            }
            return *this;
        }

        constexpr size_type size() const noexcept {
            return D_size;
        }

        constexpr bool empty() const noexcept {
            return D_size == 0;
        }

        // iterators
        constexpr iterator begin() noexcept {
            return iterator(D_head);
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(D_head);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(D_head);
        }

        constexpr iterator end() noexcept {
            return iterator(nullptr);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(nullptr);
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(nullptr);
        }

        // 访问元素
        constexpr _Tp& front() noexcept {
            return D_head->data;
        }

        constexpr const _Tp& front() const noexcept {
            return D_head->data;
        }

        constexpr _Tp& back() noexcept {
            return D_tail->data;
        }

        constexpr const _Tp& back() const noexcept {
            return D_tail->data;
        }

        // insert & erase
        iterator insert(iterator pos, const _Tp& value) noexcept {
            NodeType* new_node = new Node(value);
            if (new_node == nullptr) {
                return end();
            }
            NodeType* next = pos.D_current;
            NodeType* prev = (next != nullptr) ? next->prev : D_tail;

            U_link(prev, new_node);
            U_link(new_node, next);

            if (prev == nullptr) {
                D_head = new_node;
            }
            if (next == nullptr) {
                D_tail = new_node;
            }

            ++D_size;
            return iterator(new_node);
        }

        iterator erase(iterator pos) noexcept {
            NodeType* node = pos.D_current;
            NodeType* next = node->next;
            NodeType* prev = node->prev;

            U_link(prev, next);
            if (node == D_head) {
                D_head = next;
            }
            if (node == D_tail) {
                D_tail = prev;
            }
            delete node;
            --D_size;
            return iterator(next);
        }

        void remove(const _Tp& value) noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == value) {
                    erase(it);
                    return;
                }
            }
        }

        void push_front(const _Tp& value) noexcept {
            insert(begin(), value);
        }

        void push_back(const _Tp& value) noexcept {
            insert(end(), value);
        }

        void pop_front() noexcept {
            if (!empty()) {
                erase(begin());
            }
        }

        void pop_back() noexcept {
            if (!empty()) {
                iterator it = end();
                --it;
                erase(it);
            }
        }

        // clear
        void clear() {
            while (!empty()) {
                pop_front();
            }
        }

        bool contains(const _Tp& value) const noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == value) {
                    return true;
                }
            }
            return false;
        }
    };

    template <typename _Tp>
    class ArrayList;

    template <typename _Tp>
    class ArrayListIterator {
    public:
        using IndexType = typename ArrayList<_Tp>::IndexType;

    private:
        ArrayList<_Tp>& D_list;
        IndexType D_index;

    public:
        // iterator traits
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = _Tp;
        using difference_type   = std::ptrdiff_t;
        using pointer           = _Tp*;
        using reference         = _Tp&;

        constexpr ArrayListIterator(ArrayList<_Tp>& array_list,
                                    IndexType index) noexcept
            : D_list(array_list), D_index(index) {}
        // 解引用
        constexpr reference operator*() noexcept {
            return D_list.at(D_index);
        }
        constexpr pointer operator->() noexcept {
            return &D_list.at(D_index);
        }
        // 迭代操作
        constexpr ArrayListIterator& operator++() noexcept {
            ++D_index;
            return *this;
        }
        constexpr ArrayListIterator operator++(int) noexcept {
            ArrayListIterator tmp = *this;
            ++(*this);
            return tmp;
        }
        constexpr ArrayListIterator& operator--() noexcept {
            --D_index;
            return *this;
        }
        constexpr ArrayListIterator operator--(int) noexcept {
            ArrayListIterator tmp = *this;
            --(*this);
            return tmp;
        }
        // 比较
        constexpr bool operator==(
            const ArrayListIterator& other) const noexcept {
            return D_index == other.D_index;
        }
        constexpr bool operator!=(
            const ArrayListIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class ArrayList<_Tp>;
    };

    template <typename _Tp>
    class ArrayListConstIterator {
    public:
        using IndexType = typename ArrayList<_Tp>::IndexType;

    private:
        const ArrayList<_Tp>& D_list;
        IndexType D_index;

    public:
        // iterator traits
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = _Tp;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const _Tp*;
        using reference         = const _Tp&;

        constexpr ArrayListConstIterator(const ArrayList<_Tp>& array_list,
                                         IndexType index) noexcept
            : D_list(array_list), D_index(index) {}
        constexpr ArrayListConstIterator(
            const ArrayListIterator<_Tp>& it) noexcept
            : D_list(it.D_list), D_index(it.D_index) {}

        // 解引用
        constexpr reference operator*() const noexcept {
            return D_list.at(D_index);
        }
        constexpr pointer operator->() const noexcept {
            return &D_list.at(D_index);
        }
        // 迭代操作
        constexpr ArrayListConstIterator& operator++() const noexcept {
            ++D_index;
            return *this;
        }
        constexpr ArrayListConstIterator operator++(int) noexcept {
            ArrayListConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }
        constexpr ArrayListConstIterator& operator--() noexcept {
            --D_index;
            return *this;
        }
        constexpr ArrayListConstIterator operator--(int) noexcept {
            ArrayListConstIterator tmp = *this;
            --(*this);
            return tmp;
        }
        // 比较
        constexpr bool operator==(
            const ArrayListConstIterator& other) const noexcept {
            return D_index == other.D_index;
        }
        constexpr bool operator!=(
            const ArrayListConstIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class ArrayList<_Tp>;
    };

    /**
     * @brief 数组形式的动态列表，支持随机访问和动态扩容
     *
     * @tparam _Tp 对象结构。其应当是一个POD类型
     */
    template <typename _Tp>
    class ArrayList {
    public:
        using IndexType      = std::size_t;
        using iterator       = ArrayListIterator<_Tp>;
        using const_iterator = ArrayListConstIterator<_Tp>;
        using size_type      = std::size_t;

    private:
        _Tp* D_data;
        size_type D_size;
        size_type D_capacity;

    public:
        constexpr ArrayList(size_type capacity = 8)
            : D_data(new _Tp[capacity]), D_size(0), D_capacity(capacity) {}
        constexpr ~ArrayList() {
            if (D_data != nullptr) {
                delete[] D_data;
            }
        }

        // 拷贝
        ArrayList(const ArrayList& other) {
            D_data     = new _Tp[other.D_capacity];
            D_size     = other.D_size;
            D_capacity = other.D_capacity;
            memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
        }
        ArrayList& operator=(const ArrayList& other) {
            if (this != &other) {
                _Tp* new_space = new _Tp[other.D_capacity];
                if (new_space == nullptr) {
                    return *this;
                }
                if (D_data != nullptr) {
                    delete[] D_data;
                }
                D_data     = new_space;
                D_size     = other.D_size;
                D_capacity = other.D_capacity;
                memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
            }
            return *this;
        }

        // 移动语义
        constexpr ArrayList(ArrayList&& other) noexcept
            : D_data(other.D_data),
              D_size(other.D_size),
              D_capacity(other.D_capacity) {
            other.D_data     = nullptr;
            other.D_size     = 0;
            other.D_capacity = 0;
        }

        constexpr ArrayList& operator=(ArrayList&& other) noexcept {
            if (this != &other) {
                if (D_data != nullptr) {
                    delete[] D_data;
                }
                D_data           = other.D_data;
                D_size           = other.D_size;
                D_capacity       = other.D_capacity;
                other.D_data     = nullptr;
                other.D_size     = 0;
                other.D_capacity = 0;
            }
            return *this;
        }

        // capacity
        constexpr size_type size() const noexcept {
            return D_size;
        }
        constexpr bool empty() const noexcept {
            return D_size == 0;
        }
        constexpr size_type capacity() const noexcept {
            return D_capacity;
        }

    protected:
        void U_resize(size_type new_capacity) {
            _Tp* new_data      = new _Tp[new_capacity];
            size_t cp_capacity = std::min(new_capacity, D_capacity);
            memcpy(new_data, D_data, cp_capacity * sizeof(_Tp));
            delete[] D_data;
            D_data     = new_data;
            D_capacity = new_capacity;
        }

    public:
        void reserve(size_type new_capacity) {
            if (new_capacity > D_capacity) {
                U_resize(new_capacity);
            }
        }
        void shrink_to_fit() {
            if (D_size < D_capacity) {
                U_resize(D_size);
            }
        }

        // iterators
        constexpr iterator begin() noexcept {
            return iterator(*this, 0);
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(*this, 0);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(*this, 0);
        }

        constexpr iterator end() noexcept {
            return iterator(*this, D_size);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(*this, D_size);
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(*this, D_size);
        }

        // 访问元素
        constexpr _Tp& at(IndexType index) noexcept {
            return D_data[index];
        }
        constexpr const _Tp& at(IndexType index) const noexcept {
            return D_data[index];
        }

        // insert & erase
        iterator insert(iterator pos, const _Tp& value) noexcept {
            IndexType index = pos.D_index;
            if (D_size >= D_capacity) {
                U_resize(D_capacity * 2);
            }
            // 移动
            for (IndexType i = D_size; i > index; --i) {
                D_data[i] = D_data[i - 1];
            }
            D_data[index] = value;
            ++D_size;
            return iterator(*this, index);
        }

        // erase
        iterator erase(iterator pos) noexcept {
            IndexType index = pos.D_index;
            // 移动
            for (IndexType i = index; i < D_size - 1; ++i) {
                D_data[i] = D_data[i + 1];
            }
            --D_size;
            return iterator(*this, index);
        }

        void remove(const _Tp& value) noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == value) {
                    erase(it);
                    return;
                }
            }
        }

        // pop/push_front/back
        void push_front(const _Tp& value) noexcept {
            insert(begin(), value);
        }
        void push_back(const _Tp& value) noexcept {
            insert(end(), value);
        }
        void pop_front() noexcept {
            if (!empty()) {
                erase(begin());
            }
        }
        void pop_back() noexcept {
            if (!empty()) {
                iterator it = end();
                --it;
                erase(it);
            }
        }

        // clear
        void clear() noexcept {
            D_size = 0;
        }

        bool contains(const _Tp& value) const noexcept {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == value) {
                    return true;
                }
            }
            return false;
        }
    };
}  // namespace util