/**
 * @file slot_map.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供世代稳定句柄和稠密值迭代的 slot map。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/array_list.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/static_vector.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace tay {
    struct slot_map_handle {
        size_t index      = size_t(-1);
        size_t generation = 0;
        [[nodiscard]] friend constexpr bool operator==(const slot_map_handle&,
                                                       const slot_map_handle&) noexcept = default;
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return generation != 0;
        }
    };

    namespace detail {
        struct slot_map_slot {
            size_t dense      = 0;
            size_t generation = 1;
            size_t next_free  = size_t(-1);
            bool occupied     = false;
            bool retired      = false;
        };
    }  // namespace detail

    template <class T, class Allocator = allocator<T>>
    class dynamic_slot_map_storage {
        using allocator_traits_type = allocator_traits<Allocator>;
        using slot_allocator =
            typename allocator_traits_type::template rebind_alloc<detail::slot_map_slot>;
        using index_allocator = typename allocator_traits_type::template rebind_alloc<size_t>;
        array_list<T, Allocator> values_;
        array_list<detail::slot_map_slot, slot_allocator> slots_;
        array_list<size_t, index_allocator> dense_to_slot_;

    public:
        using value_type                              = T;
        static constexpr bool fixed_capacity          = false;
        constexpr dynamic_slot_map_storage() noexcept = default;
        constexpr explicit dynamic_slot_map_storage(const Allocator& allocator) noexcept
            : values_(allocator),
              slots_(slot_allocator(allocator)),
              dense_to_slot_(index_allocator(allocator)) {}
        [[nodiscard]] constexpr auto& values() noexcept {
            return values_;
        }
        [[nodiscard]] constexpr const auto& values() const noexcept {
            return values_;
        }
        [[nodiscard]] constexpr auto& slots() noexcept {
            return slots_;
        }
        [[nodiscard]] constexpr const auto& slots() const noexcept {
            return slots_;
        }
        [[nodiscard]] constexpr auto& dense_to_slot() noexcept {
            return dense_to_slot_;
        }
        [[nodiscard]] constexpr const auto& dense_to_slot() const noexcept {
            return dense_to_slot_;
        }
    };

    template <class T, size_t N>
    class static_slot_map_storage {
        static_vector<T, N> values_;
        static_vector<detail::slot_map_slot, N> slots_;
        static_vector<size_t, N> dense_to_slot_;

    public:
        using value_type                     = T;
        static constexpr bool fixed_capacity = true;
        [[nodiscard]] constexpr auto& values() noexcept {
            return values_;
        }
        [[nodiscard]] constexpr const auto& values() const noexcept {
            return values_;
        }
        [[nodiscard]] constexpr auto& slots() noexcept {
            return slots_;
        }
        [[nodiscard]] constexpr const auto& slots() const noexcept {
            return slots_;
        }
        [[nodiscard]] constexpr auto& dense_to_slot() noexcept {
            return dense_to_slot_;
        }
        [[nodiscard]] constexpr const auto& dense_to_slot() const noexcept {
            return dense_to_slot_;
        }
    };

    template <class Storage, class T>
    concept slot_map_storage = requires(Storage& storage, const Storage& const_storage) {
        typename Storage::value_type;
        requires std::same_as<typename Storage::value_type, T>;
        {
            storage.values()
        };
        {
            storage.slots()
        };
        {
            storage.dense_to_slot()
        };
        {
            const_storage.values()
        };
    };

    template <class T, class Storage>
        requires slot_map_storage<Storage, T>
    class basic_slot_map {
    public:
        using value_type     = T;
        using storage_type   = Storage;
        using handle_type    = slot_map_handle;
        using size_type      = size_t;
        using iterator       = decltype(std::declval<storage_type&>().values().begin());
        using const_iterator = decltype(std::declval<const storage_type&>().values().begin());
        inline static constexpr size_type npos = size_type(-1);

    private:
        storage_type storage_;
        size_type free_head_ = npos;

        [[nodiscard]] constexpr auto& values() noexcept {
            return storage_.values();
        }
        [[nodiscard]] constexpr const auto& values() const noexcept {
            return storage_.values();
        }
        [[nodiscard]] constexpr auto& slots() noexcept {
            return storage_.slots();
        }
        [[nodiscard]] constexpr const auto& slots() const noexcept {
            return storage_.slots();
        }
        [[nodiscard]] constexpr auto& dense_to_slot() noexcept {
            return storage_.dense_to_slot();
        }

        [[nodiscard]] constexpr error_code capacity_error() const noexcept {
            return storage_type::fixed_capacity ? error_code::OVERFLOW_ERROR
                                                : error_code::ALLOCATION_SIZE_OVERFLOW;
        }

    public:
        constexpr basic_slot_map() noexcept = default;
        constexpr explicit basic_slot_map(storage_type storage) noexcept
            : storage_(std::move(storage)) {}
        basic_slot_map(const basic_slot_map&)                          = delete;
        basic_slot_map& operator=(const basic_slot_map&)               = delete;
        constexpr basic_slot_map(basic_slot_map&&) noexcept            = default;
        constexpr basic_slot_map& operator=(basic_slot_map&&) noexcept = default;

        [[nodiscard]] constexpr bool empty() const noexcept {
            return values().empty();
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return values().size();
        }
        [[nodiscard]] constexpr iterator begin() noexcept {
            return values().begin();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return values().end();
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return values().begin();
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return values().end();
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<handle_type, error_code> emplace(Args&&... args) noexcept {
            size_type slot_index;
            bool appended_slot = false;
            if (free_head_ != npos) {
                slot_index = free_head_;
                free_head_ = slots()[slot_index].next_free;
            } else {
                detail::slot_map_slot fresh{};
                auto appended = slots().push_back(fresh);
                if (!appended) {
                    return expected<handle_type, error_code>(unexpect, appended.error());
                }
                slot_index    = slots().size() - 1;
                appended_slot = true;
            }

            auto value = values().emplace_back(std::forward<Args>(args)...);
            if (!value) {
                if (appended_slot) {
                    static_cast<void>(slots().pop_back());
                } else {
                    slots()[slot_index].next_free = free_head_;
                    free_head_                    = slot_index;
                }
                return expected<handle_type, error_code>(unexpect, value.error());
            }
            auto reverse = dense_to_slot().push_back(slot_index);
            if (!reverse) {
                static_cast<void>(values().pop_back());
                if (appended_slot) {
                    static_cast<void>(slots().pop_back());
                } else {
                    slots()[slot_index].next_free = free_head_;
                    free_head_                    = slot_index;
                }
                return expected<handle_type, error_code>(unexpect, reverse.error());
            }
            auto& slot     = slots()[slot_index];
            slot.dense     = values().size() - 1;
            slot.occupied  = true;
            slot.next_free = npos;
            return handle_type{slot_index, slot.generation};
        }

        [[nodiscard]] constexpr expected<handle_type, error_code> insert(
            const value_type& value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            return emplace(value);
        }
        [[nodiscard]] constexpr expected<handle_type, error_code> insert(
            value_type&& value) noexcept {
            return emplace(std::move(value));
        }

        [[nodiscard]] constexpr bool contains(handle_type handle) const noexcept {
            return handle.index < slots().size() && slots()[handle.index].occupied &&
                   slots()[handle.index].generation == handle.generation;
        }
        [[nodiscard]] constexpr value_type* get(handle_type handle) noexcept {
            return contains(handle) ? &values()[slots()[handle.index].dense] : nullptr;
        }
        [[nodiscard]] constexpr const value_type* get(handle_type handle) const noexcept {
            return contains(handle) ? &values()[slots()[handle.index].dense] : nullptr;
        }
        [[nodiscard]] constexpr expected<value_type&, error_code> at(handle_type handle) noexcept {
            value_type* value = get(handle);
            if (value == nullptr)
                return expected<value_type&, error_code>(unexpect, error_code::OUT_OF_RANGE);
            return *value;
        }

        constexpr expected<void, error_code> erase(handle_type handle) noexcept {
            if (!contains(handle))
                return expected<void, error_code>(unexpect, error_code::OUT_OF_RANGE);
            auto& slot            = slots()[handle.index];
            const size_type dense = slot.dense;
            const size_type last  = values().size() - 1;
            if (dense != last) {
                values()[dense]            = std::move(values()[last]);
                const size_type moved_slot = dense_to_slot()[last];
                dense_to_slot()[dense]     = moved_slot;
                slots()[moved_slot].dense  = dense;
            }
            static_cast<void>(values().pop_back());
            static_cast<void>(dense_to_slot().pop_back());
            slot.occupied = false;
            ++slot.generation;
            if (slot.generation == 0) {
                slot.retired = true;
            } else {
                slot.next_free = free_head_;
                free_head_     = handle.index;
            }
            return {};
        }

        [[nodiscard]] constexpr expected<value_type, error_code> extract(
            handle_type handle) noexcept {
            value_type* value = get(handle);
            if (value == nullptr)
                return expected<value_type, error_code>(unexpect, error_code::OUT_OF_RANGE);
            value_type result(std::move(*value));
            static_cast<void>(erase(handle));
            return result;
        }

        constexpr void clear() noexcept {
            while (!values().empty()) {
                const size_type slot_index = dense_to_slot()[values().size() - 1];
                auto& slot                 = slots()[slot_index];
                handle_type handle{slot_index, slot.generation};
                static_cast<void>(erase(handle));
            }
        }
    };

    template <class T, class Allocator = allocator<T>>
    using slot_map = basic_slot_map<T, dynamic_slot_map_storage<T, Allocator>>;

    template <class T, size_t N>
    using static_slot_map = basic_slot_map<T, static_slot_map_storage<T, N>>;
}  // namespace tay
