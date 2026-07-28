/**
 * @file string_view.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Exception-free, non-owning character string view.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/err.h>
#include <tay/expected.h>

#include <compare>
#include <cstddef>
#include <utility>

namespace tay {
    class string_view {
    public:
        using value_type          = char;
        using pointer             = char*;
        using const_pointer       = const char*;
        using reference           = char&;
        using const_reference     = const char&;
        using const_iterator      = const_pointer;
        using iterator            = const_iterator;
        using size_type           = std::size_t;
        using difference_type     = std::ptrdiff_t;
        using comparison_category = std::strong_ordering;

        static constexpr size_type npos = size_type(-1);

    private:
        const_pointer data_ = nullptr;
        size_type size_     = 0;

        [[nodiscard]]
        static constexpr size_type M_cstrlen(const_pointer string) noexcept {
            size_type length = 0;
            while (string[length] != '\0') {
                ++length;
            }
            return length;
        }

        [[nodiscard]]
        static constexpr int M_cmemcmp(const_pointer left, const_pointer right,
                                       size_type count) noexcept {
            for (size_type index = 0; index < count; ++index) {
                const auto left_char = static_cast<unsigned char>(left[index]);
                const auto right_char =
                    static_cast<unsigned char>(right[index]);
                if (left_char < right_char) {
                    return -1;
                }
                if (left_char > right_char) {
                    return 1;
                }
            }
            return 0;
        }

        [[nodiscard]]
        static constexpr bool M_contain(const_pointer characters,
                                        size_type count,
                                        char character) noexcept {
            for (size_type index = 0; index < count; ++index) {
                if (characters[index] == character) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]]
        constexpr const_pointer data_offset(size_type offset) const noexcept {
            return data_ + offset;
        }

    public:
        constexpr string_view() noexcept = default;

        constexpr string_view(const_pointer string) noexcept
            : data_(string), size_(M_cstrlen(string)) {}

        constexpr string_view(decltype(nullptr)) = delete;

        constexpr string_view(const_pointer string, size_type length) noexcept
            : data_(string), size_(length) {}

        constexpr string_view(const_pointer first, const_pointer last) noexcept
            : data_(first), size_(static_cast<size_type>(last - first)) {}

        constexpr string_view(const string_view&) noexcept            = default;
        constexpr string_view(string_view&&) noexcept                 = default;
        constexpr string_view& operator=(const string_view&) noexcept = default;
        constexpr string_view& operator=(string_view&&) noexcept      = default;
        constexpr ~string_view() noexcept                             = default;

        [[nodiscard]]
        constexpr const_iterator begin() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr const_iterator end() const noexcept {
            return data_offset(size_);
        }

        [[nodiscard]]
        constexpr const_iterator cbegin() const noexcept {
            return begin();
        }

        [[nodiscard]]
        constexpr const_iterator cend() const noexcept {
            return end();
        }

        // shouldn't be used unless you can make sure if position is smaller
        // than its length
        [[nodiscard]]
        constexpr const_reference operator[](
            size_type position) const noexcept {
            return data_[position];
        }

        [[nodiscard]]
        constexpr expected<const_reference, error_code> at(
            size_type position) const noexcept {
            if (position >= size_) {
                return expected<const_reference, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            return data_[position];
        }

        [[nodiscard]]
        constexpr const_reference front() const noexcept {
            return data_[0];
        }

        [[nodiscard]]
        constexpr const_reference back() const noexcept {
            return data_[size_ - 1];
        }

        [[nodiscard]]
        constexpr const_pointer data() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]]
        constexpr size_type length() const noexcept {
            return size_;
        }

        [[nodiscard]]
        constexpr size_type max_size() const noexcept {
            return (npos - sizeof(size_type) - sizeof(const_pointer)) / 4;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        constexpr expected<void, error_code> remove_prefix(
            size_type count) noexcept {
            if (count > size_) {
                return expected<void, error_code>(unexpect,
                                                  error_code::OUT_OF_RANGE);
            }
            data_  = data_offset(count);
            size_ -= count;
            return {};
        }

        constexpr expected<void, error_code> remove_suffix(
            size_type count) noexcept {
            if (count > size_) {
                return expected<void, error_code>(unexpect,
                                                  error_code::OUT_OF_RANGE);
            }
            size_ -= count;
            return {};
        }

        constexpr void swap(string_view& other) noexcept {
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> copy(
            pointer destination, size_type count,
            size_type position = 0) const noexcept {
            if (position > size_) {
                return expected<size_type, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }

            const size_type available = size_ - position;
            const size_type copied    = count < available ? count : available;
            for (size_type index = 0; index < copied; ++index) {
                destination[index] = data_[position + index];
            }
            return copied;
        }

        [[nodiscard]]
        constexpr expected<string_view, error_code> substr(
            size_type position = 0, size_type count = npos) const noexcept {
            if (position > size_) {
                return expected<string_view, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }

            const size_type available = size_ - position;
            const size_type selected  = count < available ? count : available;
            return string_view(data_offset(position), selected);
        }

        [[nodiscard]]
        constexpr int compare(string_view other) const noexcept {
            const size_type common = size_ < other.size_ ? size_ : other.size_;
            const int result       = M_cmemcmp(data_, other.data_, common);
            if (result != 0) {
                return result;
            }
            if (size_ < other.size_) {
                return -1;
            }
            if (size_ > other.size_) {
                return 1;
            }
            return 0;
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(
            size_type position, size_type count,
            string_view other) const noexcept {
            auto selected = substr(position, count);
            if (!selected) {
                return expected<int, error_code>(unexpect, selected.error());
            }
            return (*selected).compare(other);
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(
            size_type position, size_type count, string_view other,
            size_type other_position, size_type other_count) const noexcept {
            auto selected       = substr(position, count);
            auto other_selected = other.substr(other_position, other_count);
            if (!selected) {
                return expected<int, error_code>(unexpect, selected.error());
            }
            if (!other_selected) {
                return expected<int, error_code>(unexpect,
                                                 other_selected.error());
            }
            return (*selected).compare(*other_selected);
        }

        [[nodiscard]]
        constexpr int compare(const_pointer string) const noexcept {
            return compare(string_view(string));
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(
            size_type position, size_type count,
            const_pointer string) const noexcept {
            return compare(position, count, string_view(string));
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(
            size_type position, size_type count, const_pointer string,
            size_type string_count) const noexcept {
            return compare(position, count, string_view(string, string_count));
        }

        [[nodiscard]]
        constexpr bool starts_with(string_view prefix) const noexcept {
            return prefix.size_ <= size_ &&
                   M_cmemcmp(data_, prefix.data_, prefix.size_) == 0;
        }

        [[nodiscard]]
        constexpr bool starts_with(char character) const noexcept {
            return !empty() && data_[0] == character;
        }

        [[nodiscard]]
        constexpr bool starts_with(const_pointer string) const noexcept {
            return starts_with(string_view(string));
        }

        [[nodiscard]]
        constexpr bool ends_with(string_view suffix) const noexcept {
            return suffix.size_ <= size_ &&
                   M_cmemcmp(data_offset(size_ - suffix.size_), suffix.data_,
                             suffix.size_) == 0;
        }

        [[nodiscard]]
        constexpr bool ends_with(char character) const noexcept {
            return !empty() && data_[size_ - 1] == character;
        }

        [[nodiscard]]
        constexpr bool ends_with(const_pointer string) const noexcept {
            return ends_with(string_view(string));
        }

        [[nodiscard]]
        constexpr bool contains(string_view string) const noexcept {
            return find(string) != npos;
        }

        [[nodiscard]]
        constexpr bool contains(char character) const noexcept {
            return find(character) != npos;
        }

        [[nodiscard]]
        constexpr bool contains(const_pointer string) const noexcept {
            return contains(string_view(string));
        }

        [[nodiscard]]
        constexpr size_type find(char character,
                                 size_type position = 0) const noexcept {
            if (position >= size_) {
                return npos;
            }
            for (size_type index = position; index < size_; ++index) {
                if (data_[index] == character) {
                    return index;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type find(string_view string,
                                 size_type position = 0) const noexcept {
            return find(string.data_, position, string.size_);
        }

        [[nodiscard]]
        constexpr size_type find(const_pointer string,
                                 size_type position = 0) const noexcept {
            return find(string, position, M_cstrlen(string));
        }

        [[nodiscard]]
        constexpr size_type find(const_pointer string, size_type position,
                                 size_type count) const noexcept {
            if (position > size_) {
                return npos;
            }
            if (count == 0) {
                return position;
            }
            if (count > size_ - position) {
                return npos;
            }

            // TODO: implement a more efficient search algorithm like:
            // KMP(T: O(n + m), S: O(m))
            // Two-Way(T: O(n + m), S: O(1))
            // I think it would be better to apply different algorithms on different situations
            // let L to be the length of pattern string
            // then when L <= 8, use naive search algorithm,
            // when 8 < L <= 32, use Two-Way algorithm,
            // when L > 32, use Two-Way with bad characters
            for (size_type index = position; index <= size_ - count; ++index) {
                if (M_cmemcmp(data_ + index, string, count) == 0) {
                    return index;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type rfind(char character,
                                  size_type position = npos) const noexcept {
            const size_type last = size_ - 1;
            const size_type start = position < last ? position : last;
            for (size_type index = start + 1; index > 0; --index) {
                if (data_[index - 1] == character) {
                    return index - 1;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type rfind(string_view string,
                                  size_type position = npos) const noexcept {
            return rfind(string.data_, position, string.size_);
        }

        [[nodiscard]]
        constexpr size_type rfind(const_pointer string,
                                  size_type position = npos) const noexcept {
            return rfind(string, position, M_cstrlen(string));
        }

        [[nodiscard]]
        constexpr size_type rfind(const_pointer string, size_type position,
                                  size_type count) const noexcept {
            if (count == 0) {
                return position < size_ ? position : size_;
            }
            if (count > size_) {
                return npos;
            }

            const size_type last  = size_ - count;
            const size_type start = position < last ? position : last;
            for (size_type index = start + 1; index > 0; --index) {
                if (M_cmemcmp(data_ + index - 1, string, count) == 0) {
                    return index - 1;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type find_first_of(
            char character, size_type position = 0) const noexcept {
            return find(character, position);
        }

        [[nodiscard]]
        constexpr size_type find_first_of(
            string_view characters, size_type position = 0) const noexcept {
            return find_first_of(characters.data_, position, characters.size_);
        }

        [[nodiscard]]
        constexpr size_type find_first_of(
            const_pointer characters, size_type position = 0) const noexcept {
            return find_first_of(characters, position, M_cstrlen(characters));
        }

        [[nodiscard]]
        constexpr size_type find_first_of(const_pointer characters,
                                          size_type position,
                                          size_type count) const noexcept {
            for (size_type index = position; index < size_; ++index) {
                if (M_contain(characters, count, data_[index])) {
                    return index;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type find_last_of(
            char character, size_type position = npos) const noexcept {
            return rfind(character, position);
        }

        [[nodiscard]]
        constexpr size_type find_last_of(
            string_view characters, size_type position = npos) const noexcept {
            return find_last_of(characters.data_, position, characters.size_);
        }

        [[nodiscard]]
        constexpr size_type find_last_of(
            const_pointer characters,
            size_type position = npos) const noexcept {
            return find_last_of(characters, position, M_cstrlen(characters));
        }

        [[nodiscard]]
        constexpr size_type find_last_of(const_pointer characters,
                                         size_type position,
                                         size_type count) const noexcept {
            if (size_ == 0 || count == 0) {
                return npos;
            }

            const size_type start = position < size_ ? position : size_ - 1;
            for (size_type index = start + 1; index > 0; --index) {
                if (M_contain(characters, count, data_[index - 1])) {
                    return index - 1;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(
            char character, size_type position = 0) const noexcept {
            return find_first_not_of(&character, position, 1);
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(
            string_view characters, size_type position = 0) const noexcept {
            return find_first_not_of(characters.data_, position,
                                     characters.size_);
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(
            const_pointer characters, size_type position = 0) const noexcept {
            return find_first_not_of(characters, position,
                                     M_cstrlen(characters));
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(const_pointer characters,
                                              size_type position,
                                              size_type count) const noexcept {
            for (size_type index = position; index < size_; ++index) {
                if (!M_contain(characters, count, data_[index])) {
                    return index;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(
            char character, size_type position = npos) const noexcept {
            return find_last_not_of(&character, position, 1);
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(
            string_view characters, size_type position = npos) const noexcept {
            return find_last_not_of(characters.data_, position,
                                    characters.size_);
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(
            const_pointer characters,
            size_type position = npos) const noexcept {
            return find_last_not_of(characters, position,
                                    M_cstrlen(characters));
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(const_pointer characters,
                                             size_type position,
                                             size_type count) const noexcept {
            if (size_ == 0) {
                return npos;
            }

            const size_type start = position < size_ ? position : size_ - 1;
            for (size_type index = start + 1; index > 0; --index) {
                if (!M_contain(characters, count, data_[index - 1])) {
                    return index - 1;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr bool operator==(string_view other) const noexcept {
            return size_ == other.size_ && compare(other) == 0;
        }

        [[nodiscard]]
        constexpr bool operator==(const_pointer other) const noexcept {
            return *this == string_view(other);
        }

        [[nodiscard]]
        constexpr comparison_category operator<=>(
            string_view other) const noexcept {
            const int result = compare(other);
            if (result < 0) {
                return comparison_category::less;
            }
            if (result > 0) {
                return comparison_category::greater;
            }
            return comparison_category::equal;
        }

        [[nodiscard]]
        constexpr comparison_category operator<=>(
            const_pointer other) const noexcept {
            return *this <=> string_view(other);
        }
    };

    constexpr void swap(string_view& left, string_view& right) noexcept {
        left.swap(right);
    }

    struct string_view_hash {
        [[nodiscard]]
        constexpr std::size_t operator()(string_view string) const noexcept {
            constexpr std::size_t magic_number = 42;
            std::size_t hash                   = 0;
            for (char character : string) {
                hash =
                    hash * magic_number + static_cast<unsigned char>(character);
            }
            return hash;
        }
    };

    inline namespace literals {
        inline namespace string_view_literals {
            [[nodiscard]]
            constexpr string_view operator""_sv(const char* string,
                                                std::size_t length) noexcept {
                return {string, length};
            }
        }  // namespace string_view_literals
    }  // namespace literals
}  // namespace tay
