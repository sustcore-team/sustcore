/**
 * @file hash_table.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现无异常链式哈希容器共享的底层存储。
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
#include <tay/utility.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay::detail {
    struct hash_table_allocator_tag {};
    struct hash_table_hash_tag {};
    struct hash_table_equal_tag {};

    template <class T>
    struct hash_equal {
        [[nodiscard]] constexpr bool operator()(const T& left, const T& right) const
            noexcept(noexcept(left == right)) {
            return left == right;
        }
    };

    template <class Value>
    struct hash_node {
        Value value;
        hash_node* next = nullptr;

        template <class... Args>
        constexpr explicit hash_node(Args&&... args) noexcept(
            std::is_nothrow_constructible_v<Value, Args&&...>)
            : value(std::forward<Args>(args)...), next(nullptr) {}
    };

    template <class Value, class Key, class KeyOfValue, class Allocator, class Hash, class KeyEqual,
              bool ImmutableIterator>
    class hash_table : private composition<hash_table_allocator_tag, Allocator>,
                       private composition<hash_table_hash_tag, Hash>,
                       private composition<hash_table_equal_tag, KeyEqual> {
    public:
        using value_type            = Value;
        using key_type              = Key;
        using allocator_type        = Allocator;
        using allocator_traits_type = allocator_traits<allocator_type>;
        using size_type             = typename allocator_traits_type::size_type;
        using difference_type       = typename allocator_traits_type::difference_type;
        using hasher                = Hash;
        using key_equal             = KeyEqual;
        using reference             = value_type&;
        using const_reference       = const value_type&;
        using pointer               = value_type*;
        using const_pointer         = const value_type*;

    private:
        using node_type        = hash_node<value_type>;
        using node_allocator   = typename allocator_traits_type::template rebind_alloc<node_type>;
        using node_traits      = allocator_traits<node_allocator>;
        using bucket_allocator = typename allocator_traits_type::template rebind_alloc<node_type*>;
        using bucket_traits    = allocator_traits<bucket_allocator>;

        static_assert(std::is_same_v<typename allocator_traits_type::value_type, value_type>,
                      "hash table allocator value_type must match value_type");
        static_assert(std::is_nothrow_destructible_v<value_type>,
                      "hash containers require a nothrow destructor");
        static_assert(std::is_convertible_v<
                          decltype(std::declval<const hasher&>()(std::declval<const key_type&>())),
                          size_type> &&
                          noexcept(std::declval<const hasher&>()(std::declval<const key_type&>())),
                      "hash containers require a noexcept Hash");
        static_assert(std::is_convertible_v<decltype(std::declval<const key_equal&>()(
                                                std::declval<const key_type&>(),
                                                std::declval<const key_type&>())),
                                            bool>,
                      "hash containers require a boolean KeyEqual");

        node_type** buckets_        = nullptr;
        size_type bucket_count_     = 0;
        size_type size_             = 0;
        size_type max_load_percent_ = 100;

        [[nodiscard]] constexpr allocator_type& allocator_ref() noexcept {
            return get<hash_table_allocator_tag>(this);
        }

        [[nodiscard]] constexpr const allocator_type& allocator_ref() const noexcept {
            return get<hash_table_allocator_tag>(this);
        }

        [[nodiscard]] constexpr hasher& hash_ref() noexcept {
            return get<hash_table_hash_tag>(this);
        }

        [[nodiscard]] constexpr const hasher& hash_ref() const noexcept {
            return get<hash_table_hash_tag>(this);
        }

        [[nodiscard]] constexpr key_equal& equal_ref() noexcept {
            return get<hash_table_equal_tag>(this);
        }

        [[nodiscard]] constexpr const key_equal& equal_ref() const noexcept {
            return get<hash_table_equal_tag>(this);
        }

        [[nodiscard]] static constexpr const key_type& key_of(const value_type& value) noexcept {
            return KeyOfValue{}(value);
        }

        [[nodiscard]] constexpr node_allocator node_alloc() const noexcept {
            return node_allocator(allocator_ref());
        }

        [[nodiscard]] constexpr bucket_allocator bucket_alloc() const noexcept {
            return bucket_allocator(allocator_ref());
        }

        [[nodiscard]] constexpr size_type bucket_index_for(const key_type& key,
                                                           size_type count) const noexcept {
            return count == 0 ? 0 : static_cast<size_type>(hash_ref()(key)) % count;
        }

        [[nodiscard]] constexpr size_type max_elements_for_buckets(size_type count) const noexcept {
            const size_type maximum   = size_type(-1);
            const size_type whole     = max_load_percent_ / 100;
            const size_type remainder = max_load_percent_ % 100;

            if (whole != 0 && count > maximum / whole) {
                return maximum;
            }
            size_type result = count * whole;

            const size_type hundreds = count / 100;
            if (remainder != 0 && hundreds > (maximum - result) / remainder) {
                return maximum;
            }
            result               += hundreds * remainder;
            const size_type tail  = ((count % 100) * remainder) / 100;
            if (tail > maximum - result) {
                return maximum;
            }
            return result + tail;
        }

        [[nodiscard]] constexpr expected<size_type, error_code> minimum_bucket_count(
            size_type elements) const noexcept {
            if (elements == 0) {
                return size_type{1};
            }
            const size_type maximum = max_bucket_count();
            if (maximum == 0 || max_elements_for_buckets(maximum) < elements) {
                return expected<size_type, error_code>(unexpect,
                                                       error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            size_type low  = 1;
            size_type high = maximum;
            while (low < high) {
                const size_type middle = low + (high - low) / 2;
                if (max_elements_for_buckets(middle) >= elements) {
                    high = middle;
                } else {
                    low = middle + 1;
                }
            }
            return low;
        }

        [[nodiscard]] constexpr expected<node_type**, error_code> allocate_buckets(
            size_type count) noexcept {
            bucket_allocator allocator = bucket_alloc();
            auto allocation            = bucket_traits::try_allocate(allocator, count);
            if (!allocation) {
                return expected<node_type**, error_code>(unexpect, allocation.error());
            }
            for (size_type index = 0; index < count; ++index) {
                (*allocation)[index] = nullptr;
            }
            return *allocation;
        }

        constexpr void deallocate_buckets(node_type** buckets, size_type count) noexcept {
            if (buckets == nullptr) {
                return;
            }
            bucket_allocator allocator = bucket_alloc();
            bucket_traits::deallocate(allocator, buckets, count);
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...>)
        [[nodiscard]] constexpr expected<node_type*, error_code> create_node(
            Args&&... args) noexcept {
            node_allocator allocator = node_alloc();
            auto allocation          = node_traits::try_allocate(allocator, 1);
            if (!allocation) {
                return expected<node_type*, error_code>(unexpect, allocation.error());
            }
            node_traits::construct(allocator, *allocation, std::forward<Args>(args)...);
            return *allocation;
        }

        constexpr void destroy_node(node_type* node) noexcept {
            node_allocator allocator = node_alloc();
            node_traits::destroy(allocator, node);
            node_traits::deallocate(allocator, node, 1);
        }

        constexpr void reset() noexcept {
            clear();
            deallocate_buckets(buckets_, bucket_count_);
            buckets_      = nullptr;
            bucket_count_ = 0;
        }

        constexpr void take_storage(hash_table&& other) noexcept {
            buckets_            = other.buckets_;
            bucket_count_       = other.bucket_count_;
            size_               = other.size_;
            max_load_percent_   = other.max_load_percent_;
            other.buckets_      = nullptr;
            other.bucket_count_ = 0;
            other.size_         = 0;
        }

    public:
        template <bool Constant>
        class basic_iterator {
            friend class hash_table;
            template <bool>
            friend class basic_iterator;

            using table_type   = std::conditional_t<Constant, const hash_table, hash_table>;
            table_type* table_ = nullptr;
            size_type bucket_  = 0;
            node_type* node_   = nullptr;

            constexpr basic_iterator(table_type* table, size_type bucket, node_type* node) noexcept
                : table_(table), bucket_(bucket), node_(node) {}

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = hash_table::value_type;
            using difference_type   = hash_table::difference_type;
            using reference =
                std::conditional_t<Constant || ImmutableIterator, const value_type&, value_type&>;
            using pointer =
                std::conditional_t<Constant || ImmutableIterator, const value_type*, value_type*>;

            constexpr basic_iterator() noexcept                                 = default;
            constexpr basic_iterator(const basic_iterator&) noexcept            = default;
            constexpr basic_iterator& operator=(const basic_iterator&) noexcept = default;

            template <bool Other>
                requires(Constant && !Other)
            constexpr basic_iterator(const basic_iterator<Other>& other) noexcept
                : table_(other.table_), bucket_(other.bucket_), node_(other.node_) {}

            [[nodiscard]] constexpr reference operator*() const noexcept {
                return node_->value;
            }
            [[nodiscard]] constexpr pointer operator->() const noexcept {
                return &node_->value;
            }

            constexpr basic_iterator& operator++() noexcept {
                if (node_ == nullptr) {
                    return *this;
                }
                node_ = node_->next;
                while (node_ == nullptr && ++bucket_ < table_->bucket_count_) {
                    node_ = table_->buckets_[bucket_];
                }
                return *this;
            }

            constexpr basic_iterator operator++(int) noexcept {
                basic_iterator copy = *this;
                ++*this;
                return copy;
            }

            template <bool Other>
            [[nodiscard]] constexpr bool operator==(
                const basic_iterator<Other>& other) const noexcept {
                return node_ == other.node_ && table_ == other.table_;
            }
        };

        using iterator       = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

        constexpr hash_table(const allocator_type& allocator, const hasher& hash,
                             const key_equal& equal) noexcept
            : composition<hash_table_allocator_tag, allocator_type>(allocator),
              composition<hash_table_hash_tag, hasher>(hash),
              composition<hash_table_equal_tag, key_equal>(equal) {}

        constexpr hash_table(hash_table&& other) noexcept
            : composition<hash_table_allocator_tag, allocator_type>(
                  std::move(other.allocator_ref())),
              composition<hash_table_hash_tag, hasher>(std::move(other.hash_ref())),
              composition<hash_table_equal_tag, key_equal>(std::move(other.equal_ref())) {
            take_storage(std::move(other));
        }

        hash_table(const hash_table&)            = delete;
        hash_table& operator=(const hash_table&) = delete;
        hash_table& operator=(hash_table&&)      = delete;

        constexpr ~hash_table() noexcept {
            reset();
        }

        [[nodiscard]] constexpr expected<void, error_code> init(size_type bucket_count) noexcept {
            if (buckets_ != nullptr) {
                return expected<void, error_code>(unexpect, error_code::INVALID_ARGUMENT);
            }
            if (bucket_count == 0) {
                bucket_count = 1;
            }
            if (bucket_count > max_bucket_count()) {
                return expected<void, error_code>(unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            auto allocation = allocate_buckets(bucket_count);
            if (!allocation) {
                return expected<void, error_code>(unexpect, allocation.error());
            }
            buckets_      = *allocation;
            bucket_count_ = bucket_count;
            return {};
        }

        [[nodiscard]] constexpr allocator_type allocator() const noexcept {
            return allocator_ref();
        }
        [[nodiscard]] constexpr hasher hash_function() const noexcept {
            return hash_ref();
        }
        [[nodiscard]] constexpr key_equal key_eq() const noexcept {
            return equal_ref();
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            for (size_type index = 0; index < bucket_count_; ++index) {
                if (buckets_[index] != nullptr) {
                    return iterator(this, index, buckets_[index]);
                }
            }
            return end();
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return cbegin();
        }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            for (size_type index = 0; index < bucket_count_; ++index) {
                if (buckets_[index] != nullptr) {
                    return const_iterator(this, index, buckets_[index]);
                }
            }
            return cend();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(this, bucket_count_, nullptr);
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return cend();
        }
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return const_iterator(this, bucket_count_, nullptr);
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            node_allocator allocator = node_alloc();
            return node_traits::max_size(allocator);
        }
        [[nodiscard]] constexpr size_type bucket_count() const noexcept {
            return bucket_count_;
        }
        [[nodiscard]] constexpr size_type max_bucket_count() const noexcept {
            bucket_allocator allocator = bucket_alloc();
            return bucket_traits::max_size(allocator);
        }
        [[nodiscard]] constexpr size_type max_load_percent() const noexcept {
            return max_load_percent_;
        }

        constexpr expected<void, error_code> max_load_percent(size_type percent) noexcept {
            if (percent == 0) {
                return expected<void, error_code>(unexpect, error_code::INVALID_ARGUMENT);
            }
            max_load_percent_ = percent;
            return {};
        }

        constexpr void clear() noexcept {
            if (buckets_ == nullptr) {
                size_ = 0;
                return;
            }
            for (size_type index = 0; index < bucket_count_; ++index) {
                node_type* node = buckets_[index];
                while (node != nullptr) {
                    node_type* next = node->next;
                    destroy_node(node);
                    node = next;
                }
                buckets_[index] = nullptr;
            }
            size_ = 0;
        }

        [[nodiscard]] constexpr iterator find(const key_type& key) noexcept {
            if (buckets_ == nullptr) {
                return end();
            }
            const size_type index = bucket_index_for(key, bucket_count_);
            for (node_type* node = buckets_[index]; node != nullptr; node = node->next) {
                if (equal_ref()(key_of(node->value), key)) {
                    return iterator(this, index, node);
                }
            }
            return end();
        }

        [[nodiscard]] constexpr const_iterator find(const key_type& key) const noexcept {
            if (buckets_ == nullptr) {
                return cend();
            }
            const size_type index = bucket_index_for(key, bucket_count_);
            for (node_type* node = buckets_[index]; node != nullptr; node = node->next) {
                if (equal_ref()(key_of(node->value), key)) {
                    return const_iterator(this, index, node);
                }
            }
            return cend();
        }

        [[nodiscard]] constexpr bool contains(const key_type& key) const noexcept {
            return find(key) != cend();
        }

        [[nodiscard]] constexpr size_type count(const key_type& key) const noexcept {
            return contains(key) ? 1 : 0;
        }

        [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(
            const key_type& key) noexcept {
            iterator found = find(key);
            if (found == end()) {
                return {found, found};
            }
            iterator next = found;
            ++next;
            return {found, next};
        }

        [[nodiscard]] constexpr std::pair<const_iterator, const_iterator> equal_range(
            const key_type& key) const noexcept {
            const_iterator found = find(key);
            if (found == cend()) {
                return {found, found};
            }
            const_iterator next = found;
            ++next;
            return {found, next};
        }

        [[nodiscard]] constexpr size_type bucket_size(size_type index) const noexcept {
            if (index >= bucket_count_ || buckets_ == nullptr) {
                return 0;
            }
            size_type result = 0;
            for (node_type* node = buckets_[index]; node != nullptr; node = node->next) {
                ++result;
            }
            return result;
        }

        [[nodiscard]] constexpr size_type bucket(const key_type& key) const noexcept {
            return bucket_index_for(key, bucket_count_);
        }

        constexpr expected<void, error_code> rehash(size_type requested) noexcept {
            auto minimum = minimum_bucket_count(size_);
            if (!minimum) {
                return expected<void, error_code>(unexpect, minimum.error());
            }
            if (requested < *minimum) {
                requested = *minimum;
            }
            if (requested == 0) {
                requested = 1;
            }
            if (requested == bucket_count_) {
                return {};
            }
            if (requested > max_bucket_count()) {
                return expected<void, error_code>(unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            auto allocation = allocate_buckets(requested);
            if (!allocation) {
                return expected<void, error_code>(unexpect, allocation.error());
            }
            node_type** replacement = *allocation;
            for (size_type index = 0; index < bucket_count_; ++index) {
                node_type* node = buckets_[index];
                while (node != nullptr) {
                    node_type* next        = node->next;
                    const size_type target = bucket_index_for(key_of(node->value), requested);
                    node->next             = replacement[target];
                    replacement[target]    = node;
                    node                   = next;
                }
            }
            deallocate_buckets(buckets_, bucket_count_);
            buckets_      = replacement;
            bucket_count_ = requested;
            return {};
        }

        constexpr expected<void, error_code> reserve(size_type elements) noexcept {
            auto required = minimum_bucket_count(elements);
            if (!required) {
                return expected<void, error_code>(unexpect, required.error());
            }
            return *required > bucket_count_ ? rehash(*required) : expected<void, error_code>{};
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...>)
        [[nodiscard]] constexpr expected<std::pair<iterator, bool>, error_code> emplace_unique(
            const key_type& lookup_key, Args&&... args) noexcept {
            iterator existing = find(lookup_key);
            if (existing != end()) {
                return std::pair<iterator, bool>{existing, false};
            }
            if (size_ == max_size()) {
                return expected<std::pair<iterator, bool>, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            auto created = create_node(std::forward<Args>(args)...);
            if (!created) {
                return expected<std::pair<iterator, bool>, error_code>(unexpect, created.error());
            }
            node_type* new_node = *created;

            if (buckets_ == nullptr || size_ + 1 > max_elements_for_buckets(bucket_count_)) {
                auto minimum = minimum_bucket_count(size_ + 1);
                if (!minimum) {
                    destroy_node(new_node);
                    return expected<std::pair<iterator, bool>, error_code>(unexpect,
                                                                           minimum.error());
                }
                size_type target = *minimum;
                if (bucket_count_ != 0 && bucket_count_ <= max_bucket_count() - bucket_count_) {
                    const size_type doubled = bucket_count_ + bucket_count_;
                    if (doubled > target) {
                        target = doubled;
                    }
                }
                auto grown = rehash(target);
                if (!grown) {
                    destroy_node(new_node);
                    return expected<std::pair<iterator, bool>, error_code>(unexpect, grown.error());
                }
            }

            const size_type index = bucket_index_for(key_of(new_node->value), bucket_count_);
            new_node->next        = buckets_[index];
            buckets_[index]       = new_node;
            ++size_;
            return std::pair<iterator, bool>{iterator(this, index, new_node), true};
        }

        constexpr iterator erase(const_iterator position) noexcept {
            if (position.table_ != this || position.node_ == nullptr ||
                position.bucket_ >= bucket_count_)
            {
                return end();
            }
            const size_type index = position.bucket_;
            node_type* previous   = nullptr;
            node_type* current    = buckets_[index];
            while (current != nullptr && current != position.node_) {
                previous = current;
                current  = current->next;
            }
            if (current == nullptr) {
                return end();
            }
            node_type* next = current->next;
            if (previous == nullptr) {
                buckets_[index] = next;
            } else {
                previous->next = next;
            }
            destroy_node(current);
            --size_;
            if (next != nullptr) {
                return iterator(this, index, next);
            }
            for (size_type bucket = index + 1; bucket < bucket_count_; ++bucket) {
                if (buckets_[bucket] != nullptr) {
                    return iterator(this, bucket, buckets_[bucket]);
                }
            }
            return end();
        }

        constexpr iterator erase(iterator position) noexcept {
            return erase(const_iterator(position));
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept {
            while (first != last && first != cend()) {
                first = erase(first);
            }
            return first == cend() ? end() : iterator(this, first.bucket_, first.node_);
        }

        constexpr size_type erase(const key_type& key) noexcept {
            iterator found = find(key);
            if (found == end()) {
                return 0;
            }
            erase(found);
            return 1;
        }

        constexpr expected<void, error_code> swap(hash_table& other) noexcept {
            if (this == &other) {
                return {};
            }
            if constexpr (allocator_traits_type::propagate_on_container_swap::value) {
                using std::swap;
                swap(allocator_ref(), other.allocator_ref());
            } else if constexpr (!allocator_traits_type::is_always_equal::value) {
                if (allocator_ref() != other.allocator_ref()) {
                    return expected<void, error_code>(unexpect, error_code::INVALID_ARGUMENT);
                }
            }
            using std::swap;
            swap(hash_ref(), other.hash_ref());
            swap(equal_ref(), other.equal_ref());
            swap(buckets_, other.buckets_);
            swap(bucket_count_, other.bucket_count_);
            swap(size_, other.size_);
            swap(max_load_percent_, other.max_load_percent_);
            return {};
        }
    };
}  // namespace tay::detail
