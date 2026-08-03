/**
 * @file fifo.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供基于存储策略的环形 FIFO 容器。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/allocator.h>
#include <tay/array.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace tay {
    template <class T>
    struct fifo_segments {
        array_view<T> first;
        array_view<T> second;
    };

    template <class T, class Allocator = allocator<T>>
    class dynamic_fifo_storage {
    public:
        using value_type                     = T;
        using allocator_type                 = Allocator;
        using size_type                      = size_t;
        static constexpr bool fixed_capacity = false;

    private:
        [[no_unique_address]] allocator_type allocator_{};
        value_type* data_   = nullptr;
        size_type capacity_ = 0;

    public:
        constexpr dynamic_fifo_storage() noexcept                    = default;
        dynamic_fifo_storage(const dynamic_fifo_storage&)            = delete;
        dynamic_fifo_storage& operator=(const dynamic_fifo_storage&) = delete;
        constexpr dynamic_fifo_storage(dynamic_fifo_storage&& other) noexcept
            : allocator_(std::move(other.allocator_)),
              data_(other.data_),
              capacity_(other.capacity_) {
            other.data_     = nullptr;
            other.capacity_ = 0;
        }
        constexpr dynamic_fifo_storage& operator=(dynamic_fifo_storage&& other) noexcept {
            if (this != &other) {
                release();
                allocator_      = std::move(other.allocator_);
                data_           = other.data_;
                capacity_       = other.capacity_;
                other.data_     = nullptr;
                other.capacity_ = 0;
            }
            return *this;
        }
        constexpr ~dynamic_fifo_storage() noexcept {
            release();
        }

        [[nodiscard]] static constexpr expected<dynamic_fifo_storage, error_code> try_create(
            size_type capacity, const allocator_type& allocator = {}) noexcept {
            dynamic_fifo_storage result;
            result.allocator_ = allocator;
            if (capacity == 0) {
                return result;
            }
            auto memory =
                allocator_traits<allocator_type>::try_allocate(result.allocator_, capacity);
            if (!memory) {
                return expected<dynamic_fifo_storage, error_code>(unexpect, memory.error());
            }
            result.data_     = *memory;
            result.capacity_ = capacity;
            return result;
        }

        [[nodiscard]] constexpr value_type* data() noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const value_type* data() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr size_type capacity() const noexcept {
            return capacity_;
        }
        [[nodiscard]] constexpr allocator_type& allocator_ref() noexcept {
            return allocator_;
        }
        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }

    private:
        constexpr void release() noexcept {
            if (data_ != nullptr) {
                allocator_traits<allocator_type>::deallocate(allocator_, data_, capacity_);
            }
            data_     = nullptr;
            capacity_ = 0;
        }
    };

    template <class T, size_t N>
    class static_fifo_storage {
    public:
        using value_type                     = T;
        using size_type                      = size_t;
        static constexpr bool fixed_capacity = true;

    private:
        struct slot_type {
            alignas(value_type) unsigned char bytes[sizeof(value_type)];
        };
        slot_type slots_[N == 0 ? 1 : N];

    public:
        [[nodiscard]] constexpr value_type* data() noexcept {
            if constexpr (N == 0) {
                return nullptr;
            }
            return std::launder(reinterpret_cast<value_type*>(slots_));
        }
        [[nodiscard]] constexpr const value_type* data() const noexcept {
            if constexpr (N == 0) {
                return nullptr;
            }
            return std::launder(reinterpret_cast<const value_type*>(slots_));
        }
        [[nodiscard]] static constexpr size_type capacity() noexcept {
            return N;
        }
    };

    template <class Storage, class T>
    concept fifo_storage = requires(Storage& storage, const Storage& const_storage) {
        {
            storage.data()
        } -> std::convertible_to<T*>;
        {
            const_storage.capacity()
        } -> std::convertible_to<size_t>;
    };

    template <class T, class Storage>
        requires fifo_storage<Storage, T>
    class basic_fifo {
    public:
        using value_type      = T;
        using storage_type    = Storage;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = value_type&;
        using const_reference = const value_type&;

    private:
        storage_type storage_;
        size_type head_ = 0;
        size_type size_ = 0;

        constexpr explicit basic_fifo(storage_type storage) noexcept
            : storage_(std::move(storage)) {}

        [[nodiscard]] constexpr size_type physical(size_type logical) const noexcept {
            return capacity() == 0 ? 0 : (head_ + logical) % capacity();
        }
        [[nodiscard]] constexpr value_type* ptr(size_type logical) noexcept {
            return storage_.data() + physical(logical);
        }
        [[nodiscard]] constexpr const value_type* ptr(size_type logical) const noexcept {
            return storage_.data() + physical(logical);
        }

    public:
        template <bool Constant>
        class basic_iterator {
            friend class basic_fifo;
            using owner_type   = std::conditional_t<Constant, const basic_fifo, basic_fifo>;
            owner_type* owner_ = nullptr;
            size_type offset_  = 0;
            constexpr basic_iterator(owner_type* owner, size_type offset) noexcept
                : owner_(owner), offset_(offset) {}

        public:
            using iterator_category             = std::forward_iterator_tag;
            using iterator_concept              = std::forward_iterator_tag;
            using value_type                    = T;
            using difference_type               = std::ptrdiff_t;
            using reference                     = std::conditional_t<Constant, const T&, T&>;
            using pointer                       = std::conditional_t<Constant, const T*, T*>;
            constexpr basic_iterator() noexcept = default;
            [[nodiscard]] constexpr reference operator*() const noexcept {
                return *owner_->ptr(offset_);
            }
            [[nodiscard]] constexpr pointer operator->() const noexcept {
                return owner_->ptr(offset_);
            }
            constexpr basic_iterator& operator++() noexcept {
                ++offset_;
                return *this;
            }
            constexpr basic_iterator operator++(int) noexcept {
                auto copy = *this;
                ++*this;
                return copy;
            }
            friend constexpr bool operator==(const basic_iterator& left,
                                             const basic_iterator& right) noexcept {
                return left.owner_ == right.owner_ && left.offset_ == right.offset_;
            }
        };

        using iterator       = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

        constexpr basic_fifo() noexcept
            requires std::is_nothrow_default_constructible_v<storage_type>
        = default;

        constexpr explicit basic_fifo(size_type capacity) noexcept
            requires requires { storage_type::try_create(capacity); }
        {
            auto storage = storage_type::try_create(capacity);
            if (!storage) {
                tay::panic("fifo allocation failed");
            }
            storage_ = std::move(*storage);
        }
        basic_fifo(const basic_fifo&)            = delete;
        basic_fifo& operator=(const basic_fifo&) = delete;
        constexpr basic_fifo(basic_fifo&& other) noexcept
            requires std::is_nothrow_move_constructible_v<storage_type>
            : storage_(std::move(other.storage_)), head_(other.head_), size_(other.size_) {
            other.head_ = other.size_ = 0;
        }
        constexpr basic_fifo& operator=(basic_fifo&& other) noexcept
            requires std::is_nothrow_move_assignable_v<storage_type>
        {
            if (this != &other) {
                clear();
                storage_    = std::move(other.storage_);
                head_       = other.head_;
                size_       = other.size_;
                other.head_ = other.size_ = 0;
            }
            return *this;
        }
        constexpr ~basic_fifo() noexcept {
            clear();
        }

        [[nodiscard]] static constexpr expected<basic_fifo, error_code> try_create(
            size_type capacity) noexcept
            requires requires { storage_type::try_create(capacity); }
        {
            auto storage = storage_type::try_create(capacity);
            if (!storage) {
                return expected<basic_fifo, error_code>(unexpect, storage.error());
            }
            return basic_fifo(std::move(*storage));
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        [[nodiscard]] constexpr bool full() const noexcept {
            return size_ == capacity();
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr size_type capacity() const noexcept {
            return storage_.capacity();
        }
        [[nodiscard]] constexpr iterator begin() noexcept {
            return iterator(this, 0);
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(this, size_);
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_iterator(this, 0);
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(this, size_);
        }

        constexpr expected<void, error_code> reserve(size_type requested) noexcept {
            if (requested <= capacity()) {
                return {};
            }
            if constexpr (storage_type::fixed_capacity) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            } else {
                auto replacement = [&]() {
                    if constexpr (requires { storage_.get_allocator(); }) {
                        return storage_type::try_create(requested, storage_.get_allocator());
                    } else {
                        return storage_type::try_create(requested);
                    }
                }();
                if (!replacement) {
                    return expected<void, error_code>(unexpect, replacement.error());
                }
                for (size_type i = 0; i < size_; ++i) {
                    static_cast<void>(
                        std::construct_at(replacement->data() + i, std::move(*ptr(i))));
                }
                for (size_type i = 0; i < size_; ++i) {
                    std::destroy_at(ptr(i));
                }
                storage_ = std::move(*replacement);
                head_    = 0;
                return {};
            }
        }

        template <class... Args>
        constexpr expected<void, error_code> emplace(Args&&... args) noexcept {
            if (full()) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            static_cast<void>(std::construct_at(ptr(size_), std::forward<Args>(args)...));
            ++size_;
            return {};
        }
        constexpr expected<void, error_code> push(const_reference value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            return emplace(value);
        }
        constexpr expected<void, error_code> push(value_type&& value) noexcept {
            return emplace(std::move(value));
        }
        constexpr expected<void, error_code> push_back(const_reference value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            return push(value);
        }
        constexpr expected<void, error_code> push_back(value_type&& value) noexcept {
            return push(std::move(value));
        }
        [[nodiscard]] constexpr expected<value_type, error_code> pop() noexcept {
            if (empty()) {
                return expected<value_type, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            value_type result(std::move(storage_.data()[head_]));
            std::destroy_at(storage_.data() + head_);
            head_ = capacity() == 0 ? 0 : (head_ + 1) % capacity();
            --size_;
            if (size_ == 0) {
                head_ = 0;
            }
            return result;
        }
        [[nodiscard]] constexpr expected<reference, error_code> front() noexcept {
            if (empty()) {
                return expected<reference, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            return *ptr(0);
        }
        [[nodiscard]] constexpr expected<const_reference, error_code> front() const noexcept {
            if (empty()) {
                return expected<const_reference, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            return *ptr(0);
        }
        [[nodiscard]] constexpr expected<reference, error_code> back() noexcept {
            if (empty()) {
                return expected<reference, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            return *ptr(size_ - 1);
        }
        constexpr void clear() noexcept {
            while (!empty()) {
                static_cast<void>(pop());
            }
        }

        [[nodiscard]] constexpr fifo_segments<const value_type> readable_segments() const noexcept {
            if (size_ == 0) {
                return {array_view<const value_type>(nullptr, 0),
                        array_view<const value_type>(nullptr, 0)};
            }
            const size_type first_count = size_ < capacity() - head_ ? size_ : capacity() - head_;
            return {array_view<const value_type>(storage_.data() + head_, first_count),
                    array_view<const value_type>(storage_.data(), size_ - first_count)};
        }

        constexpr expected<void, error_code> consume(size_type count) noexcept {
            if (count > size_) {
                return expected<void, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            for (size_type i = 0; i < count; ++i) {
                std::destroy_at(storage_.data() + head_);
                head_ = (head_ + 1) % capacity();
            }
            size_ -= count;
            if (size_ == 0) {
                head_ = 0;
            }
            return {};
        }

        [[nodiscard]] constexpr fifo_segments<value_type> writable_segments() noexcept
            requires std::is_trivially_copyable_v<value_type>
        {
            const size_type available = capacity() - size_;
            if (available == 0) {
                return {array_view<value_type>(nullptr, 0), array_view<value_type>(nullptr, 0)};
            }
            const size_type tail = physical(size_);
            const size_type first_count =
                available < capacity() - tail ? available : capacity() - tail;
            return {array_view<value_type>(storage_.data() + tail, first_count),
                    array_view<value_type>(storage_.data(), available - first_count)};
        }
        constexpr expected<void, error_code> commit(size_type count) noexcept
            requires std::is_trivially_copyable_v<value_type>
        {
            if (count > capacity() - size_) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            size_ += count;
            return {};
        }
    };

    template <class T, class Allocator = allocator<T>>
    using fifo = basic_fifo<T, dynamic_fifo_storage<T, Allocator>>;

    template <class T, size_t N>
    using static_fifo = basic_fifo<T, static_fifo_storage<T, N>>;

    template <class Allocator = allocator<std::byte>>
    class byte_fifo {
        fifo<std::byte, Allocator> fifo_;

        constexpr explicit byte_fifo(fifo<std::byte, Allocator>&& fifo) noexcept
            : fifo_(std::move(fifo)) {}

    public:
        using size_type                = size_t;
        constexpr byte_fifo() noexcept = default;
        [[nodiscard]] static constexpr expected<byte_fifo, error_code> try_create(
            size_type capacity) noexcept {
            auto created = fifo<std::byte, Allocator>::try_create(capacity);
            if (!created) {
                return expected<byte_fifo, error_code>(unexpect, created.error());
            }
            return byte_fifo(std::move(*created));
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return fifo_.size();
        }
        [[nodiscard]] constexpr size_type capacity() const noexcept {
            return fifo_.capacity();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return fifo_.empty();
        }
        constexpr expected<void, error_code> try_write(array_view<const std::byte> input) noexcept {
            if (input.size() > capacity() - size()) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }
            for (std::byte value : input) {
                static_cast<void>(fifo_.push(value));
            }
            return {};
        }
        constexpr size_type write_some(array_view<const std::byte> input) noexcept {
            const size_type count =
                input.size() < capacity() - size() ? input.size() : capacity() - size();
            for (size_type i = 0; i < count; ++i) {
                static_cast<void>(fifo_.push(input[i]));
            }
            return count;
        }
        constexpr expected<void, error_code> try_read(array_view<std::byte> output) noexcept {
            if (output.size() > size()) {
                return expected<void, error_code>(unexpect, error_code::UNDERFLOW_ERROR);
            }
            for (size_type i = 0; i < output.size(); ++i) {
                output[i] = *fifo_.pop();
            }
            return {};
        }
        constexpr size_type read_some(array_view<std::byte> output) noexcept {
            const size_type count = output.size() < size() ? output.size() : size();
            for (size_type i = 0; i < count; ++i) {
                output[i] = *fifo_.pop();
            }
            return count;
        }
        [[nodiscard]] constexpr auto readable_segments() const noexcept {
            return fifo_.readable_segments();
        }
        [[nodiscard]] constexpr auto writable_segments() noexcept {
            return fifo_.writable_segments();
        }
        constexpr auto consume(size_type count) noexcept {
            return fifo_.consume(count);
        }
        constexpr auto commit(size_type count) noexcept {
            return fifo_.commit(count);
        }
    };
}  // namespace tay
