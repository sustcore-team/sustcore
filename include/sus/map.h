/**
 * @file map.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 映射
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>
#include <sus/optional.h>
#include <sus/pair.h>

#include <concepts>
#include <functional>

namespace util {
    template <typename Map, typename _K, typename _V>
    concept MapType = requires(Map m, _K k, _V v) {
        {
            m.get(k)
        } -> std::same_as<Optional<_V>>;
        {
            m.put(k, v)
        } -> std::same_as<void>;
        {
            m.remove(k)
        } -> std::same_as<void>;
        {
            m.contains(k)
        } -> std::same_as<bool>;
        {
            m.empty()
        } -> std::same_as<bool>;
        {
            m.size()
        } -> std::same_as<size_t>;
    };

    template <typename _K, typename _V, typename Equality = std::equal_to<_K>>
    class LinkedMap;

    template <typename _K, typename _V>
    class LinkedMapIterator {
    public:
        using Entry = typename LinkedMap<_K, _V>::Entry;
        using Pair = util::Pair<_K, _V>;
        using ListType = typename LinkedMap<_K, _V>::ListType;
    private:
        typename ListType::iterator D_it;
    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = Pair;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Pair*;
        using reference         = Pair&;

        constexpr explicit LinkedMapIterator(typename ListType::iterator it) noexcept
            : D_it(it) {}

        // 解引用
        constexpr reference operator*() noexcept {
            return D_it->pair;
        }

        constexpr pointer operator->() noexcept {
            return &D_it->pair;
        }

        // 迭代操作
        constexpr LinkedMapIterator& operator++() noexcept {
            ++D_it;
            return *this;
        }

        constexpr LinkedMapIterator operator++(int) noexcept {
            LinkedMapIterator temp = *this;
            ++D_it;
            return temp;
        }

        constexpr LinkedMapIterator& operator--() noexcept {
            --D_it;
            return *this;
        }

        constexpr LinkedMapIterator operator--(int) noexcept {
            LinkedMapIterator temp = *this;
            --D_it;
            return temp;
        }

        // 比较
        constexpr bool operator==(const LinkedMapIterator& other) const noexcept {
            return D_it == other.D_it;
        }

        constexpr bool operator!=(const LinkedMapIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class LinkedMap<_K, _V>;
    };

    template <typename _K, typename _V>
    class LinkedMapConstIterator {
    public:
        using Entry = typename LinkedMap<_K, _V>::Entry;
        using Pair = util::Pair<_K, _V>;
        using ListType = typename LinkedMap<_K, _V>::ListType;
    private:
        const typename ListType::iterator D_it;
    public:
        // iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = Pair;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const Pair*;
        using reference         = const Pair&;

        constexpr explicit LinkedMapConstIterator(const typename ListType::iterator it) noexcept
            : D_it(it) {}
        constexpr LinkedMapConstIterator(const LinkedMapIterator<_K, _V>& it) noexcept
            : D_it(it.operator->()) {}

        // 解引用
        constexpr reference operator*() const noexcept {
            return D_it->pair;
        }

        constexpr pointer operator->() const noexcept {
            return &D_it->pair;
        }

        // 迭代操作
        constexpr LinkedMapConstIterator& operator++() noexcept {
            ++D_it;
            return *this;
        }

        constexpr LinkedMapConstIterator operator++(int) noexcept {
            LinkedMapConstIterator temp = *this;
            ++D_it;
            return temp;
        }

        constexpr LinkedMapConstIterator& operator--() noexcept {
            --D_it;
            return *this;
        }

        constexpr LinkedMapConstIterator operator--(int) noexcept {
            LinkedMapConstIterator temp = *this;
            --D_it;
            return temp;
        }

        // 比较
        constexpr bool operator==(const LinkedMapConstIterator& other) const noexcept {
            return D_it == other.D_it;
        }

        constexpr bool operator!=(const LinkedMapConstIterator& other) const noexcept {
            return !(*this == other);
        }

        friend class LinkedMap<_K, _V>;
    };

    template <typename _K, typename _V, typename Equality>
    class LinkedMap {
    public:
        struct Entry {
            Pair<_K, _V> pair;
            ListHead<Entry> list_head;
        };
        using ListType = IntrusiveList<Entry>;

        // iterators
        using iterator       = LinkedMapIterator<_K, _V>;
        using const_iterator = LinkedMapConstIterator<_K, _V>;
        using size_type      = std::size_t;

    private:
        ListType entries;

    public:
        ~LinkedMap() {
            for (auto &e : entries) {
                delete &e;
            }
        }
        Optional<_V> get(const _K &key) {
            for (auto &e : entries) {
                if (Equality()(e.pair.first, key)) {
                    return Optional<_V>(e.pair.second);
                }
            }
            return Optional<_V>();
        }
        Optional<Pair<_K,  _V>> get_entry(const _K &key) {
            for (auto &e : entries) {
                if (Equality()(e.pair.first, key)) {
                    return Optional<Pair<_K,  _V>>(e.pair);
                }
            }
            return Optional<Pair<_K,  _V>>();
        }
        void put(const _K &key, const _V &value) {
            for (auto &e : entries) {
                if (Equality()(e.pair.first, key)) {
                    e.pair.second = value;
                    return;
                }
            }
            // Key not found, insert new entry
            Entry *new_entry = new Entry{Pair<_K, _V>(key, value)};
            entries.push_back(*new_entry);
        }
        void remove(const _K &key) {
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                if (Equality()(it->pair.first, key)) {
                    entries.erase(it);
                    delete &*it;
                    return;
                }
            }
        }
        bool contains(const _K &key) const noexcept {
            for (auto &e : entries) {
                if (Equality()(e.pair.first, key)) {
                    return true;
                }
            }
            return false;
        }

        constexpr bool empty() const noexcept {
            return entries.empty();
        }
        constexpr size_t size() const {
            return entries.size();
        }

        // iterators
        constexpr iterator begin() noexcept {
            return iterator(entries.begin());
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(entries.begin());
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(entries.cbegin());
        }
        constexpr iterator end() noexcept {
            return iterator(entries.end());
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(entries.end());
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(entries.cend());
        }
    };

    static_assert(MapType<LinkedMap<int, int>, int, int>);
}  // namespace util