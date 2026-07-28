/**
 * @file map.h
 * @brief Exception-free unique-key chained hash maps.
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
        template <class Key, class T>
        struct map_key_of {
            [[nodiscard]] constexpr const Key& operator()(
                const std::pair<const Key, T>& value) const noexcept {
                return value.first;
            }
        };
    }  // namespace detail

    template <class Key, class T, class Allocator, class Hash = std::hash<Key>,
              class KeyEqual = detail::hash_equal<Key>>
    class hash_map {
    public:
        using key_type       = Key;
        using mapped_type    = T;
        using value_type     = std::pair<const key_type, mapped_type>;
        using allocator_type = Allocator;
        using hasher         = Hash;
        using key_equal      = KeyEqual;

    private:
        using table_type =
            detail::hash_table<value_type, key_type,
                               detail::map_key_of<key_type, mapped_type>,
                               allocator_type, hasher, key_equal, false>;
        using allocator_traits_type = allocator_traits<allocator_type>;
        struct empty_tag {};

        static constexpr bool has_defaults =
            std::is_nothrow_default_constructible_v<allocator_type> &&
            std::is_nothrow_default_constructible_v<hasher> &&
            std::is_nothrow_default_constructible_v<key_equal>;

        table_type table_;

        constexpr hash_map(empty_tag, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : table_(allocator, hash, equal) {}

        [[noreturn]] static constexpr void panic_error(
            error_code error) noexcept {
            switch (error) {
                case error_code::OUT_OF_MEMORY:
                    tay::panic("hash_map allocation failed");
                case error_code::ALLOCATION_SIZE_OVERFLOW:
                    tay::panic("hash_map size overflow");
                case error_code::OUT_OF_RANGE:
                    tay::panic("hash_map key not found");
                case error_code::INVALID_ARGUMENT:
                    tay::panic("hash_map invalid argument");
                default: tay::panic("hash_map operation failed");
            }
        }

        template <class InputIt>
        static constexpr expected<hash_map, error_code> create_range(
            InputIt first, InputIt last, typename table_type::size_type buckets,
            const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            auto result = try_create(buckets, hash, equal, allocator);
            if (!result) {
                return result;
            }
            auto inserted = result->insert(first, last);
            if (!inserted) {
                return expected<hash_map, error_code>(unexpect,
                                                      inserted.error());
            }
            return result;
        }

    public:
        using size_type       = typename table_type::size_type;
        using difference_type = typename table_type::difference_type;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = typename table_type::iterator;
        using const_iterator  = typename table_type::const_iterator;

        constexpr hash_map() noexcept
            requires(has_defaults)
            : hash_map(1) {}

        constexpr explicit hash_map(size_type bucket_count) noexcept
            requires(has_defaults)
            : hash_map(bucket_count, hasher{}, key_equal{}, allocator_type{}) {}

        constexpr explicit hash_map(const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : hash_map(1, hasher{}, key_equal{}, allocator) {}

        constexpr hash_map(size_type bucket_count,
                           const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : hash_map(bucket_count, hasher{}, key_equal{}, allocator) {}

        constexpr hash_map(size_type bucket_count, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : table_(allocator, hash, equal) {
            auto initialized = table_.initialize(bucket_count);
            if (!initialized) {
                panic_error(initialized.error());
            }
        }

        template <class InputIt>
        constexpr hash_map(InputIt first, InputIt last, size_type bucket_count,
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
        constexpr hash_map(InputIt first, InputIt last,
                           size_type bucket_count = 1) noexcept
            requires(has_defaults)
            : hash_map(first, last, bucket_count, hasher{}, key_equal{},
                       allocator_type{}) {}

        constexpr hash_map(std::initializer_list<value_type> values,
                           size_type bucket_count = 1) noexcept
            requires(has_defaults)
            : hash_map(values.begin(), values.end(), bucket_count) {}

        constexpr hash_map(std::initializer_list<value_type> values,
                           size_type bucket_count, const hasher& hash,
                           const key_equal& equal,
                           const allocator_type& allocator) noexcept
            : hash_map(values.begin(), values.end(), bucket_count, hash, equal,
                       allocator) {}

        constexpr hash_map(const hash_map& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
            : hash_map(
                  other,
                  allocator_traits_type::select_on_container_copy_construction(
                      other.get_allocator())) {}

        constexpr hash_map(const hash_map& other,
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

        constexpr hash_map(hash_map&& other) noexcept
            : table_(std::move(other.table_)) {}

        constexpr hash_map(hash_map&& other,
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

        constexpr ~hash_map() noexcept = default;

        static constexpr expected<hash_map, error_code> try_create() noexcept
            requires(has_defaults)
        {
            return try_create(1, hasher{}, key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_map, error_code> try_create(
            const allocator_type& allocator) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
        {
            return try_create(1, hasher{}, key_equal{}, allocator);
        }

        static constexpr expected<hash_map, error_code> try_create(
            size_type bucket_count) noexcept
            requires(has_defaults)
        {
            return try_create(bucket_count, hasher{}, key_equal{},
                              allocator_type{});
        }

        static constexpr expected<hash_map, error_code> try_create(
            size_type bucket_count, const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            hash_map result(empty_tag{}, hash, equal, allocator);
            auto initialized = result.table_.initialize(bucket_count);
            if (!initialized) {
                return expected<hash_map, error_code>(unexpect,
                                                      initialized.error());
            }
            return result;
        }

        template <class InputIt>
        static constexpr expected<hash_map, error_code> try_create(
            InputIt first, InputIt last, size_type bucket_count,
            const hasher& hash, const key_equal& equal,
            const allocator_type& allocator) noexcept {
            return create_range(first, last, bucket_count, hash, equal,
                                allocator);
        }

        template <class InputIt>
        static constexpr expected<hash_map, error_code> try_create(
            InputIt first, InputIt last, size_type bucket_count = 1) noexcept
            requires(has_defaults)
        {
            return create_range(first, last, bucket_count, hasher{},
                                key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_map, error_code> try_create(
            std::initializer_list<value_type> values,
            size_type bucket_count = 1) noexcept
            requires(has_defaults)
        {
            return create_range(values.begin(), values.end(), bucket_count,
                                hasher{}, key_equal{}, allocator_type{});
        }

        static constexpr expected<hash_map, error_code> try_create(
            const hash_map& other, const allocator_type& allocator) noexcept
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
                return expected<hash_map, error_code>(unexpect,
                                                      percent.error());
            }
            auto inserted = result->insert(other.begin(), other.end());
            if (!inserted) {
                return expected<hash_map, error_code>(unexpect,
                                                      inserted.error());
            }
            return result;
        }

        constexpr hash_map& operator=(const hash_map& other) noexcept
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

        constexpr hash_map& operator=(hash_map&& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type>)
        {
            if (this == &other) {
                return *this;
            }
            hash_map replacement(std::move(other), get_allocator());
            auto swapped = table_.swap(replacement.table_);
            if (!swapped) {
                panic_error(swapped.error());
            }
            return *this;
        }

        constexpr hash_map& operator=(
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
            return table_.emplace_unique(value.first, value);
        }

        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            value_type&& value) noexcept
            requires(std::is_nothrow_constructible_v<value_type, value_type &&>)
        {
            return table_.emplace_unique(value.first, std::move(value));
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
            requires(std::is_nothrow_constructible_v<value_type, value_type &&>)
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
                     std::is_nothrow_constructible_v<value_type, value_type &&>)
        constexpr expected<std::pair<iterator, bool>, error_code> emplace(
            Args&&... args) noexcept {
            value_type value(std::forward<Args>(args)...);
            return insert(std::move(value));
        }

        template <class... Args>
            requires(std::is_nothrow_constructible_v<value_type, Args && ...> &&
                     std::is_nothrow_constructible_v<value_type, value_type &&>)
        constexpr expected<iterator, error_code> emplace_hint(
            const_iterator, Args&&... args) noexcept {
            auto result = emplace(std::forward<Args>(args)...);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return result->first;
        }

        template <class... Args>
            requires(
                std::is_nothrow_constructible_v<mapped_type, Args && ...> &&
                std::is_nothrow_copy_constructible_v<key_type>)
        constexpr expected<std::pair<iterator, bool>, error_code> try_emplace(
            const key_type& key, Args&&... args) noexcept {
            iterator existing = find(key);
            if (existing != end()) {
                return std::pair<iterator, bool>{existing, false};
            }
            mapped_type mapped(std::forward<Args>(args)...);
            return table_.emplace_unique(key, key, std::move(mapped));
        }

        template <class... Args>
            requires(
                std::is_nothrow_constructible_v<mapped_type, Args && ...> &&
                std::is_nothrow_move_constructible_v<key_type>)
        constexpr expected<std::pair<iterator, bool>, error_code> try_emplace(
            key_type&& key, Args&&... args) noexcept {
            iterator existing = find(key);
            if (existing != end()) {
                return std::pair<iterator, bool>{existing, false};
            }
            mapped_type mapped(std::forward<Args>(args)...);
            return table_.emplace_unique(key, std::move(key),
                                         std::move(mapped));
        }

        template <class... Args>
        constexpr expected<iterator, error_code> try_emplace(
            const_iterator, const key_type& key, Args&&... args) noexcept {
            auto result = try_emplace(key, std::forward<Args>(args)...);
            if (!result) {
                return expected<iterator, error_code>(unexpect, result.error());
            }
            return result->first;
        }

        template <class M>
            requires(std::is_nothrow_assignable_v<mapped_type&, M &&> &&
                     std::is_nothrow_constructible_v<mapped_type, M &&> &&
                     std::is_nothrow_copy_constructible_v<key_type>)
        constexpr expected<std::pair<iterator, bool>, error_code>
        insert_or_assign(const key_type& key, M&& value) noexcept {
            iterator found = find(key);
            if (found != end()) {
                found->second = std::forward<M>(value);
                return std::pair<iterator, bool>{found, false};
            }
            return try_emplace(key, std::forward<M>(value));
        }

        template <class M>
            requires(std::is_nothrow_assignable_v<mapped_type&, M &&> &&
                     std::is_nothrow_constructible_v<mapped_type, M &&> &&
                     std::is_nothrow_move_constructible_v<key_type>)
        constexpr expected<std::pair<iterator, bool>, error_code>
        insert_or_assign(key_type&& key, M&& value) noexcept {
            iterator found = find(key);
            if (found != end()) {
                found->second = std::forward<M>(value);
                return std::pair<iterator, bool>{found, false};
            }
            return try_emplace(std::move(key), std::forward<M>(value));
        }

        [[nodiscard]] constexpr expected<mapped_type&, error_code> at(
            const key_type& key) noexcept {
            iterator found = find(key);
            if (found == end()) {
                return expected<mapped_type&, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            return found->second;
        }

        [[nodiscard]] constexpr expected<const mapped_type&, error_code> at(
            const key_type& key) const noexcept {
            const_iterator found = find(key);
            if (found == cend()) {
                return expected<const mapped_type&, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            }
            return found->second;
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
        constexpr expected<void, error_code> swap(hash_map& other) noexcept {
            return table_.swap(other.table_);
        }
    };

    template <class Key, class T, class Allocator, class Hash, class KeyEqual>
    constexpr expected<void, error_code> swap(
        hash_map<Key, T, Allocator, Hash, KeyEqual>& left,
        hash_map<Key, T, Allocator, Hash, KeyEqual>& right) noexcept {
        return left.swap(right);
    }

    template <class Key, class T, class Allocator, class Hash = std::hash<Key>,
              class KeyEqual = detail::hash_equal<Key>>
    using map = hash_map<Key, T, Allocator, Hash, KeyEqual>;
}  // namespace tay
