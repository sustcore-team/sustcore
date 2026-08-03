/**
 * @file array.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供支持动态、内联和借用存储的定长数组。
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

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace tay {
    inline constexpr size_t dynamic_extent = size_t(-1);

    template <class T, size_t N, class Allocator = allocator<T>>
    class dynamic_array_storage {
    public:
        using value_type     = T;
        using allocator_type = Allocator;
        using size_type      = size_t;

    private:
        struct empty_tag {};
        [[no_unique_address]] allocator_type allocator_{};
        value_type* data_ = nullptr;
        size_type size_   = 0;

        constexpr dynamic_array_storage(empty_tag, const allocator_type& allocator) noexcept
            : allocator_(allocator) {}

        constexpr void reset() noexcept {
            if (data_ == nullptr) {
                size_ = 0;
                return;
            }
            for (size_type i = 0; i < size_; ++i) {
                allocator_traits<allocator_type>::destroy(allocator_, data_ + i);
            }
            allocator_traits<allocator_type>::deallocate(allocator_, data_, N);
            data_ = nullptr;
            size_ = 0;
        }

    public:
        constexpr dynamic_array_storage() noexcept
            requires(std::is_nothrow_default_constructible_v<allocator_type> &&
                     std::is_nothrow_default_constructible_v<value_type>)
            : dynamic_array_storage(allocator_type{}) {}

        constexpr explicit dynamic_array_storage(const allocator_type& allocator) noexcept
            requires std::is_nothrow_default_constructible_v<value_type>
            : allocator_(allocator) {
            auto created = try_create(allocator_);
            if (!created) {
                tay::panic("array allocation failed");
            }
            *this = std::move(*created);
        }

        constexpr dynamic_array_storage(const dynamic_array_storage& other) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
            : allocator_(other.allocator_) {
            auto memory = allocator_traits<allocator_type>::try_allocate(allocator_, N);
            if (!memory) {
                tay::panic("array allocation failed");
            }
            data_ = *memory;
            for (; size_ < other.size_; ++size_) {
                allocator_traits<allocator_type>::construct(allocator_, data_ + size_,
                                                            other.data_[size_]);
            }
        }

        constexpr dynamic_array_storage(dynamic_array_storage&& other) noexcept
            : allocator_(std::move(other.allocator_)), data_(other.data_), size_(other.size_) {
            other.data_ = nullptr;
            other.size_ = 0;
        }

        constexpr dynamic_array_storage& operator=(dynamic_array_storage&& other) noexcept {
            if (this != &other) {
                reset();
                allocator_  = std::move(other.allocator_);
                data_       = other.data_;
                size_       = other.size_;
                other.data_ = nullptr;
                other.size_ = 0;
            }
            return *this;
        }

        constexpr ~dynamic_array_storage() noexcept {
            reset();
        }

        [[nodiscard]] static constexpr expected<dynamic_array_storage, error_code> try_create(
            const allocator_type& allocator = {}) noexcept
            requires std::is_nothrow_default_constructible_v<value_type>
        {
            dynamic_array_storage result(empty_tag{}, allocator);
            if constexpr (N == 0) {
                return result;
            }
            auto memory = allocator_traits<allocator_type>::try_allocate(result.allocator_, N);
            if (!memory) {
                return expected<dynamic_array_storage, error_code>(unexpect, memory.error());
            }
            result.data_ = *memory;
            for (; result.size_ < N; ++result.size_) {
                allocator_traits<allocator_type>::construct(result.allocator_,
                                                            result.data_ + result.size_);
            }
            return result;
        }

        [[nodiscard]] constexpr value_type* data() noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const value_type* data() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }
    };

    template <class T, size_t N>
    class static_array_storage {
    public:
        using value_type = T;
        using size_type  = size_t;

    private:
        struct zero_storage {};
        using storage_type = std::conditional_t<N == 0, zero_storage, value_type[N]>;
        storage_type values_{};

    public:
        constexpr static_array_storage() noexcept = default;
        [[nodiscard]] constexpr value_type* data() noexcept {
            if constexpr (N == 0) {
                return nullptr;
            } else {
                return values_;
            }
        }
        [[nodiscard]] constexpr const value_type* data() const noexcept {
            if constexpr (N == 0) {
                return nullptr;
            } else {
                return values_;
            }
        }
        [[nodiscard]] static constexpr size_type size() noexcept {
            return N;
        }
    };

    template <class T, size_t N = dynamic_extent>
    class array_view_storage {
    public:
        using value_type = T;
        using size_type  = size_t;

    private:
        value_type* data_ = nullptr;
        size_type size_   = N == dynamic_extent ? 0 : N;

        [[nodiscard]] static constexpr size_type distance(value_type* first,
                                                          value_type* last) noexcept {
            if (first == last) {
                return 0;
            }
            if (first == nullptr || last == nullptr) {
                tay::panic("array_view received an invalid pointer range");
            }
            const auto count = last - first;
            if (count < 0) {
                tay::panic("array_view received a reversed pointer range");
            }
            return static_cast<size_type>(count);
        }

    public:
        constexpr array_view_storage() noexcept
            requires(N == 0 || N == dynamic_extent)
        = default;

        constexpr array_view_storage(value_type* data, size_type count) noexcept
            : data_(data), size_(count) {
            if constexpr (N != dynamic_extent) {
                if (count != N) {
                    tay::panic("array_view extent mismatch");
                }
            }
            if (count != 0 && data == nullptr) {
                tay::panic("array_view received a null data pointer");
            }
        }

        constexpr array_view_storage(value_type* first, value_type* last) noexcept
            : array_view_storage(first, distance(first, last)) {}

        template <size_t M>
            requires(N == dynamic_extent || M == N)
        constexpr array_view_storage(value_type (&values)[M]) noexcept : data_(values), size_(M) {}

        [[nodiscard]] constexpr value_type* data() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
    };

    template <class Storage, class T>
    concept array_storage = requires(Storage& storage, const Storage& const_storage) {
        {
            storage.data()
        } -> std::convertible_to<T*>;
        {
            const_storage.size()
        } -> std::convertible_to<size_t>;
    };

    template <class T, size_t N, class Storage>
        requires array_storage<Storage, T>
    class basic_array {
    public:
        using value_type                  = std::remove_cv_t<T>;
        using element_type                = T;
        using storage_type                = Storage;
        using size_type                   = size_t;
        using difference_type             = std::ptrdiff_t;
        using reference                   = element_type&;
        using const_reference             = const element_type&;
        using pointer                     = element_type*;
        using const_pointer               = const element_type*;
        using iterator                    = pointer;
        using const_iterator              = const_pointer;
        static constexpr size_type extent = N;

    private:
        storage_type storage_;

        constexpr explicit basic_array(storage_type storage) noexcept
            : storage_(std::move(storage)) {}

    public:
        constexpr basic_array() noexcept
            requires std::is_nothrow_default_constructible_v<storage_type>
        = default;

        template <class... Args>
            requires std::constructible_from<storage_type, Args&&...>
        constexpr explicit basic_array(Args&&... args) noexcept(
            std::is_nothrow_constructible_v<storage_type, Args&&...>)
            : storage_(std::forward<Args>(args)...) {}

        [[nodiscard]] static constexpr expected<basic_array, error_code> try_create() noexcept
            requires requires { storage_type::try_create(); }
        {
            auto storage = storage_type::try_create();
            if (!storage) {
                return expected<basic_array, error_code>(unexpect, storage.error());
            }
            return basic_array(std::move(*storage));
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            return storage_.data();
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return storage_.data();
        }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return begin();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return size() == 0 ? begin() : begin() + size();
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return size() == 0 ? begin() : begin() + size();
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
        [[nodiscard]] constexpr size_type size() const noexcept {
            return storage_.size();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        }
        [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
            return data()[index];
        }
        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept {
            return data()[index];
        }
        [[nodiscard]] constexpr expected<reference, error_code> at(size_type index) noexcept {
            if (index >= size()) {
                return expected<reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return data()[index];
        }
        [[nodiscard]] constexpr expected<const_reference, error_code> at(
            size_type index) const noexcept {
            if (index >= size()) {
                return expected<const_reference, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return data()[index];
        }
        [[nodiscard]] constexpr reference front() noexcept {
            return data()[0];
        }
        [[nodiscard]] constexpr const_reference front() const noexcept {
            return data()[0];
        }
        [[nodiscard]] constexpr reference back() noexcept {
            return data()[size() - 1];
        }
        [[nodiscard]] constexpr const_reference back() const noexcept {
            return data()[size() - 1];
        }

        template <class Function>
            requires std::invocable<Function&, reference>
        constexpr void foreach (Function&& function) noexcept(
            std::is_nothrow_invocable_v<Function&, reference>) {
            for (auto& element : *this) {
                std::invoke(function, element);
            }
        }

        template <class Function>
            requires std::invocable<Function&, const_reference>
        constexpr void foreach (Function&& function) const
            noexcept(std::is_nothrow_invocable_v<Function&, const_reference>) {
            for (const auto& element : *this) {
                std::invoke(function, element);
            }
        }

        constexpr void fill(const value_type& value) noexcept(
            std::is_nothrow_copy_assignable_v<value_type>)
            requires(!std::is_const_v<element_type>)
        {
            for (auto& element : *this) {
                element = value;
            }
        }

        [[nodiscard]] constexpr auto get_allocator() const noexcept
            requires requires { storage_.get_allocator(); }
        {
            return storage_.get_allocator();
        }
    };

    template <class T, size_t N, class Allocator = allocator<T>>
    using array = basic_array<T, N, dynamic_array_storage<T, N, Allocator>>;

    template <class T, size_t N>
    using static_array = basic_array<T, N, static_array_storage<T, N>>;

    template <class T, size_t N = dynamic_extent>
    using array_view = basic_array<T, N, array_view_storage<T, N>>;

    template <class T, size_t N, class S1, class S2>
    [[nodiscard]] constexpr bool operator==(const basic_array<T, N, S1>& left,
                                            const basic_array<T, N, S2>& right) {
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
