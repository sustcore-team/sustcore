/**
 * @file set.h
 * @brief Exception-free unique-key chained hash sets.
 */

#pragma once

#include <tay/detail/hash_table.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        template <class Key>
        struct set_key_of {
            [[nodiscard]] constexpr const Key& operator()(
                const Key& value) const noexcept {
                return value;
            }
        };
    }  // namespace detail

    template <class Key, class Allocator = allocator<Key>,
              class Hash     = std::hash<Key>,
              class KeyEqual = detail::hash_equal<Key>>
    class hash_set {
    public:
        using key_type       = Key;
        using value_type     = Key;
        using allocator_type = Allocator;
        using hasher         = Hash;
        using key_equal      = KeyEqual;

    private:
        using table_type =
            detail::hash_table<value_type, key_type,
                               detail::set_key_of<key_type>, allocator_type,
                               hasher, key_equal, true>;
        using allocator_traits_type = allocator_traits<allocator_type>;
        struct empty_tag {};

        static constexpr bool has_defaults =
            std::is_nothrow_default_constructible_v<allocator_type> &&
            std::is_nothrow_default_constructible_v<hasher> &&
            std::is_nothrow_default_constructible_v<key_equal>;

        table_type table_;

        constexpr hash_set(empty_tag, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : table_(allocator, hash, equal) {}

        [[noreturn]] static constexpr void panic_error(
            error_code error) noexcept {
            switch (error) {
                case error_code::OUT_OF_MEMORY:
                    tay::panic("hash_set allocation failed");
                case error_code::ALLOCATION_SIZE_OVERFLOW:
                    tay::panic("hash_set size overflow");
                case error_code::INVALID_ARGUMENT:
                    tay::panic("hash_set invalid argument");
                default: tay::panic("hash_set operation failed");
            }
        }

        template <class InputIt>
        static constexpr expected<hash_set, error_code> create_range(
            InputIt first, InputIt last, typename table_type::size_type buckets,
            const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            auto result = try_create(buckets, hash, equal, allocator);
            if (!result) {
                return result;
            }
            auto inserted = result->insert(first, last);
            if (!inserted) {
                return expected<hash_set, error_code>(unexpect,
                                                      inserted.error());
            }
            return result;
        }

    public:
        using size_type       = typename table_type::size_type;
        using difference_type = typename table_type::difference_type;
        using reference       = const value_type&;
        using const_reference = const value_type&;
        using pointer         = const value_type*;
        using const_pointer   = const value_type*;
        using iterator        = typename table_type::iterator;
        using const_iterator  = typename table_type::const_iterator;

        constexpr hash_set() noexcept
            requires(has_defaults)
            : hash_set(1) {}

        constexpr explicit hash_set(size_type bucket_count) noexcept
            requires(has_defaults)
            : hash_set(bucket_count, hasher{}, key_equal{}, allocator_type{}) {}

        constexpr explicit hash_set(const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : hash_set(1, hasher{}, key_equal{}, allocator) {}

        constexpr hash_set(size_type bucket_count,
                           const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : hash_set(bucket_count, hasher{}, key_equal{}, allocator) {}

        constexpr hash_set(size_type bucket_count, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : table_(allocator, hash, equal) {
            auto initialized = table_.initialize(bucket_count);
            if (!initialized) {
                panic_error(initialized.error());
            }
        }

        template <class InputIt>
        constexpr hash_set(InputIt first, InputIt last, size_type bucket_count,
                           const hasher& hash, const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : table_(allocator, hash, equal) {
            auto created =
                create_range(first, last, bucket_count, hash, equal, allocator);
            if (!created) {
                panic_error(created.error());
            }
            auto swapped = table_.swap(created->table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
        }

        template <class InputIt>
        constexpr hash_set(InputIt first, InputIt last,
                           size_type bucket_count = 1) noexcept
            requires(has_defaults)
            : hash_set(first, last, bucket_count, hasher{}, key_equal{},
                       allocator_type{}) {}

        constexpr hash_set(std::initializer_list<value_type> values,
                           size_type bucket_count = 1) noexcept
            requires(has_defaults)
            : hash_set(values.begin(), values.end(), bucket_count) {}

        constexpr hash_set(std::initializer_list<value_type> values,
                           size_type bucket_count, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : hash_set(values.begin(), values.end(), bucket_count, hash, equal,
                       allocator) {}

        constexpr hash_set(const hash_set& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : hash_set(
                  other,
                  allocator_traits_type::select_on_container_copy_construction(
                      other.get_allocator())) {}

        constexpr hash_set(const hash_set& other,
                           const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : table_(allocator, other.hash_function(), other.key_eq()) {
            auto created = try_create(other, allocator);
            if (!created) {
                panic_error(created.error());
            }
            auto swapped = table_.swap(created->table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
        }

        constexpr hash_set(hash_set&& other) noexcept
            : table_(std::move(other.table_)) {}

        constexpr hash_set(hash_set&& other,
                           const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : table_(allocator, other.hash_function(), other.key_eq()) {
            if constexpr (allocator_traits_type::is_always_equal::value) {
                auto swapped = table_.swap(other.table_);
                if (!swapped) {
                    panic_error(swapped.error());
                }
            } else if (allocator == other.get_allocator()) {
                auto swapped = table_.swap(other.table_);
                if (!swapped) {
                    panic_error(swapped.error());
                }
            } else {
                auto created = try_create(other, allocator);
                if (!created) {
                    panic_error(created.error());
                }
                auto swapped = table_.swap(created->table_);
                if (!swapped) {
                    panic_error(swapped.error());
                }
                other.clear();
            }
        }

        constexpr ~hash_set() noexcept = default;

        static constexpr expected<hash_set, error_code> try_create() noexcept
            requires(has_defaults)
        {
            return try_create(1, hasher{}, key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_set, error_code> try_create(
            const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
        {
            return try_create(1, hasher{}, key_equal{}, allocator);
        }

        static constexpr expected<hash_set, error_code> try_create(
            size_type bucket_count) noexcept
            requires(has_defaults)
        {
            return try_create(bucket_count, hasher{}, key_equal{},
                              allocator_type{});
        }

        static constexpr expected<hash_set, error_code> try_create(
            size_type bucket_count, const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            hash_set result(empty_tag{}, hash, equal, allocator);
            auto initialized = result.table_.initialize(bucket_count);
            if (!initialized) {
                return expected<hash_set, error_code>(unexpect,
                                                      initialized.error());
            }
            return result;
        }

        template <class InputIt>
        static constexpr expected<hash_set, error_code> try_create(
            InputIt first, InputIt last, size_type bucket_count,
            const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            return create_range(first, last, bucket_count, hash, equal,
                                allocator);
        }

        template <class InputIt>
        static constexpr expected<hash_set, error_code> try_create(
            InputIt first, InputIt last, size_type bucket_count = 1) noexcept
            requires(has_defaults)
        {
            return create_range(first, last, bucket_count, hasher{},
                                key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_set, error_code> try_create(
            std::initializer_list<value_type> values,
            size_type bucket_count = 1) noexcept
            requires(has_defaults)
        {
            return create_range(values.begin(), values.end(), bucket_count,
                                hasher{}, key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_set, error_code> try_create(
            const hash_set& other, const allocator_type& allocator) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto result =
                try_create(other.bucket_count(), other.hash_function(),
                           other.key_eq(), allocator);
            if (!result) {
                return result;
            }
            auto percent = result->max_load_percent(other.max_load_percent());
            if (!percent) {
                return expected<hash_set, error_code>(unexpect,
                                                      percent.error());
            }
            auto inserted = result->insert(other.begin(), other.end());
            if (!inserted) {
                return expected<hash_set, error_code>(unexpect,
                                                      inserted.error());
            }
            return result;
        }

        constexpr hash_set& operator=(const hash_set& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            if (this == &other) {
                return *this;
            }
            const allocator_type target_allocator =
                allocator_traits_type::propagate_on_container_copy_assignment::
                        value
                    ? other.get_allocator()
                    : get_allocator();
            auto replacement = try_create(other, target_allocator);
            if (!replacement) {
                panic_error(replacement.error());
            }
            auto swapped = table_.swap(replacement->table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
            return *this;
        }

        constexpr hash_set& operator=(hash_set&& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            if (this == &other) {
                return *this;
            }
            hash_set replacement(std::move(other), get_allocator());
            auto swapped = table_.swap(replacement.table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
            return *this;
        }

        constexpr hash_set& operator=(
            std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto replacement =
                create_range(values.begin(), values.end(), 1, hash_function(),
                             key_eq(), get_allocator());
            if (!replacement) {
                panic_error(replacement.error());
            }
            auto swapped = table_.swap(replacement->table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
            return *this;
        }

        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return table_.allocator();
        }
        [[nodiscard]] constexpr iterator begin() noexcept {
            return table_.begin();
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return table_.begin();
        }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return table_.cbegin();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return table_.end();
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return table_.end();
        }
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return table_.cend();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return table_.empty();
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return table_.size();
        }
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return table_.max_size();
        }
        constexpr void clear() noexcept {
            table_.clear();
        }

        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            const value_type& value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return table_.emplace_unique(value, value);
        }

        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            value_type&& value) noexcept
            requires(std::is_nothrow_move_constructible_v<value_type>)
        {
            return table_.emplace_unique(value, std::move(value));
        }

        template <class P>
            requires(!std::is_same_v<std::remove_cvref_t<P>, value_type> &&
                     std::is_nothrow_constructible_v<value_type, P &&>)
        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            P&& value) noexcept {
            value_type converted(std::forward<P>(value));
            return insert(std::move(converted));
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator, const value_type& value) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            auto result = insert(value);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return result->first;
        }

        constexpr expected<iterator, error_code> insert(
            const_iterator, value_type&& value) noexcept
            requires(std::is_nothrow_move_constructible_v<value_type>)
        {
            auto result = insert(std::move(value));
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return result->first;
        }

        template <class InputIt>
        constexpr expected<void, error_code> insert(InputIt first,
                                                    InputIt last) noexcept {
            for (; first != last; ++first) {
                auto result = insert(*first);
                if (!result) {
                    return expected<void, error_code>(unexpect, result.error());
                }
            }
            return {};
        }

        constexpr expected<void, error_code> insert(
            std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            return insert(values.begin(), values.end());
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...> &&
                     std::is_nothrow_move_constructible_v<value_type>)
        constexpr expected<std::pair<iterator, bool>, error_code> emplace(
            Args&&... args) noexcept {
            value_type value(std::forward<Args>(args)...);
            return insert(std::move(value));
        }

        template <class... Args>
        constexpr expected<iterator, error_code> emplace_hint(
            const_iterator, Args&&... args) noexcept {
            auto result = emplace(std::forward<Args>(args)...);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return result->first;
        }

        constexpr iterator erase(iterator position) noexcept {
            return table_.erase(position);
        }
        constexpr iterator erase(const_iterator position) noexcept {
            return table_.erase(position);
        }
        constexpr iterator erase(const_iterator first,
                                 const_iterator last) noexcept {
            return table_.erase(first, last);
        }
        constexpr size_type erase(const key_type& key) noexcept {
            return table_.erase(key);
        }
        [[nodiscard]] constexpr iterator find(const key_type& key) noexcept {
            return table_.find(key);
        }
        [[nodiscard]] constexpr const_iterator find(
            const key_type& key) const noexcept {
            return table_.find(key);
        }
        [[nodiscard]] constexpr bool contains(
            const key_type& key) const noexcept {
            return table_.contains(key);
        }
        [[nodiscard]] constexpr size_type count(
            const key_type& key) const noexcept {
            return table_.count(key);
        }
        [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(
            const key_type& key) noexcept {
            return table_.equal_range(key);
        }
        [[nodiscard]] constexpr std::pair<const_iterator, const_iterator>
        equal_range(const key_type& key) const noexcept {
            return table_.equal_range(key);
        }
        [[nodiscard]] constexpr size_type bucket_count() const noexcept {
            return table_.bucket_count();
        }
        [[nodiscard]] constexpr size_type max_bucket_count() const noexcept {
            return table_.max_bucket_count();
        }
        [[nodiscard]] constexpr size_type bucket_size(
            size_type index) const noexcept {
            return table_.bucket_size(index);
        }
        [[nodiscard]] constexpr size_type bucket(
            const key_type& key) const noexcept {
            return table_.bucket(key);
        }
        [[nodiscard]] constexpr size_type max_load_percent() const noexcept {
            return table_.max_load_percent();
        }
        constexpr expected<void, error_code> max_load_percent(
            size_type percent) noexcept {
            return table_.max_load_percent(percent);
        }
        constexpr expected<void, error_code> rehash(size_type count) noexcept {
            return table_.rehash(count);
        }
        constexpr expected<void, error_code> reserve(size_type count) noexcept {
            return table_.reserve(count);
        }
        [[nodiscard]] constexpr hasher hash_function() const noexcept {
            return table_.hash_function();
        }
        [[nodiscard]] constexpr key_equal key_eq() const noexcept {
            return table_.key_eq();
        }
        constexpr expected<void, error_code> swap(hash_set& other) noexcept {
            return table_.swap(other.table_);
        }
    };

    template <class Key, class Allocator, class Hash, class KeyEqual>
    constexpr expected<void, error_code> swap(
        hash_set<Key, Allocator, Hash, KeyEqual>& left,
        hash_set<Key, Allocator, Hash, KeyEqual>& right) noexcept {
        return left.swap(right);
    }

    template <class Key, class Allocator = allocator<Key>,
              class Hash     = std::hash<Key>,
              class KeyEqual = detail::hash_equal<Key>>
    using set = hash_set<Key, Allocator, Hash, KeyEqual>;
}  // namespace tay
