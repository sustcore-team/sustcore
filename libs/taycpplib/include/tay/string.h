/**
 * @file string.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供带显式分配器的无异常拥有型字符串。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/allocator.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>
#include <tay/string_view.h>

#include <compare>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    template <class Allocator = allocator<char>>
    class string {
    public:
        using allocator_type        = Allocator;
        using allocator_traits_type = allocator_traits<allocator_type>;
        using value_type            = char;
        using size_type             = typename allocator_traits_type::size_type;
        using difference_type       = typename allocator_traits_type::difference_type;
        using reference             = value_type&;
        using const_reference       = const value_type&;
        using pointer               = typename allocator_traits_type::pointer;
        using const_pointer         = typename allocator_traits_type::const_pointer;
        using iterator              = pointer;
        using const_iterator        = const_pointer;

        static constexpr size_type npos = size_type(-1);

    private:
        static_assert(std::is_same_v<typename allocator_traits_type::value_type, char>,
                      "tay::string requires an allocator whose value_type is char");
        static_assert(std::is_same_v<pointer, char*>,
                      "tay::string currently requires raw allocator pointers");

        static constexpr size_type local_bytes    = 16;
        static constexpr size_type local_capacity = local_bytes - 1;
        static constexpr bool has_default_allocator =
            std::is_nothrow_default_constructible_v<allocator_type>;

        union storage_type {
            size_type capacity;
            char local[local_bytes];

            constexpr storage_type() noexcept : local{} {}
            constexpr ~storage_type() noexcept {}
        };

        struct empty_tag {};

        [[no_unique_address]] allocator_type allocator_;
        pointer data_   = nullptr;
        size_type size_ = 0;
        storage_type storage_{};

        [[noreturn]] static constexpr void panic_error(error_code error) noexcept {
            switch (error) {
                case error_code::OUT_OF_MEMORY:            tay::panic("string allocation failed");
                case error_code::ALLOCATION_SIZE_OVERFLOW: tay::panic("string size overflow");
                case error_code::OUT_OF_RANGE:             tay::panic("string position out of range");
                case error_code::NULLPTR:                  tay::panic("string received a null pointer");
                case error_code::INVALID_ARGUMENT:
                    tay::panic("string received an invalid argument");
                default: tay::panic("string operation failed");
            }
        }

        template <class T>
        static constexpr void panic_if_error(const expected<T, error_code>& result) noexcept {
            if (!result) {
                panic_error(result.error());
            }
        }

        [[nodiscard]]
        static constexpr size_type cstrlen(const_pointer text) noexcept {
            size_type length = 0;
            while (text[length] != '\0') {
                ++length;
            }
            return length;
        }

        static constexpr void copy_chars(pointer destination, const_pointer source,
                                         size_type count) noexcept {
            for (size_type index = 0; index < count; ++index) {
                destination[index] = source[index];
            }
        }

        static constexpr void move_chars(pointer destination, const_pointer source,
                                         size_type count) noexcept {
            if (destination < source) {
                copy_chars(destination, source, count);
                return;
            }
            if (destination > source) {
                for (size_type index = count; index > 0; --index) {
                    destination[index - 1] = source[index - 1];
                }
            }
        }

        static constexpr void fill_chars(pointer destination, size_type count,
                                         char character) noexcept {
            for (size_type index = 0; index < count; ++index) {
                destination[index] = character;
            }
        }

        constexpr explicit string(empty_tag, const allocator_type& allocator) noexcept
            : allocator_(allocator) {
            init_empty();
        }

        constexpr explicit string(empty_tag, allocator_type&& allocator) noexcept
            : allocator_(std::move(allocator)) {
            init_empty();
        }

        constexpr void init_empty() noexcept {
            data_             = storage_.local;
            size_             = 0;
            storage_.local[0] = '\0';
        }

        [[nodiscard]]
        constexpr bool is_local() const noexcept {
            return data_ == storage_.local;
        }

        [[nodiscard]]
        constexpr size_type alias_offset(const_pointer text) const noexcept {
            if consteval {
                return npos;
            }
            for (size_type index = 0; index <= size_; ++index) {
                if (text == data_ + index) {
                    return index;
                }
            }
            return npos;
        }

        [[nodiscard]]
        constexpr size_type dynamic_capacity() const noexcept {
            return storage_.capacity;
        }

        constexpr void release_dynamic() noexcept {
            if (!is_local()) {
                allocator_traits_type::deallocate(allocator_, data_, dynamic_capacity() + 1);
            }
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> grown_capacity(
            size_type requested) const noexcept {
            if (requested > max_size()) {
                return expected<size_type, error_code>(unexpect,
                                                       error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            size_type current = capacity();
            if (current >= requested) {
                return current;
            }
            if (current > max_size() - current) {
                return max_size();
            }
            const size_type doubled = current + current;
            return doubled < requested ? requested : doubled;
        }

        constexpr expected<void, error_code> reallocate(size_type new_capacity) noexcept {
            if (new_capacity <= capacity()) {
                return {};
            }
            if (new_capacity > max_size()) {
                return expected<void, error_code>(unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            auto allocation = allocator_traits_type::try_allocate(allocator_, new_capacity + 1);
            if (!allocation) {
                return expected<void, error_code>(unexpect, allocation.error());
            }

            pointer new_data = *allocation;
            copy_chars(new_data, data_, size_ + 1);

            pointer old_data             = data_;
            const bool old_local         = is_local();
            const size_type old_capacity = capacity();
            data_                        = new_data;
            storage_.capacity            = new_capacity;
            if (!old_local) {
                allocator_traits_type::deallocate(allocator_, old_data, old_capacity + 1);
            }
            return {};
        }

        constexpr expected<void, error_code> ensure_capacity(size_type requested) noexcept {
            if (requested <= capacity()) {
                return {};
            }
            auto grown = grown_capacity(requested);
            if (!grown) {
                return expected<void, error_code>(unexpect, grown.error());
            }
            return reallocate(*grown);
        }

        constexpr void take_storage(string&& other) noexcept {
            if (other.is_local()) {
                data_ = storage_.local;
                size_ = other.size_;
                copy_chars(storage_.local, other.storage_.local, other.size_ + 1);
            } else {
                data_             = other.data_;
                size_             = other.size_;
                storage_.capacity = other.storage_.capacity;
                other.init_empty();
            }
        }

        constexpr void replace_with(string&& replacement) noexcept {
            release_dynamic();
            take_storage(std::move(replacement));
        }

        constexpr void swap_storage(string& other) noexcept {
            const bool left_local  = is_local();
            const bool right_local = other.is_local();

            if (!left_local && !right_local) {
                std::swap(data_, other.data_);
                std::swap(size_, other.size_);
                std::swap(storage_.capacity, other.storage_.capacity);
                return;
            }

            if (left_local && right_local) {
                for (size_type index = 0; index < local_bytes; ++index) {
                    std::swap(storage_.local[index], other.storage_.local[index]);
                }
                std::swap(size_, other.size_);
                return;
            }

            if (!left_local && right_local) {
                other.swap_storage(*this);
                return;
            }

            char saved_local[local_bytes]{};
            copy_chars(saved_local, storage_.local, size_ + 1);
            const size_type saved_size = size_;

            data_             = other.data_;
            size_             = other.size_;
            storage_.capacity = other.storage_.capacity;

            other.data_ = other.storage_.local;
            other.size_ = saved_size;
            copy_chars(other.storage_.local, saved_local, saved_size + 1);
        }

        template <class InputIt>
        static constexpr expected<string, error_code> try_create_range(
            InputIt first, InputIt last, const allocator_type& allocator) noexcept {
            string result(empty_tag{}, allocator);
            for (; first != last; ++first) {
                auto appended = result.push_back(static_cast<char>(*first));
                if (!appended) {
                    return expected<string, error_code>(unexpect, appended.error());
                }
            }
            return std::move(result);
        }

    public:
        constexpr string() noexcept
            requires(has_default_allocator)
            : string(allocator_type{}) {}

        constexpr explicit string(const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {}

        constexpr string(size_type count, char character) noexcept
            requires(has_default_allocator)
            : string(count, character, allocator_type{}) {}

        constexpr string(size_type count, char character, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(count, character));
        }

        constexpr string(const_pointer text, size_type count) noexcept
            requires(has_default_allocator)
            : string(text, count, allocator_type{}) {}

        constexpr string(const_pointer text, size_type count,
                         const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(text, count));
        }

        constexpr string(const_pointer text) noexcept
            requires(has_default_allocator)
            : string(text, allocator_type{}) {}

        constexpr string(const_pointer text, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            if (text == nullptr) {
                panic_error(error_code::NULLPTR);
            }
            panic_if_error(assign(text, cstrlen(text)));
        }

        constexpr string(string_view view) noexcept
            requires(has_default_allocator)
            : string(view, allocator_type{}) {}

        constexpr string(string_view view, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(view));
        }

        constexpr string(string_view view, size_type position, size_type count) noexcept
            requires(has_default_allocator)
            : string(view, position, count, allocator_type{}) {}

        constexpr string(string_view view, size_type position, size_type count,
                         const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(view, position, count));
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> && has_default_allocator)
        constexpr string(InputIt first, InputIt last) noexcept
            : string(first, last, allocator_type{}) {}

        template <class InputIt>
            requires(!std::is_integral_v<InputIt>)
        constexpr string(InputIt first, InputIt last, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            auto created = try_create_range(first, last, allocator_);
            panic_if_error(created);
            replace_with(std::move(*created));
        }

        constexpr string(std::initializer_list<char> characters) noexcept
            requires(has_default_allocator)
            : string(characters, allocator_type{}) {}

        constexpr string(std::initializer_list<char> characters,
                         const allocator_type& allocator) noexcept
            : string(characters.begin(), characters.end(), allocator) {}

        constexpr string(const string& other) noexcept
            : string(empty_tag{}, allocator_traits_type::select_on_container_copy_construction(
                                      other.allocator_)) {
            panic_if_error(assign(other));
        }

        constexpr string(const string& other, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(other));
        }

        constexpr string(const string& other, size_type position, size_type count) noexcept
            requires(has_default_allocator)
            : string(other, position, count, allocator_type{}) {}

        constexpr string(const string& other, size_type position, size_type count,
                         const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            panic_if_error(assign(other, position, count));
        }

        constexpr string(string&& other) noexcept
            : string(empty_tag{}, std::move(other.allocator_)) {
            take_storage(std::move(other));
        }

        constexpr string(string&& other, const allocator_type& allocator) noexcept
            : string(empty_tag{}, allocator) {
            if constexpr (allocator_traits_type::is_always_equal::value) {
                take_storage(std::move(other));
            } else if (allocator_ == other.allocator_) {
                take_storage(std::move(other));
            } else {
                panic_if_error(assign(other));
            }
        }

        string(decltype(nullptr))                        = delete;
        string(decltype(nullptr), const allocator_type&) = delete;

        constexpr ~string() noexcept {
            release_dynamic();
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create() noexcept
            requires(has_default_allocator)
        {
            return try_create(allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            const allocator_type& allocator) noexcept {
            return string(empty_tag{}, allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(size_type count,
                                                                 char character) noexcept
            requires(has_default_allocator)
        {
            return try_create(count, character, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            size_type count, char character, const allocator_type& allocator) noexcept {
            string result(empty_tag{}, allocator);
            auto assigned = result.assign(count, character);
            if (!assigned) {
                return expected<string, error_code>(unexpect, assigned.error());
            }
            return std::move(result);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(const_pointer text,
                                                                 size_type count) noexcept
            requires(has_default_allocator)
        {
            return try_create(text, count, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            const_pointer text, size_type count, const allocator_type& allocator) noexcept {
            string result(empty_tag{}, allocator);
            auto assigned = result.assign(text, count);
            if (!assigned) {
                return expected<string, error_code>(unexpect, assigned.error());
            }
            return std::move(result);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(const_pointer text) noexcept
            requires(has_default_allocator)
        {
            return try_create(text, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            const_pointer text, const allocator_type& allocator) noexcept {
            if (text == nullptr) {
                return expected<string, error_code>(unexpect, error_code::NULLPTR);
            }
            return try_create(text, cstrlen(text), allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(string_view view) noexcept
            requires(has_default_allocator)
        {
            return try_create(view, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            string_view view, const allocator_type& allocator) noexcept {
            return try_create(view.data(), view.size(), allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(string_view view,
                                                                 size_type position,
                                                                 size_type count) noexcept
            requires(has_default_allocator)
        {
            return try_create(view, position, count, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            string_view view, size_type position, size_type count,
            const allocator_type& allocator) noexcept {
            auto selected = view.substr(position, count);
            if (!selected) {
                return expected<string, error_code>(unexpect, selected.error());
            }
            return try_create(*selected, allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(const string& other) noexcept
            requires(has_default_allocator)
        {
            return try_create(other, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            const string& other, const allocator_type& allocator) noexcept {
            return try_create(other.data(), other.size(), allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(const string& other,
                                                                 size_type position,
                                                                 size_type count) noexcept
            requires(has_default_allocator)
        {
            return try_create(other, position, count, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            const string& other, size_type position, size_type count,
            const allocator_type& allocator) noexcept {
            return try_create(string_view(other), position, count, allocator);
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> && has_default_allocator)
        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(InputIt first,
                                                                 InputIt last) noexcept {
            return try_create(first, last, allocator_type{});
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt>)
        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            InputIt first, InputIt last, const allocator_type& allocator) noexcept {
            return try_create_range(first, last, allocator);
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            std::initializer_list<char> characters) noexcept
            requires(has_default_allocator)
        {
            return try_create(characters, allocator_type{});
        }

        [[nodiscard]]
        static constexpr expected<string, error_code> try_create(
            std::initializer_list<char> characters, const allocator_type& allocator) noexcept {
            return try_create_range(characters.begin(), characters.end(), allocator);
        }

        constexpr string& operator=(const string& other) noexcept {
            if (this == &other) {
                return *this;
            }

            if constexpr (allocator_traits_type::propagate_on_container_copy_assignment::value) {
                if constexpr (!allocator_traits_type::is_always_equal::value) {
                    if (!(allocator_ == other.allocator_)) {
                        auto replacement = try_create(other, other.allocator_);
                        panic_if_error(replacement);
                        release_dynamic();
                        allocator_ = other.allocator_;
                        init_empty();
                        take_storage(std::move(*replacement));
                        return *this;
                    }
                }
                allocator_ = other.allocator_;
            }
            panic_if_error(assign(other));
            return *this;
        }

        constexpr string& operator=(string&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            if constexpr (allocator_traits_type::propagate_on_container_move_assignment::value) {
                release_dynamic();
                allocator_ = std::move(other.allocator_);
                init_empty();
                take_storage(std::move(other));
            } else if constexpr (allocator_traits_type::is_always_equal::value) {
                release_dynamic();
                init_empty();
                take_storage(std::move(other));
            } else if (allocator_ == other.allocator_) {
                release_dynamic();
                init_empty();
                take_storage(std::move(other));
            } else {
                panic_if_error(assign(other));
                other.clear();
            }
            return *this;
        }

        constexpr string& operator=(const_pointer text) noexcept {
            if (text == nullptr) {
                panic_error(error_code::NULLPTR);
            }
            panic_if_error(assign(text, cstrlen(text)));
            return *this;
        }

        constexpr string& operator=(char character) noexcept {
            panic_if_error(assign(1, character));
            return *this;
        }

        constexpr string& operator=(string_view view) noexcept {
            panic_if_error(assign(view));
            return *this;
        }

        constexpr string& operator=(std::initializer_list<char> characters) noexcept {
            panic_if_error(assign(characters));
            return *this;
        }

        [[nodiscard]]
        constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }

        [[nodiscard]]
        constexpr expected<reference, error_code> at(size_type position) noexcept {
            if (position >= size_) {
                return expected<reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return data_[position];
        }

        [[nodiscard]]
        constexpr expected<const_reference, error_code> at(size_type position) const noexcept {
            if (position >= size_) {
                return expected<const_reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return data_[position];
        }

        [[nodiscard]]
        constexpr reference operator[](size_type position) noexcept {
            return data_[position];
        }

        [[nodiscard]]
        constexpr const_reference operator[](size_type position) const noexcept {
            return data_[position];
        }

        [[nodiscard]]
        constexpr reference front() noexcept {
            return data_[0];
        }

        [[nodiscard]]
        constexpr const_reference front() const noexcept {
            return data_[0];
        }

        [[nodiscard]]
        constexpr reference back() noexcept {
            return data_[size_ - 1];
        }

        [[nodiscard]]
        constexpr const_reference back() const noexcept {
            return data_[size_ - 1];
        }

        [[nodiscard]]
        constexpr pointer data() noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr const_pointer data() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr const_pointer c_str() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr iterator begin() noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr const_iterator begin() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr const_iterator cbegin() const noexcept {
            return data_;
        }

        [[nodiscard]]
        constexpr iterator end() noexcept {
            return data_ + size_;
        }

        [[nodiscard]]
        constexpr const_iterator end() const noexcept {
            return data_ + size_;
        }

        [[nodiscard]]
        constexpr const_iterator cend() const noexcept {
            return data_ + size_;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return size_ == 0;
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
        constexpr size_type capacity() const noexcept {
            return is_local() ? local_capacity : dynamic_capacity();
        }

        [[nodiscard]]
        constexpr size_type max_size() const noexcept {
            const size_type allocator_max = allocator_traits_type::max_size(allocator_);
            return allocator_max == 0 ? 0 : allocator_max - 1;
        }

        constexpr expected<void, error_code> reserve(size_type new_capacity) noexcept {
            if (new_capacity > max_size()) {
                return expected<void, error_code>(unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            return reallocate(new_capacity);
        }

        constexpr expected<void, error_code> shrink_to_fit() noexcept {
            if (is_local() || size_ == capacity()) {
                return {};
            }

            if (size_ <= local_capacity) {
                pointer old_data             = data_;
                const size_type old_capacity = capacity();
                char saved[local_bytes]{};
                copy_chars(saved, old_data, size_ + 1);
                data_ = storage_.local;
                copy_chars(storage_.local, saved, size_ + 1);
                allocator_traits_type::deallocate(allocator_, old_data, old_capacity + 1);
                return {};
            }

            auto allocation = allocator_traits_type::try_allocate(allocator_, size_ + 1);
            if (!allocation) {
                return expected<void, error_code>(unexpect, allocation.error());
            }
            pointer new_data = *allocation;
            copy_chars(new_data, data_, size_ + 1);
            pointer old_data             = data_;
            const size_type old_capacity = capacity();
            data_                        = new_data;
            storage_.capacity            = size_;
            allocator_traits_type::deallocate(allocator_, old_data, old_capacity + 1);
            return {};
        }

        constexpr void clear() noexcept {
            size_    = 0;
            data_[0] = '\0';
        }

        constexpr expected<string&, error_code> assign(size_type count, char character) noexcept {
            if (count > max_size()) {
                return expected<string&, error_code>(unexpect,
                                                     error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            auto prepared = ensure_capacity(count);
            if (!prepared) {
                return expected<string&, error_code>(unexpect, prepared.error());
            }
            fill_chars(data_, count, character);
            size_        = count;
            data_[size_] = '\0';
            return *this;
        }

        constexpr expected<string&, error_code> assign(const_pointer text,
                                                       size_type count) noexcept {
            if (text == nullptr && count != 0) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }
            if (count > max_size()) {
                return expected<string&, error_code>(unexpect,
                                                     error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            if (count <= capacity()) {
                if (alias_offset(text) != npos) {
                    move_chars(data_, text, count);
                } else {
                    copy_chars(data_, text, count);
                }
                size_        = count;
                data_[size_] = '\0';
                return *this;
            }

            auto allocation = allocator_traits_type::try_allocate(allocator_, count + 1);
            if (!allocation) {
                return expected<string&, error_code>(unexpect, allocation.error());
            }
            pointer new_data = *allocation;
            copy_chars(new_data, text, count);
            new_data[count] = '\0';

            pointer old_data             = data_;
            const bool old_local         = is_local();
            const size_type old_capacity = capacity();
            data_                        = new_data;
            size_                        = count;
            storage_.capacity            = count;
            if (!old_local) {
                allocator_traits_type::deallocate(allocator_, old_data, old_capacity + 1);
            }
            return *this;
        }

        constexpr expected<string&, error_code> assign(const_pointer text) noexcept {
            if (text == nullptr) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }
            return assign(text, cstrlen(text));
        }

        constexpr expected<string&, error_code> assign(string_view view) noexcept {
            return assign(view.data(), view.size());
        }

        constexpr expected<string&, error_code> assign(string_view view, size_type position,
                                                       size_type count = npos) noexcept {
            auto selected = view.substr(position, count);
            if (!selected) {
                return expected<string&, error_code>(unexpect, selected.error());
            }
            return assign(*selected);
        }

        constexpr expected<string&, error_code> assign(const string& other) noexcept {
            if (this == &other) {
                return *this;
            }
            return assign(other.data(), other.size());
        }

        constexpr expected<string&, error_code> assign(const string& other, size_type position,
                                                       size_type count = npos) noexcept {
            return assign(string_view(other), position, count);
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt>)
        constexpr expected<string&, error_code> assign(InputIt first, InputIt last) noexcept {
            auto replacement = try_create_range(first, last, allocator_);
            if (!replacement) {
                return expected<string&, error_code>(unexpect, replacement.error());
            }
            replace_with(std::move(*replacement));
            return *this;
        }

        constexpr expected<string&, error_code> assign(
            std::initializer_list<char> characters) noexcept {
            return assign(characters.begin(), characters.end());
        }

        constexpr expected<string&, error_code> insert(size_type position, const_pointer text,
                                                       size_type count) noexcept {
            if (position > size_) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            if (text == nullptr && count != 0) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }
            if (count == 0) {
                return *this;
            }
            if (count > max_size() - size_) {
                return expected<string&, error_code>(unexpect,
                                                     error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            if (alias_offset(text) != npos) {
                auto temporary = try_create(text, count, allocator_);
                if (!temporary) {
                    return expected<string&, error_code>(unexpect, temporary.error());
                }
                return insert(position, temporary->data(), temporary->size());
            }

            const size_type new_size = size_ + count;
            auto prepared            = ensure_capacity(new_size);
            if (!prepared) {
                return expected<string&, error_code>(unexpect, prepared.error());
            }
            move_chars(data_ + position + count, data_ + position, size_ - position + 1);
            copy_chars(data_ + position, text, count);
            size_ = new_size;
            return *this;
        }

        constexpr expected<string&, error_code> insert(size_type position,
                                                       const_pointer text) noexcept {
            if (text == nullptr) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }
            return insert(position, text, cstrlen(text));
        }

        constexpr expected<string&, error_code> insert(size_type position, size_type count,
                                                       char character) noexcept {
            if (position > size_) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            if (count == 0) {
                return *this;
            }
            if (count > max_size() - size_) {
                return expected<string&, error_code>(unexpect,
                                                     error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            const size_type new_size = size_ + count;
            auto prepared            = ensure_capacity(new_size);
            if (!prepared) {
                return expected<string&, error_code>(unexpect, prepared.error());
            }
            move_chars(data_ + position + count, data_ + position, size_ - position + 1);
            fill_chars(data_ + position, count, character);
            size_ = new_size;
            return *this;
        }

        constexpr expected<string&, error_code> insert(size_type position,
                                                       string_view view) noexcept {
            return insert(position, view.data(), view.size());
        }

        constexpr expected<string&, error_code> insert(size_type position,
                                                       const string& other) noexcept {
            return insert(position, other.data(), other.size());
        }

        constexpr expected<string&, error_code> insert(size_type position, const string& other,
                                                       size_type other_position,
                                                       size_type count = npos) noexcept {
            auto selected = string_view(other).substr(other_position, count);
            if (!selected) {
                return expected<string&, error_code>(unexpect, selected.error());
            }
            return insert(position, *selected);
        }

        constexpr expected<string&, error_code> insert(size_type position, string_view view,
                                                       size_type view_position,
                                                       size_type count = npos) noexcept {
            auto selected = view.substr(view_position, count);
            if (!selected) {
                return expected<string&, error_code>(unexpect, selected.error());
            }
            return insert(position, *selected);
        }

        constexpr expected<iterator, error_code> insert(const_iterator position,
                                                        char character) noexcept {
            if (position < begin() || position > end()) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type index = static_cast<size_type>(position - begin());
            auto result           = insert(index, 1, character);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return begin() + index;
        }

        constexpr expected<iterator, error_code> insert(const_iterator position, size_type count,
                                                        char character) noexcept {
            if (position < begin() || position > end()) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type index = static_cast<size_type>(position - begin());
            auto result           = insert(index, count, character);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return begin() + index;
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt>)
        constexpr expected<iterator, error_code> insert(const_iterator position, InputIt first,
                                                        InputIt last) noexcept {
            if (position < begin() || position > end()) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type index = static_cast<size_type>(position - begin());
            auto temporary        = try_create_range(first, last, allocator_);
            if (!temporary) {
                return expected<iterator, error_code>(unexpect, temporary.error());
            }
            auto result = insert(index, temporary->data(), temporary->size());
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return begin() + index;
        }

        constexpr expected<string&, error_code> erase(size_type position = 0,
                                                      size_type count    = npos) noexcept {
            if (position > size_) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type available = size_ - position;
            const size_type removed   = count < available ? count : available;
            move_chars(data_ + position, data_ + position + removed,
                       size_ - position - removed + 1);
            size_ -= removed;
            return *this;
        }

        constexpr expected<iterator, error_code> erase(const_iterator position) noexcept {
            if (position < begin() || position >= end()) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type index = static_cast<size_type>(position - begin());
            auto result           = erase(index, 1);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return begin() + index;
        }

        constexpr expected<iterator, error_code> erase(const_iterator first,
                                                       const_iterator last) noexcept {
            if (first < begin() || first > last || last > end()) {
                return expected<iterator, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type index = static_cast<size_type>(first - begin());
            auto result           = erase(index, static_cast<size_type>(last - first));
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return begin() + index;
        }

        constexpr expected<void, error_code> push_back(char character) noexcept {
            auto result = insert(size_, 1, character);
            if (!result) {
                return expected<void, error_code>(unexpect, result.error());
            }
            return {};
        }

        constexpr expected<void, error_code> pop_back() noexcept {
            if (empty()) {
                return expected<void, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            --size_;
            data_[size_] = '\0';
            return {};
        }

        constexpr expected<string&, error_code> append(const_pointer text,
                                                       size_type count) noexcept {
            return insert(size_, text, count);
        }

        constexpr expected<string&, error_code> append(const_pointer text) noexcept {
            return insert(size_, text);
        }

        constexpr expected<string&, error_code> append(string_view view) noexcept {
            return insert(size_, view);
        }

        constexpr expected<string&, error_code> append(const string& other) noexcept {
            return insert(size_, other);
        }

        constexpr expected<string&, error_code> append(const string& other, size_type position,
                                                       size_type count = npos) noexcept {
            return insert(size_, other, position, count);
        }

        constexpr expected<string&, error_code> append(string_view view, size_type position,
                                                       size_type count = npos) noexcept {
            auto selected = view.substr(position, count);
            if (!selected) {
                return expected<string&, error_code>(unexpect, selected.error());
            }
            return append(*selected);
        }

        constexpr expected<string&, error_code> append(size_type count, char character) noexcept {
            return insert(size_, count, character);
        }

        constexpr expected<string&, error_code> append(char character) noexcept {
            auto result = push_back(character);
            if (!result) {
                return expected<string&, error_code>(unexpect, result.error());
            }
            return *this;
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt>)
        constexpr expected<string&, error_code> append(InputIt first, InputIt last) noexcept {
            auto result = insert(end(), first, last);
            if (!result) {
                return expected<string&, error_code>(unexpect, result.error());
            }
            return *this;
        }

        constexpr expected<string&, error_code> append(
            std::initializer_list<char> characters) noexcept {
            return append(characters.begin(), characters.end());
        }

        constexpr expected<string&, error_code> operator+=(const string& other) noexcept {
            return append(other);
        }

        constexpr expected<string&, error_code> operator+=(string_view view) noexcept {
            return append(view);
        }

        constexpr expected<string&, error_code> operator+=(const_pointer text) noexcept {
            return append(text);
        }

        constexpr expected<string&, error_code> operator+=(char character) noexcept {
            return append(character);
        }

        constexpr expected<string&, error_code> replace(size_type position, size_type count,
                                                        const_pointer text,
                                                        size_type text_count) noexcept {
            if (position > size_) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            if (text == nullptr && text_count != 0) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }

            const size_type removed = count < size_ - position ? count : size_ - position;
            if (text_count > max_size() - (size_ - removed)) {
                return expected<string&, error_code>(unexpect,
                                                     error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            string replacement(empty_tag{}, allocator_);
            const size_type final_size = size_ - removed + text_count;
            auto reserved              = replacement.reserve(final_size);
            if (!reserved) {
                return expected<string&, error_code>(unexpect, reserved.error());
            }
            auto prefix = replacement.append(data_, position);
            if (!prefix) {
                return expected<string&, error_code>(unexpect, prefix.error());
            }
            auto middle = replacement.append(text, text_count);
            if (!middle) {
                return expected<string&, error_code>(unexpect, middle.error());
            }
            auto suffix =
                replacement.append(data_ + position + removed, size_ - position - removed);
            if (!suffix) {
                return expected<string&, error_code>(unexpect, suffix.error());
            }
            replace_with(std::move(replacement));
            return *this;
        }

        constexpr expected<string&, error_code> replace(size_type position, size_type count,
                                                        const_pointer text) noexcept {
            if (text == nullptr) {
                return expected<string&, error_code>(unexpect, error_code::NULLPTR);
            }
            return replace(position, count, text, cstrlen(text));
        }

        constexpr expected<string&, error_code> replace(size_type position, size_type count,
                                                        string_view view) noexcept {
            return replace(position, count, view.data(), view.size());
        }

        constexpr expected<string&, error_code> replace(size_type position, size_type count,
                                                        const string& other) noexcept {
            return replace(position, count, other.data(), other.size());
        }

        constexpr expected<string&, error_code> replace(size_type position, size_type count,
                                                        size_type replacement_count,
                                                        char character) noexcept {
            auto temporary = try_create(replacement_count, character, allocator_);
            if (!temporary) {
                return expected<string&, error_code>(unexpect, temporary.error());
            }
            return replace(position, count, temporary->data(), temporary->size());
        }

        constexpr expected<string&, error_code> replace(const_iterator first, const_iterator last,
                                                        string_view view) noexcept {
            if (first < begin() || first > last || last > end()) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return replace(static_cast<size_type>(first - begin()),
                           static_cast<size_type>(last - first), view);
        }

        constexpr expected<string&, error_code> replace(const_iterator first, const_iterator last,
                                                        const_pointer text,
                                                        size_type count) noexcept {
            if (first < begin() || first > last || last > end()) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return replace(static_cast<size_type>(first - begin()),
                           static_cast<size_type>(last - first), text, count);
        }

        constexpr expected<string&, error_code> replace(const_iterator first, const_iterator last,
                                                        size_type count, char character) noexcept {
            if (first < begin() || first > last || last > end()) {
                return expected<string&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return replace(static_cast<size_type>(first - begin()),
                           static_cast<size_type>(last - first), count, character);
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> copy(pointer destination, size_type count,
                                                       size_type position = 0) const noexcept {
            if (position > size_) {
                return expected<size_type, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            if (destination == nullptr && count != 0) {
                return expected<size_type, error_code>(unexpect, error_code::NULLPTR);
            }
            const size_type available = size_ - position;
            const size_type copied    = count < available ? count : available;
            copy_chars(destination, data_ + position, copied);
            return copied;
        }

        constexpr expected<void, error_code> resize(size_type count,
                                                    char character = '\0') noexcept {
            if (count <= size_) {
                size_        = count;
                data_[size_] = '\0';
                return {};
            }
            auto prepared = ensure_capacity(count);
            if (!prepared) {
                return prepared;
            }
            fill_chars(data_ + size_, count - size_, character);
            size_        = count;
            data_[size_] = '\0';
            return {};
        }

        template <class Operation>
        constexpr expected<void, error_code> resize_and_overwrite(size_type count,
                                                                  Operation operation) noexcept {
            static_assert(noexcept(operation(data_, count)),
                          "resize_and_overwrite operation must be noexcept");
            auto prepared = ensure_capacity(count);
            if (!prepared) {
                return prepared;
            }
            const size_type written = static_cast<size_type>(operation(data_, count));
            if (written > count) {
                tay::panic("resize_and_overwrite returned an invalid size");
            }
            size_        = written;
            data_[size_] = '\0';
            return {};
        }

        constexpr expected<void, error_code> swap(string& other) noexcept {
            if (this == &other) {
                return {};
            }
            if constexpr (allocator_traits_type::propagate_on_container_swap::value) {
                std::swap(allocator_, other.allocator_);
            } else if constexpr (!allocator_traits_type::is_always_equal::value) {
                if (!(allocator_ == other.allocator_)) {
                    return expected<void, error_code>(unexpect, error_code::INVALID_ARGUMENT);
                }
            }
            swap_storage(other);
            return {};
        }

        [[nodiscard]]
        constexpr operator string_view() const noexcept {
            return string_view(data_, size_);
        }

        [[nodiscard]]
        constexpr int compare(string_view other) const noexcept {
            return string_view(*this).compare(other);
        }

        [[nodiscard]]
        constexpr int compare(const string& other) const noexcept {
            return compare(string_view(other));
        }

        [[nodiscard]]
        constexpr int compare(const_pointer text) const noexcept {
            return compare(string_view(text));
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(size_type position, size_type count,
                                                    string_view other) const noexcept {
            return string_view(*this).compare(position, count, other);
        }

        [[nodiscard]]
        constexpr expected<int, error_code> compare(size_type position, size_type count,
                                                    string_view other, size_type other_position,
                                                    size_type other_count = npos) const noexcept {
            return string_view(*this).compare(position, count, other, other_position, other_count);
        }

        [[nodiscard]]
        constexpr bool starts_with(string_view prefix) const noexcept {
            return string_view(*this).starts_with(prefix);
        }

        [[nodiscard]]
        constexpr bool starts_with(char character) const noexcept {
            return string_view(*this).starts_with(character);
        }

        [[nodiscard]]
        constexpr bool starts_with(const_pointer text) const noexcept {
            return string_view(*this).starts_with(text);
        }

        [[nodiscard]]
        constexpr bool ends_with(string_view suffix) const noexcept {
            return string_view(*this).ends_with(suffix);
        }

        [[nodiscard]]
        constexpr bool ends_with(char character) const noexcept {
            return string_view(*this).ends_with(character);
        }

        [[nodiscard]]
        constexpr bool ends_with(const_pointer text) const noexcept {
            return string_view(*this).ends_with(text);
        }

        [[nodiscard]]
        constexpr bool contains(string_view text) const noexcept {
            return string_view(*this).contains(text);
        }

        [[nodiscard]]
        constexpr bool contains(char character) const noexcept {
            return string_view(*this).contains(character);
        }

        [[nodiscard]]
        constexpr bool contains(const_pointer text) const noexcept {
            return string_view(*this).contains(text);
        }

        [[nodiscard]]
        constexpr size_type find(string_view text, size_type position = 0) const noexcept {
            return string_view(*this).find(text, position);
        }

        [[nodiscard]]
        constexpr size_type find(char character, size_type position = 0) const noexcept {
            return string_view(*this).find(character, position);
        }

        [[nodiscard]]
        constexpr size_type find(const_pointer text, size_type position = 0) const noexcept {
            return string_view(*this).find(text, position);
        }

        [[nodiscard]]
        constexpr size_type rfind(string_view text, size_type position = npos) const noexcept {
            return string_view(*this).rfind(text, position);
        }

        [[nodiscard]]
        constexpr size_type rfind(char character, size_type position = npos) const noexcept {
            return string_view(*this).rfind(character, position);
        }

        [[nodiscard]]
        constexpr size_type rfind(const_pointer text, size_type position = npos) const noexcept {
            return string_view(*this).rfind(text, position);
        }

        [[nodiscard]]
        constexpr size_type find_first_of(string_view characters,
                                          size_type position = 0) const noexcept {
            return string_view(*this).find_first_of(characters, position);
        }

        [[nodiscard]]
        constexpr size_type find_first_of(char character, size_type position = 0) const noexcept {
            return string_view(*this).find_first_of(character, position);
        }

        [[nodiscard]]
        constexpr size_type find_last_of(string_view characters,
                                         size_type position = npos) const noexcept {
            return string_view(*this).find_last_of(characters, position);
        }

        [[nodiscard]]
        constexpr size_type find_last_of(char character, size_type position = npos) const noexcept {
            return string_view(*this).find_last_of(character, position);
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(string_view characters,
                                              size_type position = 0) const noexcept {
            return string_view(*this).find_first_not_of(characters, position);
        }

        [[nodiscard]]
        constexpr size_type find_first_not_of(char character,
                                              size_type position = 0) const noexcept {
            return string_view(*this).find_first_not_of(character, position);
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(string_view characters,
                                             size_type position = npos) const noexcept {
            return string_view(*this).find_last_not_of(characters, position);
        }

        [[nodiscard]]
        constexpr size_type find_last_not_of(char character,
                                             size_type position = npos) const noexcept {
            return string_view(*this).find_last_not_of(character, position);
        }

        [[nodiscard]]
        constexpr expected<string, error_code> substr(size_type position = 0,
                                                      size_type count    = npos) const noexcept {
            if (position > size_) {
                return expected<string, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            const size_type available = size_ - position;
            const size_type selected  = count < available ? count : available;
            return try_create(data_ + position, selected, allocator_);
        }

        [[nodiscard]]
        constexpr bool operator==(string_view other) const noexcept {
            return string_view(*this) == other;
        }

        [[nodiscard]]
        constexpr bool operator==(const string& other) const noexcept {
            return string_view(*this) == string_view(other);
        }

        [[nodiscard]]
        constexpr bool operator==(const_pointer text) const noexcept {
            return string_view(*this) == text;
        }

        [[nodiscard]]
        constexpr std::strong_ordering operator<=>(string_view other) const noexcept {
            return string_view(*this) <=> other;
        }

        [[nodiscard]]
        constexpr std::strong_ordering operator<=>(const string& other) const noexcept {
            return string_view(*this) <=> string_view(other);
        }

        [[nodiscard]]
        constexpr std::strong_ordering operator<=>(const_pointer text) const noexcept {
            return string_view(*this) <=> text;
        }
    };

    template <class Allocator>
    constexpr expected<void, error_code> swap(string<Allocator>& left,
                                              string<Allocator>& right) noexcept {
        return left.swap(right);
    }

    struct string_hash {
        template <class Allocator>
        [[nodiscard]]
        constexpr size_t operator()(const string<Allocator>& text) const noexcept {
            return string_view_hash{}(string_view(text));
        }
    };
}  // namespace tay
