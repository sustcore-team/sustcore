/**
 * @file flat.h
 * @brief Sorted contiguous flat set and flat map containers.
 */

#pragma once

#include <tay/algo/binary_search.h>
#include <tay/array_list.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/utility.h>

#include <cstddef>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        struct flat_compare_tag {};
    }

    template <class Key, class Sequence, class Compare = std::ranges::less>
    class basic_flat_set
        : private composition<detail::flat_compare_tag, Compare> {
    public:
        using key_type = Key;
        using value_type = Key;
        using sequence_type = Sequence;
        using key_compare = Compare;
        using size_type = typename sequence_type::size_type;
        using iterator = typename sequence_type::const_iterator;
        using const_iterator = typename sequence_type::const_iterator;

    private:
        sequence_type values_;
        [[nodiscard]] constexpr key_compare& compare() noexcept {
            return get<detail::flat_compare_tag>(this);
        }
        [[nodiscard]] constexpr const key_compare& compare() const noexcept {
            return get<detail::flat_compare_tag>(this);
        }

    public:
        constexpr basic_flat_set() noexcept = default;
        constexpr explicit basic_flat_set(sequence_type sequence,
                                          key_compare compare = {}) noexcept
            : composition<detail::flat_compare_tag, Compare>(
                  std::move(compare)),
              values_(std::move(sequence)) {}

        [[nodiscard]] constexpr iterator begin() const noexcept {
            return values_.begin();
        }
        [[nodiscard]] constexpr iterator end() const noexcept {
            return values_.end();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return values_.empty();
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return values_.size();
        }
        [[nodiscard]] constexpr iterator lower_bound(const key_type& key) const {
            return tay::lower_bound(values_, key, compare());
        }
        [[nodiscard]] constexpr iterator find(const key_type& key) const {
            iterator position = lower_bound(key);
            if (position == end() || compare()(key, *position) ||
                compare()(*position, key))
                return end();
            return position;
        }
        [[nodiscard]] constexpr bool contains(const key_type& key) const {
            return find(key) != end();
        }
        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            const value_type& value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            iterator position = lower_bound(value);
            if (position != end() && !compare()(value, *position) &&
                !compare()(*position, value))
                return std::pair<iterator, bool>{position, false};
            auto inserted = values_.insert(position, value);
            if (!inserted)
                return expected<std::pair<iterator, bool>, error_code>(
                    unexpect, inserted.error());
            return std::pair<iterator, bool>{*inserted, true};
        }
        constexpr expected<std::pair<iterator, bool>, error_code> insert(
            value_type&& value) noexcept {
            iterator position = lower_bound(value);
            if (position != end() && !compare()(value, *position) &&
                !compare()(*position, value))
                return std::pair<iterator, bool>{position, false};
            auto inserted = values_.insert(position, std::move(value));
            if (!inserted)
                return expected<std::pair<iterator, bool>, error_code>(
                    unexpect, inserted.error());
            return std::pair<iterator, bool>{*inserted, true};
        }
        constexpr size_type erase(const key_type& key) noexcept {
            iterator position = find(key);
            if (position == end()) return 0;
            static_cast<void>(values_.erase(position));
            return 1;
        }
        constexpr void clear() noexcept { values_.clear(); }
    };

    template <class Key, class Compare = std::ranges::less,
              class Allocator = allocator<Key>>
    using flat_set = basic_flat_set<Key, array_list<Key, Allocator>, Compare>;

    template <class Key, class T>
    struct flat_map_storage_value {
        Key key;
        T mapped;
    };

    template <class Key, class T, class Sequence,
              class Compare = std::ranges::less>
    class basic_flat_map
        : private composition<detail::flat_compare_tag, Compare> {
        using stored_type = flat_map_storage_value<Key, T>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using sequence_type = Sequence;
        using key_compare = Compare;
        using size_type = typename sequence_type::size_type;

        struct reference {
            const key_type& first;
            mapped_type& second;
        };
        struct const_reference {
            const key_type& first;
            const mapped_type& second;
        };

    private:
        sequence_type values_;
        [[nodiscard]] constexpr key_compare& compare() noexcept {
            return get<detail::flat_compare_tag>(this);
        }
        [[nodiscard]] constexpr const key_compare& compare() const noexcept {
            return get<detail::flat_compare_tag>(this);
        }

        template <bool Constant>
        class basic_iterator {
            friend class basic_flat_map;
            using base_iterator = std::conditional_t<
                Constant, typename sequence_type::const_iterator,
                typename sequence_type::iterator>;
            base_iterator current_{};
            constexpr explicit basic_iterator(base_iterator current) noexcept
                : current_(current) {}

        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = std::pair<const Key, T>;
            using difference_type = std::ptrdiff_t;
            using reference_type = std::conditional_t<
                Constant, typename basic_flat_map::const_reference,
                typename basic_flat_map::reference>;
            struct arrow_proxy {
                reference_type value;
                [[nodiscard]] constexpr const reference_type* operator->()
                    const noexcept { return &value; }
            };
            constexpr basic_iterator() noexcept = default;
            [[nodiscard]] constexpr reference_type operator*() const noexcept {
                return {current_->key, current_->mapped};
            }
            [[nodiscard]] constexpr arrow_proxy operator->() const noexcept {
                return {**this};
            }
            constexpr basic_iterator& operator++() noexcept { ++current_; return *this; }
            constexpr basic_iterator operator++(int) noexcept { auto c=*this; ++*this; return c; }
            constexpr basic_iterator& operator--() noexcept { --current_; return *this; }
            constexpr basic_iterator& operator+=(difference_type n) noexcept { current_ += n; return *this; }
            constexpr basic_iterator& operator-=(difference_type n) noexcept { current_ -= n; return *this; }
            [[nodiscard]] constexpr basic_iterator operator+(difference_type n) const noexcept { auto c=*this; return c+=n; }
            [[nodiscard]] constexpr basic_iterator operator-(difference_type n) const noexcept { auto c=*this; return c-=n; }
            [[nodiscard]] constexpr difference_type operator-(const basic_iterator& other) const noexcept { return current_-other.current_; }
            [[nodiscard]] friend constexpr bool operator==(const basic_iterator&, const basic_iterator&) noexcept = default;
            [[nodiscard]] friend constexpr auto operator<=>(const basic_iterator&, const basic_iterator&) noexcept = default;
        };

    public:
        using iterator = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

        constexpr basic_flat_map() noexcept = default;
        constexpr explicit basic_flat_map(sequence_type sequence,
                                          key_compare compare = {}) noexcept
            : composition<detail::flat_compare_tag, Compare>(
                  std::move(compare)),
              values_(std::move(sequence)) {}

        [[nodiscard]] constexpr iterator begin() noexcept {
            return iterator(values_.begin());
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(values_.end());
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_iterator(values_.begin());
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(values_.end());
        }
        [[nodiscard]] constexpr bool empty() const noexcept { return values_.empty(); }
        [[nodiscard]] constexpr size_type size() const noexcept { return values_.size(); }

        [[nodiscard]] constexpr iterator lower_bound(const key_type& key) {
            auto position = tay::lower_bound(
                values_, key, compare(),
                [](const stored_type& value) -> const key_type& {
                    return value.key;
                });
            return iterator(position);
        }
        [[nodiscard]] constexpr const_iterator lower_bound(
            const key_type& key) const {
            auto position = tay::lower_bound(
                values_, key, compare(),
                [](const stored_type& value) -> const key_type& {
                    return value.key;
                });
            return const_iterator(position);
        }
        [[nodiscard]] constexpr iterator find(const key_type& key) {
            auto position = lower_bound(key);
            if (position == end()) return end();
            auto value = *position;
            return compare()(key, value.first) || compare()(value.first, key)
                       ? end() : position;
        }
        [[nodiscard]] constexpr const_iterator find(const key_type& key) const {
            auto position = lower_bound(key);
            if (position == end()) return end();
            auto value = *position;
            return compare()(key, value.first) || compare()(value.first, key)
                       ? end() : position;
        }
        [[nodiscard]] constexpr bool contains(const key_type& key) const {
            return find(key) != end();
        }
        [[nodiscard]] constexpr expected<mapped_type&, error_code> at(
            const key_type& key) noexcept {
            auto position = find(key);
            if (position == end())
                return expected<mapped_type&, error_code>(
                    unexpect, error_code::OUT_OF_RANGE);
            return (*position).second;
        }

        template <class... Args>
        constexpr expected<std::pair<iterator, bool>, error_code> try_emplace(
            const key_type& key, Args&&... args) noexcept {
            auto position = lower_bound(key);
            if (position != end()) {
                auto value = *position;
                if (!compare()(key, value.first) &&
                    !compare()(value.first, key))
                    return std::pair<iterator, bool>{position, false};
            }
            stored_type stored{key,
                               mapped_type(std::forward<Args>(args)...)};
            auto inserted = values_.insert(position.current_,
                                           std::move(stored));
            if (!inserted)
                return expected<std::pair<iterator, bool>, error_code>(
                    unexpect, inserted.error());
            return std::pair<iterator, bool>{iterator(*inserted), true};
        }
        template <class M>
        constexpr expected<std::pair<iterator, bool>, error_code>
        insert_or_assign(const key_type& key, M&& mapped) noexcept {
            auto position = find(key);
            if (position != end()) {
                (*position).second = std::forward<M>(mapped);
                return std::pair<iterator, bool>{position, false};
            }
            return try_emplace(key, std::forward<M>(mapped));
        }
        constexpr size_type erase(const key_type& key) noexcept {
            auto position = find(key);
            if (position == end()) return 0;
            static_cast<void>(values_.erase(position.current_));
            return 1;
        }
        constexpr void clear() noexcept { values_.clear(); }
    };

    template <class Key, class T, class Compare = std::ranges::less,
              class Allocator = allocator<flat_map_storage_value<Key, T>>>
    using flat_map = basic_flat_map<
        Key, T, array_list<flat_map_storage_value<Key, T>, Allocator>, Compare>;
}  // namespace tay
