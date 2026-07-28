/**
 * @file list.h
 * @brief Exception-free contiguous dynamic arrays.
 */

#pragma once

#include <tay/allocator.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <compare>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    template <class T, class Allocator>
    class array_list {
    public:
        using value_type            = T;
        using allocator_type        = Allocator;
        using allocator_traits_type = allocator_traits<allocator_type>;
        using size_type             = typename allocator_traits_type::size_type;
        using difference_type = typename allocator_traits_type::difference_type;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using pointer         = typename allocator_traits_type::pointer;
        using const_pointer   = typename allocator_traits_type::const_pointer;
        using iterator        = pointer;
        using const_iterator  = const_pointer;

    private:
        struct empty_tag {};

        static_assert(std::is_same_v<typename allocator_traits_type::value_type,
                                     value_type>,
                      "tay::array_list allocator value_type must match T");
        static_assert(std::is_same_v<pointer, value_type*>,
                      "tay::array_list currently requires raw pointers");
        static_assert(std::is_nothrow_destructible_v<value_type>,
                      "tay::array_list requires a nothrow destructor");
        static_assert(std::is_nothrow_move_constructible_v<value_type>,
                      "tay::array_list requires nothrow move construction");
        static_assert(std::is_nothrow_move_assignable_v<value_type>,
                      "tay::array_list requires nothrow move assignment");

        static constexpr bool has_default_allocator =
            std::is_nothrow_default_constructible_v<allocator_type>;

        [[no_unique_address]] allocator_type allocator_;
        pointer data_       = nullptr;
        size_type size_     = 0;
        size_type capacity_ = 0;

        constexpr explicit array_list(empty_tag,
                                      const allocator_type& allocator) noexcept
            : allocator_(allocator) {}

        constexpr explicit array_list(empty_tag,
                                      allocator_type&& allocator) noexcept
            : allocator_(std::move(allocator)) {}

        [[noreturn]] static constexpr void panic_error(
            error_code error) noexcept {
            switch (error) {
                case error_code::OUT_OF_MEMORY:
                    tay::panic("array_list allocation failed");
                case error_code::ALLOCATION_SIZE_OVERFLOW:
                    tay::panic("array_list size overflow");
                case error_code::OUT_OF_RANGE:
                    tay::panic("array_list position out of range");
                case error_code::INVALID_ARGUMENT:
                    tay::panic("array_list invalid argument");
                default: tay::panic("array_list operation failed");
            }
        }

        static constexpr void panic_if_error(
            const expected<void, error_code>& result) noexcept {
            if (!result) {
                panic_error(result.error());
            }
        }

        constexpr void destroy_range(pointer first, pointer last) noexcept {
            while (first != last) {
                allocator_traits_type::destroy(allocator_, first);
                ++first;
            }
        }

        constexpr void reset() noexcept {
            if (data_ != nullptr) {
                destroy_range(data_, data_ + size_);
                allocator_traits_type::deallocate(allocator_, data_, capacity_);
            }
            data_ = nullptr;
            size_ = capacity_ = 0;
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> next_capacity(
            size_type required) const noexcept {
            if (required > max_size()) {
                return expected<size_type, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            if (capacity_ >= required) {
                return capacity_;
            }
            if (capacity_ == 0) {
                return required > 1 ? required : 1;
            }
            if (capacity_ > max_size() - capacity_) {
                return max_size();
            }
            const size_type doubled = capacity_ + capacity_;
            return doubled < required ? required : doubled;
        }

        constexpr expected<void, error_code> reallocate(
            size_type new_capacity) noexcept {
            if (new_capacity == capacity_) {
                return {};
            }
            if (new_capacity < size_ || new_capacity > max_size()) {
                return expected<void, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            pointer new_data = nullptr;
            if (new_capacity != 0) {
                auto allocation = allocator_traits_type::try_allocate(
                    allocator_, new_capacity);
                if (!allocation) {
                    return expected<void, error_code>(unexpect,
                                                      allocation.error());
                }
                new_data = *allocation;
            }

            for (size_type index = 0; index < size_; ++index) {
                allocator_traits_type::construct(allocator_, new_data + index,
                                                 std::move(data_[index]));
            }
            if (data_ != nullptr) {
                destroy_range(data_, data_ + size_);
                allocator_traits_type::deallocate(allocator_, data_, capacity_);
            }
            data_     = new_data;
            capacity_ = new_capacity;
            return {};
        }

        constexpr expected<void, error_code> ensure_capacity(
            size_type required) noexcept {
            if (required <= capacity_) {
                return {};
            }
            auto grown = next_capacity(required);
            if (!grown) {
                return expected<void, error_code>(unexpect, grown.error());
            }
            return reallocate(*grown);
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> checked_add(
            size_type left, size_type right) const noexcept {
            if (right > max_size() - left) {
                return expected<size_type, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            return left + right;
        }

        [[nodiscard]]
        constexpr expected<size_type, error_code> position_of(
            const_iterator position, bool allow_end = true) const noexcept {
            if (data_ == nullptr) {
                if (position == nullptr && allow_end) {
                    return size_type{0};
                }
                return expected<size_type, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            for (size_type index = 0; index < size_; ++index) {
                if (position == data_ + index) {
                    return index;
                }
            }
            if (allow_end && position == data_ + size_) {
                return size_;
            }
            return expected<size_type, error_code>(unexpect,
                                                   error_code::OUT_OF_RANGE);
        }

        constexpr void shift_right(size_type index, size_type count) noexcept {
            const size_type old_size = size_;
            for (size_type cursor = old_size; cursor > index; --cursor) {
                const size_type source = cursor - 1;
                const size_type target = source + count;
                if (target >= old_size) {
                    allocator_traits_type::construct(allocator_, data_ + target,
                                                     std::move(data_[source]));
                } else {
                    data_[target] = std::move(data_[source]);
                }
            }
        }

        template <class Source>
        constexpr void fill_gap(size_type index, size_type count,
                                Source&& source) noexcept {
            const size_type old_size = size_;
            for (size_type offset = 0; offset < count; ++offset) {
                const size_type target = index + offset;
                if (target < old_size) {
                    data_[target] = source(offset);
                } else {
                    allocator_traits_type::construct(allocator_, data_ + target,
                                                     source(offset));
                }
            }
            size_ = old_size + count;
        }

        template <class InputIt>
        static constexpr expected<array_list, error_code> create_range(
            InputIt first, InputIt last,
            const allocator_type& allocator) noexcept {
            array_list result(empty_tag{}, allocator);
            for (; first != last; ++first) {
                auto appended = result.push_back(*first);
                if (!appended) {
                    return expected<array_list, error_code>(unexpect,
                                                            appended.error());
                }
            }
            return result;
        }

        static constexpr expected<array_list, error_code> create_move_range(
            pointer first, pointer last,
            const allocator_type& allocator) noexcept {
            array_list result(empty_tag{}, allocator);
            for (; first != last; ++first) {
                auto appended = result.push_back(std::move(*first));
                if (!appended) {
                    return expected<array_list, error_code>(unexpect,
                                                            appended.error());
                }
            }
            return result;
        }

        constexpr void take_storage(array_list&& other) noexcept {
            data_       = other.data_;
            size_       = other.size_;
            capacity_   = other.capacity_;
            other.data_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }

        constexpr void replace_storage(array_list&& other) noexcept {
            reset();
            take_storage(std::move(other));
        }

    public:
        constexpr array_list() noexcept
            requires(has_default_allocator)
            : allocator_{} {}

        constexpr explicit array_list(const allocator_type& allocator) noexcept
            : allocator_(allocator) {}

        constexpr explicit array_list(size_type count) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_default_constructible_v<value_type>)
            : array_list(count, allocator_type{}) {}

        constexpr array_list(size_type count,
                             const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<value_type>)
            : allocator_(allocator) {
            auto created = try_create(count, allocator_);
            if (!created) {
                panic_error(created.error());
            }
            take_storage(std::move(*created));
        }

        constexpr array_list(size_type count, const_reference value) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_copy_constructible_v<value_type>)
            : array_list(count, value, allocator_type{}) {}

        constexpr array_list(size_type count, const_reference value,
                             const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : allocator_(allocator) {
            auto created = try_create(count, value, allocator_);
            if (!created) {
                panic_error(created.error());
            }
            take_storage(std::move(*created));
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> && has_default_allocator &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        constexpr array_list(InputIt first, InputIt last) noexcept
            : array_list(first, last, allocator_type{}) {}

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        constexpr array_list(InputIt first, InputIt last,
                             const allocator_type& allocator) noexcept
            : allocator_(allocator) {
            auto created = create_range(first, last, allocator_);
            if (!created) {
                panic_error(created.error());
            }
            take_storage(std::move(*created));
        }

        constexpr array_list(std::initializer_list<value_type> values) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_copy_constructible_v<value_type>)
            : array_list(values, allocator_type{}) {}

        constexpr array_list(std::initializer_list<value_type> values,
                             const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : array_list(values.begin(), values.end(), allocator) {}

        constexpr array_list(const array_list& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : allocator_(
                  allocator_traits_type::select_on_container_copy_construction(
                      other.allocator_)) {
            auto created = try_create(other, allocator_);
            if (!created) {
                panic_error(created.error());
            }
            take_storage(std::move(*created));
        }

        constexpr array_list(const array_list& other,
                             const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : allocator_(allocator) {
            auto created = try_create(other, allocator_);
            if (!created) {
                panic_error(created.error());
            }
            take_storage(std::move(*created));
        }

        constexpr array_list(array_list&& other) noexcept
            : allocator_(std::move(other.allocator_)) {
            take_storage(std::move(other));
        }

        constexpr array_list(array_list&& other,
                             const allocator_type& allocator) noexcept
            : allocator_(allocator) {
            if constexpr (allocator_traits_type::is_always_equal::value) {
                take_storage(std::move(other));
            } else if (allocator_ == other.allocator_) {
                take_storage(std::move(other));
            } else {
                auto created =
                    create_move_range(other.begin(), other.end(), allocator_);
                if (!created) {
                    panic_error(created.error());
                }
                take_storage(std::move(*created));
                other.clear();
            }
        }

        constexpr ~array_list() noexcept {
            reset();
        }

        static constexpr expected<array_list, error_code> try_create() noexcept
            requires(has_default_allocator)
        {
            return array_list(empty_tag{}, allocator_type{});
        }

        static constexpr expected<array_list, error_code> try_create(
            const allocator_type& allocator) noexcept {
            return array_list(empty_tag{}, allocator);
        }

        static constexpr expected<array_list, error_code> try_create(
            size_type count) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_default_constructible_v<value_type>)
        {
            return try_create(count, allocator_type{});
        }

        static constexpr expected<array_list, error_code> try_create(
            size_type count, const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<value_type>)
        {
            array_list result(empty_tag{}, allocator);
            auto reserved = result.reserve(count);
            if (!reserved) {
                return expected<array_list, error_code>(unexpect,
                                                        reserved.error());
            }
            while (result.size_ < count) {
                allocator_traits_type::construct(result.allocator_,
                                                 result.data_ + result.size_);
                ++result.size_;
            }
            return result;
        }

        static constexpr expected<array_list, error_code> try_create(
            size_type count, const_reference value) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_copy_constructible_v<value_type>)
        {
            return try_create(count, value, allocator_type{});
        }

        static constexpr expected<array_list, error_code> try_create(
            size_type count, const_reference value,
            const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            array_list result(empty_tag{}, allocator);
            auto reserved = result.reserve(count);
            if (!reserved) {
                return expected<array_list, error_code>(unexpect,
                                                        reserved.error());
            }
            while (result.size_ < count) {
                allocator_traits_type::construct(
                    result.allocator_, result.data_ + result.size_, value);
                ++result.size_;
            }
            return result;
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> && has_default_allocator &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        static constexpr expected<array_list, error_code> try_create(
            InputIt first, InputIt last) noexcept {
            return create_range(first, last, allocator_type{});
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        static constexpr expected<array_list, error_code> try_create(
            InputIt first, InputIt last,
            const allocator_type& allocator) noexcept {
            return create_range(first, last, allocator);
        }

        static constexpr expected<array_list, error_code> try_create(
            std::initializer_list<value_type> values) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_copy_constructible_v<value_type>)
        {
            return create_range(values.begin(), values.end(), allocator_type{});
        }

        static constexpr expected<array_list, error_code> try_create(
            std::initializer_list<value_type> values,
            const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return create_range(values.begin(), values.end(), allocator);
        }

        static constexpr expected<array_list, error_code> try_create(
            const array_list& other) noexcept
            requires(has_default_allocator &&
                     std::is_nothrow_copy_constructible_v<value_type>)
        {
            return create_range(other.begin(), other.end(), allocator_type{});
        }

        static constexpr expected<array_list, error_code> try_create(
            const array_list& other, const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return create_range(other.begin(), other.end(), allocator);
        }

        constexpr array_list& operator=(const array_list& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            if (this == &other) {
                return *this;
            }
            if constexpr (allocator_traits_type::
                              propagate_on_container_copy_assignment::value)
            {
                auto created = try_create(other, other.allocator_);
                if (!created) {
                    panic_error(created.error());
                }
                reset();
                allocator_ = other.allocator_;
                take_storage(std::move(*created));
            } else {
                auto created = try_create(other, allocator_);
                if (!created) {
                    panic_error(created.error());
                }
                replace_storage(std::move(*created));
            }
            return *this;
        }

        constexpr array_list& operator=(array_list&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            if constexpr (allocator_traits_type::
                              propagate_on_container_move_assignment::value)
            {
                reset();
                allocator_ = std::move(other.allocator_);
                take_storage(std::move(other));
            } else if constexpr (allocator_traits_type::is_always_equal::value)
            {
                replace_storage(std::move(other));
            } else if (allocator_ == other.allocator_) {
                replace_storage(std::move(other));
            } else {
                auto created =
                    create_move_range(other.begin(), other.end(), allocator_);
                if (!created) {
                    panic_error(created.error());
                }
                replace_storage(std::move(*created));
                other.clear();
            }
            return *this;
        }

        constexpr array_list& operator=(
            std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto result = assign(values);
            panic_if_error(result);
            return *this;
        }

        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }

        [[nodiscard]] constexpr expected<reference, error_code> at(
            size_type position) noexcept {
            if (position >= size_) {
                return expected<reference, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            return data_[position];
        }

        [[nodiscard]] constexpr expected<const_reference, error_code> at(
            size_type position) const noexcept {
            if (position >= size_) {
                return expected<const_reference, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            return data_[position];
        }

        [[nodiscard]] constexpr reference operator[](
            size_type position) noexcept {
            return data_[position];
        }
        [[nodiscard]] constexpr const_reference operator[](
            size_type position) const noexcept {
            return data_[position];
        }
        [[nodiscard]] constexpr reference front() noexcept {
            return data_[0];
        }
        [[nodiscard]] constexpr const_reference front() const noexcept {
            return data_[0];
        }
        [[nodiscard]] constexpr reference back() noexcept {
            return data_[size_ - 1];
        }
        [[nodiscard]] constexpr const_reference back() const noexcept {
            return data_[size_ - 1];
        }
        [[nodiscard]] constexpr pointer data() noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const_pointer data() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr iterator begin() noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return data_ == nullptr ? nullptr : data_ + size_;
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return data_ == nullptr ? nullptr : data_ + size_;
        }
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return data_ == nullptr ? nullptr : data_ + size_;
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr size_type capacity() const noexcept {
            return capacity_;
        }
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return allocator_traits_type::max_size(allocator_);
        }

        constexpr expected<void, error_code> reserve(
            size_type new_capacity) noexcept {
            if (new_capacity > max_size()) {
                return expected<void, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            return new_capacity > capacity_ ? reallocate(new_capacity)
                                            : expected<void, error_code>{};
        }

        constexpr expected<void, error_code> shrink_to_fit() noexcept {
            return size_ == capacity_ ? expected<void, error_code>{}
                                      : reallocate(size_);
        }

        constexpr void clear() noexcept {
            if (data_ != nullptr) {
                destroy_range(data_, data_ + size_);
            }
            size_ = 0;
        }

        constexpr expected<void, error_code> assign(
            size_type count, const_reference value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto created = try_create(count, value, allocator_);
            if (!created) {
                return expected<void, error_code>(unexpect, created.error());
            }
            replace_storage(std::move(*created));
            return {};
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        constexpr expected<void, error_code> assign(InputIt first,
                                                    InputIt last) noexcept {
            auto created = create_range(first, last, allocator_);
            if (!created) {
                return expected<void, error_code>(unexpect, created.error());
            }
            replace_storage(std::move(*created));
            return {};
        }

        constexpr expected<void, error_code> assign(
            std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return assign(values.begin(), values.end());
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...>)
        constexpr expected<iterator, error_code> emplace(
            const_iterator position, Args&&... args) noexcept {
            auto index = position_of(position);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            auto final_size = checked_add(size_, 1);
            if (!final_size) {
                return expected<iterator, error_code>(unexpect,
                                                      final_size.error());
            }
            value_type value(std::forward<Args>(args)...);
            auto reserved = ensure_capacity(*final_size);
            if (!reserved) {
                return expected<iterator, error_code>(unexpect,
                                                      reserved.error());
            }
            shift_right(*index, 1);
            fill_gap(*index, 1, [&](size_type) noexcept -> value_type&& {
                return std::move(value);
            });
            return data_ + *index;
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator position, const_reference value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return emplace(position, value);
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator position, value_type&& value) noexcept {
            return emplace(position, std::move(value));
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator position, size_type count,
            const_reference value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_copy_assignable_v<value_type>)
        {
            auto index = position_of(position);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            if (count == 0) {
                return data_ == nullptr ? nullptr : data_ + *index;
            }
            auto final_size = checked_add(size_, count);
            if (!final_size) {
                return expected<iterator, error_code>(unexpect,
                                                      final_size.error());
            }
            value_type copy(value);
            auto reserved = ensure_capacity(*final_size);
            if (!reserved) {
                return expected<iterator, error_code>(unexpect,
                                                      reserved.error());
            }
            shift_right(*index, count);
            fill_gap(*index, count, [&](size_type) noexcept -> const_reference {
                return copy;
            });
            return data_ + *index;
        }

        template <class InputIt>
            requires(!std::is_integral_v<InputIt> &&
                     std::is_nothrow_constructible_v<
                         value_type, decltype(*std::declval<InputIt&>())>)
        constexpr expected<iterator, error_code> insert(const_iterator position,
                                                        InputIt first,
                                                        InputIt last) noexcept {
            auto index = position_of(position);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            auto incoming = create_range(first, last, allocator_);
            if (!incoming) {
                return expected<iterator, error_code>(unexpect,
                                                      incoming.error());
            }
            const size_type count = incoming->size();
            if (count == 0) {
                return data_ == nullptr ? nullptr : data_ + *index;
            }
            auto final_size = checked_add(size_, count);
            if (!final_size) {
                return expected<iterator, error_code>(unexpect,
                                                      final_size.error());
            }
            auto reserved = ensure_capacity(*final_size);
            if (!reserved) {
                return expected<iterator, error_code>(unexpect,
                                                      reserved.error());
            }
            shift_right(*index, count);
            fill_gap(*index, count,
                     [&](size_type offset) noexcept -> value_type&& {
                         return std::move((*incoming)[offset]);
                     });
            return data_ + *index;
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator position,
            std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return insert(position, values.begin(), values.end());
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...>)
        constexpr expected<reference, error_code> emplace_back(
            Args&&... args) noexcept {
            auto inserted = emplace(end(), std::forward<Args>(args)...);
            if (!inserted) {
                return expected<reference, error_code>(unexpect,
                                                       inserted.error());
            }
            return **inserted;
        }

        constexpr expected<void, error_code> push_back(
            const_reference value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto result = emplace(end(), value);
            if (!result) {
                return expected<void, error_code>(unexpect, result.error());
            }
            return {};
        }

        constexpr expected<void, error_code> push_back(
            value_type&& value) noexcept {
            auto result = emplace(end(), std::move(value));
            if (!result) {
                return expected<void, error_code>(unexpect, result.error());
            }
            return {};
        }

        constexpr expected<void, error_code> pop_back() noexcept {
            if (empty()) {
                return expected<void, error_code>(unexpect,
                                                  error_code::OUT_OF_RANGE);
            }
            --size_;
            allocator_traits_type::destroy(allocator_, data_ + size_);
            return {};
        }

        constexpr expected<iterator, error_code> erase(
            const_iterator position) noexcept {
            auto index = position_of(position, false);
            if (!index) {
                return expected<iterator, error_code>(unexpect, index.error());
            }
            return erase(position, position + 1);
        }

        constexpr expected<iterator, error_code> erase(
            const_iterator first, const_iterator last) noexcept {
            auto first_index = position_of(first);
            auto last_index  = position_of(last);
            if (!first_index || !last_index || *last_index < *first_index) {
                return expected<iterator, error_code>(unexpect,
                                                      error_code::OUT_OF_RANGE);
            }
            const size_type count = *last_index - *first_index;
            if (count == 0) {
                return end();
            }
            for (size_type index = *first_index; index + count < size_; ++index)
            {
                data_[index] = std::move(data_[index + count]);
            }
            destroy_range(data_ + size_ - count, data_ + size_);
            size_ -= count;
            return data_ == nullptr ? nullptr : data_ + *first_index;
        }

        constexpr expected<void, error_code> resize(size_type count) noexcept
            requires(std::is_nothrow_default_constructible_v<value_type>)
        {
            if (count < size_) {
                destroy_range(data_ + count, data_ + size_);
                size_ = count;
                return {};
            }
            auto reserved = ensure_capacity(count);
            if (!reserved) {
                return reserved;
            }
            while (size_ < count) {
                allocator_traits_type::construct(allocator_, data_ + size_);
                ++size_;
            }
            return {};
        }

        constexpr expected<void, error_code> resize(
            size_type count, const_reference value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            if (count < size_) {
                destroy_range(data_ + count, data_ + size_);
                size_ = count;
                return {};
            }
            value_type copy(value);
            auto reserved = ensure_capacity(count);
            if (!reserved) {
                return reserved;
            }
            while (size_ < count) {
                allocator_traits_type::construct(allocator_, data_ + size_,
                                                 copy);
                ++size_;
            }
            return {};
        }

        constexpr expected<void, error_code> swap(array_list& other) noexcept {
            if (this == &other) {
                return {};
            }
            if constexpr (allocator_traits_type::propagate_on_container_swap::
                              value)
            {
                using std::swap;
                swap(allocator_, other.allocator_);
            } else if constexpr (!allocator_traits_type::is_always_equal::value)
            {
                if (allocator_ != other.allocator_) {
                    return expected<void, error_code>(
                        unexpect, error_code::INVALID_ARGUMENT);
                }
            }
            using std::swap;
            swap(data_, other.data_);
            swap(size_, other.size_);
            swap(capacity_, other.capacity_);
            return {};
        }
    };

    template <class T, class Allocator>
    constexpr bool operator==(const array_list<T, Allocator>& left,
                              const array_list<T, Allocator>& right) noexcept {
        if (left.size() != right.size()) {
            return false;
        }
        for (typename array_list<T, Allocator>::size_type index = 0;
             index < left.size(); ++index)
        {
            if (!(left[index] == right[index])) {
                return false;
            }
        }
        return true;
    }

    template <class T, class Allocator>
        requires requires(const T& left, const T& right) { left <=> right; }
    constexpr auto operator<=>(const array_list<T, Allocator>& left,
                               const array_list<T, Allocator>& right) noexcept {
        using result_type =
            decltype(std::declval<const T&>() <=> std::declval<const T&>());
        const auto count =
            left.size() < right.size() ? left.size() : right.size();
        for (typename array_list<T, Allocator>::size_type index = 0;
             index < count; ++index)
        {
            const auto result = left[index] <=> right[index];
            if (result != 0) {
                return result;
            }
        }
        return left.size() < right.size()   ? result_type::less
               : left.size() > right.size() ? result_type::greater
                                            : result_type::equivalent;
    }

    template <class T, class Allocator>
    constexpr expected<void, error_code> swap(
        array_list<T, Allocator>& left,
        array_list<T, Allocator>& right) noexcept {
        return left.swap(right);
    }

    template <class T, class Allocator>
    using list = array_list<T, Allocator>;
}  // namespace tay
