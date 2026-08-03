/**
 * @file static_vector.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供具有未初始化内联存储的固定容量连续向量。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace tay {
    template <class T, size_t N>
    class static_vector {
    public:
        using value_type      = T;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = pointer;
        using const_iterator  = const_pointer;

    private:
        struct slot_type {
            alignas(value_type) unsigned char bytes[sizeof(value_type)];
        };

        slot_type storage_[N == 0 ? 1 : N];
        size_type size_ = 0;

        static_assert(std::is_nothrow_destructible_v<value_type>,
                      "static_vector requires a nothrow destructor");
        static_assert(std::is_nothrow_move_constructible_v<value_type>,
                      "static_vector requires nothrow move construction");
        static_assert(std::is_nothrow_move_assignable_v<value_type>,
                      "static_vector requires nothrow move assignment");

        [[nodiscard]] constexpr pointer ptr(size_type index) noexcept {
            return std::launder(reinterpret_cast<pointer>(storage_[index].bytes));
        }
        [[nodiscard]] constexpr const_pointer ptr(size_type index) const noexcept {
            return std::launder(reinterpret_cast<const_pointer>(storage_[index].bytes));
        }

        [[nodiscard]] constexpr expected<size_type, error_code> index_of(
            const_iterator position, bool allow_end = true) const noexcept {
            if (position == end() && allow_end) {
                return size_;
            }
            for (size_type i = 0; i < size_; ++i) {
                if (position == ptr(i)) {
                    return i;
                }
            }
            return expected<size_type, error_code>(unexpect, error_code::OUT_OF_RANGE);
        }

        [[noreturn]] static constexpr void panic_error(error_code error) {
            switch (error) {
                case error_code::OVERFLOW_ERROR: tay::panic("static_vector capacity exhausted");
                case error_code::OUT_OF_RANGE:   tay::panic("static_vector position out of range");
                default:                         tay::panic("static_vector operation failed");
            }
        }

    public:
        constexpr static_vector() noexcept = default;

        constexpr static_vector(std::initializer_list<value_type> values) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            auto result = assign(values.begin(), values.end());
            if (!result) {
                panic_error(result.error());
            }
        }

        constexpr static_vector(const static_vector& other) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            for (const auto& value : other) {
                static_cast<void>(std::construct_at(ptr(size_), value));
                ++size_;
            }
        }

        constexpr static_vector(static_vector&& other) noexcept {
            for (auto& value : other) {
                static_cast<void>(std::construct_at(ptr(size_), std::move(value)));
                ++size_;
            }
            other.clear();
        }

        constexpr static_vector& operator=(const static_vector& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_copy_assignable_v<value_type>)
        {
            if (this != &other) {
                auto result = assign(other.begin(), other.end());
                if (!result) {
                    panic_error(result.error());
                }
            }
            return *this;
        }

        constexpr static_vector& operator=(static_vector&& other) noexcept {
            if (this != &other) {
                clear();
                for (auto& value : other) {
                    static_cast<void>(std::construct_at(ptr(size_), std::move(value)));
                    ++size_;
                }
                other.clear();
            }
            return *this;
        }

        constexpr ~static_vector() noexcept {
            clear();
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            return N == 0 ? nullptr : ptr(0);
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return N == 0 ? nullptr : ptr(0);
        }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return begin();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return N == 0 ? nullptr : ptr(0) + size_;
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return N == 0 ? nullptr : ptr(0) + size_;
        }
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return end();
        }
        [[nodiscard]] constexpr pointer data() noexcept {
            return begin();
        }
        [[nodiscard]] constexpr const_pointer data() const noexcept {
            return begin();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        [[nodiscard]] constexpr bool full() const noexcept {
            return size_ == N;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] static constexpr size_type capacity() noexcept {
            return N;
        }
        [[nodiscard]] static constexpr size_type max_size() noexcept {
            return N;
        }

        [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
            return *ptr(index);
        }
        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept {
            return *ptr(index);
        }
        [[nodiscard]] constexpr expected<reference, error_code> at(size_type index) noexcept {
            if (index >= size_) {
                return expected<reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return *ptr(index);
        }
        [[nodiscard]] constexpr expected<const_reference, error_code> at(
            size_type index) const noexcept {
            if (index >= size_) {
                return expected<const_reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return *ptr(index);
        }
        [[nodiscard]] constexpr reference front() noexcept {
            return *ptr(0);
        }
        [[nodiscard]] constexpr const_reference front() const noexcept {
            return *ptr(0);
        }
        [[nodiscard]] constexpr reference back() noexcept {
            return *ptr(size_ - 1);
        }
        [[nodiscard]] constexpr const_reference back() const noexcept {
            return *ptr(size_ - 1);
        }

        constexpr expected<void, error_code> reserve(size_type requested) noexcept {
            if (requested > N) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            return {};
        }
        constexpr expected<void, error_code> shrink_to_fit() noexcept {
            return {};
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<reference, error_code> emplace_back(
            Args&&... args) noexcept {
            if (full()) {
                return expected<reference, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            pointer location = ptr(size_);
            static_cast<void>(std::construct_at(location, std::forward<Args>(args)...));
            ++size_;
            return *location;
        }
        constexpr expected<void, error_code> push_back(const_reference value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            auto result = emplace_back(value);
            return result ? expected<void, error_code>{}
                          : expected<void, error_code>(unexpect, result.error());
        }
        constexpr expected<void, error_code> push_back(value_type&& value) noexcept {
            auto result = emplace_back(std::move(value));
            return result ? expected<void, error_code>{}
                          : expected<void, error_code>(unexpect, result.error());
        }
        constexpr expected<void, error_code> pop_back() noexcept {
            if (empty()) {
                return expected<void, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            --size_;
            std::destroy_at(ptr(size_));
            return {};
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<iterator, error_code> emplace(const_iterator position,
                                                                       Args&&... args) noexcept {
            auto index = index_of(position);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            if (full()) {
                return expected<iterator, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            if (*index == size_) {
                auto result = emplace_back(std::forward<Args>(args)...);
                return result ? ptr(size_ - 1)
                              : expected<iterator, error_code>(unexpect, result.error());
            }
            static_cast<void>(std::construct_at(ptr(size_), std::move(back())));
            for (size_type i = size_ - 1; i > *index; --i) {
                *ptr(i) = std::move(*ptr(i - 1));
            }
            *ptr(*index) = value_type(std::forward<Args>(args)...);
            ++size_;
            return ptr(*index);
        }
        constexpr auto insert(const_iterator position, const_reference value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            return emplace(position, value);
        }
        constexpr auto insert(const_iterator position, value_type&& value) noexcept {
            return emplace(position, std::move(value));
        }

        constexpr expected<iterator, error_code> erase(const_iterator position) noexcept {
            auto index = index_of(position, false);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            for (size_type i = *index; i + 1 < size_; ++i) {
                *ptr(i) = std::move(*ptr(i + 1));
            }
            --size_;
            std::destroy_at(ptr(size_));
            return ptr(*index);
        }

        constexpr expected<iterator, error_code> erase(const_iterator first,
                                                       const_iterator last) noexcept {
            auto begin_index = index_of(first);
            auto end_index   = index_of(last);
            if (!begin_index || !end_index || *begin_index > *end_index) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type count = *end_index - *begin_index;
            for (size_type i = *begin_index; i + count < size_; ++i) {
                *ptr(i) = std::move(*ptr(i + count));
            }
            for (size_type i = 0; i < count; ++i) {
                --size_;
                std::destroy_at(ptr(size_));
            }
            return N == 0 ? nullptr : ptr(*begin_index);
        }

        constexpr void clear() noexcept {
            while (size_ != 0) {
                --size_;
                std::destroy_at(ptr(size_));
            }
        }

        template <class InputIt>
        constexpr expected<void, error_code> assign(InputIt first, InputIt last) noexcept {
            clear();
            for (; first != last; ++first) {
                auto result = emplace_back(*first);
                if (!result) {
                    clear();
                    return expected<void, error_code>(unexpect, result.error());
                }
            }
            return {};
        }

        constexpr expected<void, error_code> resize(size_type count) noexcept
            requires std::is_nothrow_default_constructible_v<value_type>
        {
            if (count > N) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            while (size_ > count) {
                static_cast<void>(pop_back());
            }
            while (size_ < count) {
                static_cast<void>(emplace_back());
            }
            return {};
        }
        constexpr expected<void, error_code> resize(size_type count, const_reference value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            if (count > N) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            while (size_ > count) {
                static_cast<void>(pop_back());
            }
            while (size_ < count) {
                static_cast<void>(emplace_back(value));
            }
            return {};
        }

        constexpr void swap(static_vector& other) noexcept {
            const size_type common = size_ < other.size_ ? size_ : other.size_;
            for (size_type i = 0; i < common; ++i) {
                using std::swap;
                swap((*this)[i], other[i]);
            }
            if (size_ > other.size_) {
                const size_type old = size_;
                for (size_type i = common; i < old; ++i) {
                    static_cast<void>(
                        std::construct_at(other.ptr(other.size_), std::move((*this)[i])));
                    ++other.size_;
                }
                while (size_ > common) {
                    --size_;
                    std::destroy_at(ptr(size_));
                }
            } else {
                const size_type old = other.size_;
                for (size_type i = common; i < old; ++i) {
                    static_cast<void>(std::construct_at(ptr(size_), std::move(other[i])));
                    ++size_;
                }
                while (other.size_ > common) {
                    --other.size_;
                    std::destroy_at(other.ptr(other.size_));
                }
            }
        }
    };

    template <class T, size_t N>
    [[nodiscard]] constexpr bool operator==(const static_vector<T, N>& left,
                                            const static_vector<T, N>& right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (size_t i = 0; i < left.size(); ++i) {
            if (!(left[i] == right[i])) {
                return false;
            }
        }
        return true;
    }
}  // namespace tay
