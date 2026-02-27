/**
 * @file queue.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 队列
 * @version alpha-1.0.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>
#include <utility>

namespace util {
    template <typename _Tp, size_t D_capacity>
    class StaticArrayQueue {
    public:
        using IndexType      = std::size_t;
        // using iterator       = ;
        // using const_iterator = ;
        using size_type      = std::size_t;

    private:
        // 环形队列
        alignas(_Tp) _Tp D_data[D_capacity];
        IndexType D_head;
        IndexType D_tail;

        constexpr IndexType U_next(IndexType idx) {
            return (idx + 1) % D_capacity;
        }

    public:
        constexpr StaticArrayQueue()
            : D_head(0), D_tail(0) {}
        ~StaticArrayQueue() {
        }

        // 拷贝
        StaticArrayQueue(const StaticArrayQueue& other) {
            D_head     = other.D_head;
            D_tail     = other.D_tail;
            memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
        }
        StaticArrayQueue& operator=(const StaticArrayQueue& other) {
            if (this != &other) {
                D_head     = other.D_head;
                D_tail     = other.D_tail;
                memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
            }
            return *this;
        }

        // 移动语义
        constexpr StaticArrayQueue(StaticArrayQueue&& other) noexcept
            : D_head(other.D_head),
              D_tail(other.D_tail) {
            memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
            other.D_head     = 0;
            other.D_tail     = 0;
        }

        constexpr StaticArrayQueue& operator=(StaticArrayQueue&& other) noexcept {
            if (this != &other) {
                memcpy(D_data, other.D_data, D_capacity * sizeof(_Tp));
                D_head           = other.D_head;
                D_tail           = other.D_tail;
                other.D_head     = 0;
                other.D_tail     = 0;
            }
            return *this;
        }

        // capacity
        constexpr size_type size() const noexcept {
            return D_tail >= D_head ? D_tail - D_head : D_capacity - (D_head - D_tail);
        }
        constexpr bool empty() const noexcept {
            return D_tail == D_head;
        }
        constexpr bool full() const noexcept {
            return size() == D_capacity - 1;
        }
        constexpr size_type capacity() const noexcept {
            return D_capacity;
        }
    public:
        // iterators
        // constexpr iterator begin() noexcept {
        //     return iterator(*this, 0);
        // }
        // constexpr const_iterator begin() const noexcept {
        //     return const_iterator(*this, 0);
        // }
        // constexpr const_iterator cbegin() const noexcept {
        //     return const_iterator(*this, 0);
        // }

        // constexpr iterator end() noexcept {
        //     return iterator(*this, D_size);
        // }
        // constexpr const_iterator end() const noexcept {
        //     return const_iterator(*this, D_size);
        // }
        // constexpr const_iterator cend() const noexcept {
        //     return const_iterator(*this, D_size);
        // }

        // emplace
        template <typename... Args>
        bool emplace(Args&&... args) noexcept {
            if (full()) {
                // in fact, we should throw an exception here
                // however, we don't have exception handling mechanism in Sustcore
                return false;
            }

            const IndexType last_idx = D_tail;
            D_tail = U_next(D_tail);

            // 原地构造
            D_data[last_idx] = _Tp(std::forward<Args>(args)...);
            return true;
        }

        // pop/push_front/back
        bool push(const _Tp& value) noexcept {
            if (full()) {
                // in fact, we should throw an exception here
                // however, we don't have exception handling mechanism in Sustcore
                return false;
            }

            const IndexType last_idx = D_tail;
            D_tail = U_next(D_tail);

            D_data[last_idx] = value;
            return true;
        }

        bool pop() noexcept {
            if (empty()) {
                return false;
            }
            D_head = U_next(D_head);
            return true;
        }

        _Tp & front() noexcept {
            return D_data[D_head];
        }

        // clear
        void clear() noexcept {
            D_head = D_tail = 0;
        }

        bool contains(const _Tp& value) const noexcept {
            for (IndexType i = D_head; i != D_tail; i = U_next(i)) {
                if (D_data[i] == value) {
                    return true;
                }
            }
            return false;
        }
    };
}