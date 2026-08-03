/**
 * @file set.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供无异常的动态和固定容量链式哈希集合。
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
#include <tay/utility.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        struct hash_set_storage_tag {};
        struct hash_set_hash_tag {};
        struct hash_set_equal_tag {};

        template <class Key>
        struct hash_set_node {
            Key value;
            hash_set_node* next = nullptr;

            template <class... Args>
            constexpr explicit hash_set_node(Args&&... args) noexcept(
                std::is_nothrow_constructible_v<Key, Args&&...>)
                : value(std::forward<Args>(args)...), next(nullptr) {}
        };
    }  // namespace detail

    template <class Key, class Allocator = allocator<Key>>
    class dynamic_hash_set_storage {
    public:
        using key_type                       = Key;
        using allocator_type                 = Allocator;
        using node_type                      = detail::hash_set_node<key_type>;
        using size_type                      = size_t;
        static constexpr bool fixed_capacity = false;

    private:
        using allocator_traits_type = allocator_traits<allocator_type>;
        using node_allocator   = typename allocator_traits_type::template rebind_alloc<node_type>;
        using node_traits      = allocator_traits<node_allocator>;
        using bucket_allocator = typename allocator_traits_type::template rebind_alloc<node_type*>;
        using bucket_traits    = allocator_traits<bucket_allocator>;

        [[no_unique_address]] allocator_type allocator_{};
        node_type** buckets_    = nullptr;
        size_type bucket_count_ = 0;

    public:
        constexpr dynamic_hash_set_storage() noexcept = default;
        constexpr dynamic_hash_set_storage(const allocator_type& allocator) noexcept
            : allocator_(allocator) {}

        dynamic_hash_set_storage(const dynamic_hash_set_storage&)            = delete;
        dynamic_hash_set_storage& operator=(const dynamic_hash_set_storage&) = delete;

        constexpr dynamic_hash_set_storage(dynamic_hash_set_storage&& other) noexcept
            : allocator_(std::move(other.allocator_)),
              buckets_(other.buckets_),
              bucket_count_(other.bucket_count_) {
            other.buckets_      = nullptr;
            other.bucket_count_ = 0;
        }

        constexpr dynamic_hash_set_storage& operator=(dynamic_hash_set_storage&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            release_buckets();
            allocator_          = std::move(other.allocator_);
            buckets_            = other.buckets_;
            bucket_count_       = other.bucket_count_;
            other.buckets_      = nullptr;
            other.bucket_count_ = 0;
            return *this;
        }

        constexpr ~dynamic_hash_set_storage() noexcept {
            release_buckets();
        }

        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }

        [[nodiscard]] constexpr node_type** buckets() noexcept {
            return buckets_;
        }
        [[nodiscard]] constexpr node_type* const* buckets() const noexcept {
            return buckets_;
        }
        [[nodiscard]] constexpr size_type bucket_count() const noexcept {
            return bucket_count_;
        }
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            node_allocator alloc(allocator_);
            return node_traits::max_size(alloc);
        }

        constexpr expected<void, error_code> init(size_type count) noexcept {
            if (bucket_count_ != 0) {
                return {};
            }
            return rehash(
                count == 0 ? 1 : count,
                [](const key_type&, size_type) constexpr noexcept { return size_type{0}; });
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<node_type*, error_code> create_node(
            Args&&... args) noexcept {
            node_allocator alloc(allocator_);
            auto memory = node_traits::try_allocate(alloc, 1);
            if (!memory) {
                return expected<node_type*, error_code>(unexpect, memory.error());
            }
            node_traits::construct(alloc, *memory, std::forward<Args>(args)...);
            return *memory;
        }

        constexpr void destroy_node(node_type* node) noexcept {
            node_allocator alloc(allocator_);
            node_traits::destroy(alloc, node);
            node_traits::deallocate(alloc, node, 1);
        }

        template <class Indexer>
        constexpr expected<void, error_code> rehash(size_type count, Indexer indexer) noexcept {
            if (count == 0) {
                count = 1;
            }
            bucket_allocator alloc(allocator_);
            auto memory = bucket_traits::try_allocate(alloc, count);
            if (!memory) {
                return expected<void, error_code>(unexpect, memory.error());
            }
            node_type** replacement = *memory;
            for (size_type i = 0; i < count; ++i) {
                replacement[i] = nullptr;
            }
            for (size_type i = 0; i < bucket_count_; ++i) {
                node_type* node = buckets_[i];
                while (node != nullptr) {
                    node_type* next        = node->next;
                    const size_type target = indexer(node->value, count);
                    node->next             = replacement[target];
                    replacement[target]    = node;
                    node                   = next;
                }
            }
            release_buckets();
            buckets_      = replacement;
            bucket_count_ = count;
            return {};
        }

    private:
        constexpr void release_buckets() noexcept {
            if (buckets_ == nullptr) {
                return;
            }
            bucket_allocator alloc(allocator_);
            bucket_traits::deallocate(alloc, buckets_, bucket_count_);
            buckets_      = nullptr;
            bucket_count_ = 0;
        }
    };

    template <class Key, size_t N>
    class static_hash_set_storage {
    public:
        using key_type                       = Key;
        using node_type                      = detail::hash_set_node<key_type>;
        using size_type                      = size_t;
        static constexpr bool fixed_capacity = true;

    private:
        struct slot_type {
            alignas(node_type) unsigned char memory[sizeof(node_type)];
            bool occupied = false;

            [[nodiscard]] constexpr node_type* node() noexcept {
                return std::launder(reinterpret_cast<node_type*>(memory));
            }
            [[nodiscard]] constexpr const node_type* node() const noexcept {
                return std::launder(reinterpret_cast<const node_type*>(memory));
            }
        };

        slot_type slots_[N == 0 ? 1 : N]{};
        node_type* buckets_[N == 0 ? 1 : N]{};
        size_type bucket_count_ = 0;

    public:
        constexpr static_hash_set_storage() noexcept                       = default;
        static_hash_set_storage(const static_hash_set_storage&)            = delete;
        static_hash_set_storage& operator=(const static_hash_set_storage&) = delete;
        constexpr static_hash_set_storage(static_hash_set_storage&& other) noexcept
            requires std::is_nothrow_move_constructible_v<key_type>
        {
            move_from(other);
        }
        constexpr static_hash_set_storage& operator=(static_hash_set_storage&& other) noexcept
            requires std::is_nothrow_move_constructible_v<key_type>
        {
            if (this != &other) {
                move_from(other);
            }
            return *this;
        }

        constexpr ~static_hash_set_storage() noexcept = default;

        [[nodiscard]] constexpr node_type** buckets() noexcept {
            return buckets_;
        }
        [[nodiscard]] constexpr node_type* const* buckets() const noexcept {
            return buckets_;
        }
        [[nodiscard]] constexpr size_type bucket_count() const noexcept {
            return bucket_count_;
        }
        [[nodiscard]] static constexpr size_type max_size() noexcept {
            return N;
        }

        constexpr expected<void, error_code> init(size_type) noexcept {
            bucket_count_ = N;
            return {};
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<node_type*, error_code> create_node(
            Args&&... args) noexcept {
            for (size_type i = 0; i < N; ++i) {
                if (!slots_[i].occupied) {
                    node_type* node = slots_[i].node();
                    static_cast<void>(std::construct_at(node, std::forward<Args>(args)...));
                    slots_[i].occupied = true;
                    return node;
                }
            }
            return expected<node_type*, error_code>(unexpect, error_code::OVERFLOW_ERROR);
        }

        constexpr void destroy_node(node_type* node) noexcept {
            for (size_type i = 0; i < N; ++i) {
                if (slots_[i].occupied && slots_[i].node() == node) {
                    std::destroy_at(node);
                    slots_[i].occupied = false;
                    return;
                }
            }
            tay::panic("static_hash_set received an unknown node");
        }

        template <class Indexer>
        constexpr expected<void, error_code> rehash(size_type count, Indexer) noexcept {
            if (count <= bucket_count_) {
                return {};
            }
            return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
        }

    private:
        [[nodiscard]] constexpr size_type index_of(const node_type* node) const noexcept {
            for (size_type i = 0; i < N; ++i) {
                if (slots_[i].occupied && slots_[i].node() == node) {
                    return i;
                }
            }
            tay::panic("static_hash_set has an invalid link");
        }

        constexpr void move_from(static_hash_set_storage& other) noexcept
            requires std::is_nothrow_move_constructible_v<key_type>
        {
            bucket_count_ = other.bucket_count_;
            for (size_type i = 0; i < N; ++i) {
                buckets_[i] = nullptr;
                if (other.slots_[i].occupied) {
                    static_cast<void>(std::construct_at(slots_[i].node(),
                                                        std::move(other.slots_[i].node()->value)));
                    slots_[i].occupied = true;
                }
            }
            for (size_type bucket = 0; bucket < bucket_count_; ++bucket) {
                node_type** link = &buckets_[bucket];
                for (node_type* source = other.buckets_[bucket]; source; source = source->next) {
                    node_type* target = slots_[other.index_of(source)].node();
                    *link             = target;
                    link              = &target->next;
                }
                *link = nullptr;
            }
            for (size_type i = 0; i < N; ++i) {
                if (other.slots_[i].occupied) {
                    std::destroy_at(other.slots_[i].node());
                    other.slots_[i].occupied = false;
                }
                other.buckets_[i] = nullptr;
            }
            other.bucket_count_ = 0;
        }
    };

    template <class Storage, class Key>
    concept hash_set_storage =
        requires(Storage& storage, const Storage& const_storage, size_t count) {
            typename Storage::key_type;
            typename Storage::node_type;
            requires std::same_as<typename Storage::key_type, Key>;
            {
                storage.buckets()
            };
            {
                const_storage.bucket_count()
            } -> std::convertible_to<size_t>;
            {
                const_storage.max_size()
            } -> std::convertible_to<size_t>;
            {
                storage.init(count)
            } -> std::same_as<expected<void, error_code>>;
        };

    template <class Key, class Storage, class Hash = std::hash<Key>,
              class KeyEqual = std::equal_to<Key>>
        requires hash_set_storage<Storage, Key>
    class basic_hash_set : private composition<detail::hash_set_storage_tag, Storage>,
                           private composition<detail::hash_set_hash_tag, Hash>,
                           private composition<detail::hash_set_equal_tag, KeyEqual> {
    public:
        using key_type        = Key;
        using value_type      = Key;
        using storage_type    = Storage;
        using hasher          = Hash;
        using key_equal       = KeyEqual;
        using node_type       = typename storage_type::node_type;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = const value_type&;
        using const_reference = const value_type&;
        using pointer         = const value_type*;
        using const_pointer   = const value_type*;

    private:
        struct empty_tag {};
        size_type size_             = 0;
        size_type max_load_percent_ = 100;

        [[nodiscard]] constexpr storage_type& storage() noexcept {
            return get<detail::hash_set_storage_tag>(this);
        }
        [[nodiscard]] constexpr const storage_type& storage() const noexcept {
            return get<detail::hash_set_storage_tag>(this);
        }
        [[nodiscard]] constexpr hasher& hash_ref() noexcept {
            return get<detail::hash_set_hash_tag>(this);
        }
        [[nodiscard]] constexpr const hasher& hash_ref() const noexcept {
            return get<detail::hash_set_hash_tag>(this);
        }
        [[nodiscard]] constexpr key_equal& equal_ref() noexcept {
            return get<detail::hash_set_equal_tag>(this);
        }
        [[nodiscard]] constexpr const key_equal& equal_ref() const noexcept {
            return get<detail::hash_set_equal_tag>(this);
        }

        constexpr basic_hash_set(empty_tag, storage_type storage, hasher hash,
                                 key_equal equal) noexcept
            : composition<detail::hash_set_storage_tag, Storage>(std::move(storage)),
              composition<detail::hash_set_hash_tag, Hash>(std::move(hash)),
              composition<detail::hash_set_equal_tag, KeyEqual>(std::move(equal)) {}

        [[noreturn]] static constexpr void panic_error(error_code error) {
            switch (error) {
                case error_code::OUT_OF_MEMORY: tay::panic("hash_set allocation failed");
                case error_code::OVERFLOW_ERROR:
                case error_code::ALLOCATION_SIZE_OVERFLOW:
                    tay::panic("hash_set capacity exhausted");
                default: tay::panic("hash_set operation failed");
            }
        }

        [[nodiscard]] constexpr size_type bucket_index(const key_type& key,
                                                       size_type count) const noexcept {
            return count == 0 ? 0 : static_cast<size_type>(hash_ref()(key)) % count;
        }

        [[nodiscard]] constexpr size_type bucket_limit(size_type buckets) const noexcept {
            if (max_load_percent_ == 0) {
                return 0;
            }
            const size_type maximum = size_type(-1);
            if (buckets > maximum / max_load_percent_) {
                return maximum;
            }
            return (buckets * max_load_percent_) / 100;
        }

        [[nodiscard]] constexpr size_type required_buckets(size_type elements) const noexcept {
            if (elements == 0) {
                return 1;
            }
            return (elements * 100 + max_load_percent_ - 1) / max_load_percent_;
        }

        constexpr expected<void, error_code> ensure_for_insert() noexcept {
            if (size_ == max_size()) {
                return expected<void, error_code>(
                    unexpect, storage_type::fixed_capacity ? error_code::OVERFLOW_ERROR
                                                           : error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            if (size_ + 1 <= bucket_limit(bucket_count())) {
                return {};
            }
            size_type target = required_buckets(size_ + 1);
            if constexpr (!storage_type::fixed_capacity) {
                const size_type current = bucket_count();
                if (current != 0 && current <= size_type(-1) - current && current * 2 > target) {
                    target = current * 2;
                }
            }
            return rehash(target);
        }

        [[nodiscard]] static constexpr storage_type copy_storage(
            const basic_hash_set& other) noexcept {
            if constexpr (requires { other.get_allocator(); }) {
                return storage_type(other.get_allocator());
            } else {
                return storage_type{};
            }
        }

    public:
        class iterator {
            friend class basic_hash_set;
            basic_hash_set* set_ = nullptr;
            size_type bucket_    = 0;
            node_type* node_     = nullptr;

            constexpr iterator(basic_hash_set* set, size_type bucket, node_type* node) noexcept
                : set_(set), bucket_(bucket), node_(node) {}

        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept  = std::forward_iterator_tag;
            using value_type        = Key;
            using difference_type   = std::ptrdiff_t;
            using reference         = const Key&;
            using pointer           = const Key*;

            constexpr iterator() noexcept = default;
            [[nodiscard]] constexpr reference operator*() const noexcept {
                return node_->value;
            }
            [[nodiscard]] constexpr pointer operator->() const noexcept {
                return &node_->value;
            }
            constexpr iterator& operator++() noexcept {
                node_ = node_->next;
                while (node_ == nullptr && bucket_ < set_->bucket_count()) {
                    ++bucket_;
                    if (bucket_ < set_->bucket_count()) {
                        node_ = set_->storage().buckets()[bucket_];
                    }
                }
                return *this;
            }
            constexpr iterator operator++(int) noexcept {
                iterator copy = *this;
                ++*this;
                return copy;
            }
            friend constexpr bool operator==(const iterator& left, const iterator& right) noexcept {
                return left.set_ == right.set_ && left.node_ == right.node_ &&
                       left.bucket_ == right.bucket_;
            }
        };

        using const_iterator = iterator;

        constexpr basic_hash_set() noexcept
            requires(std::is_nothrow_default_constructible_v<storage_type> &&
                     std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : basic_hash_set(size_type{1}) {}

        constexpr explicit basic_hash_set(size_type bucket_count) noexcept
            requires(std::is_nothrow_default_constructible_v<storage_type> &&
                     std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : basic_hash_set(bucket_count, hasher{}, key_equal{}, storage_type{}) {}

        constexpr explicit basic_hash_set(storage_type storage) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : basic_hash_set(1, hasher{}, key_equal{}, std::move(storage)) {}

        constexpr basic_hash_set(size_type bucket_count, hasher hash, key_equal equal,
                                 storage_type storage = {}) noexcept
            : basic_hash_set(empty_tag{}, std::move(storage), std::move(hash), std::move(equal)) {
            auto initialized = this->storage().init(bucket_count);
            if (!initialized) {
                panic_error(initialized.error());
            }
        }

        constexpr basic_hash_set(std::initializer_list<value_type> values) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_default_constructible_v<storage_type> &&
                     std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
            : basic_hash_set() {
            for (const auto& value : values) {
                auto result = insert(value);
                if (!result) {
                    panic_error(result.error());
                }
            }
        }

        constexpr basic_hash_set(const basic_hash_set& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_copy_constructible_v<hasher> &&
                     std::is_nothrow_copy_constructible_v<key_equal>)
            : basic_hash_set(other.bucket_count(), other.hash_function(), other.key_eq(),
                             copy_storage(other)) {
            max_load_percent_ = other.max_load_percent_;
            for (const auto& value : other) {
                auto inserted = insert(value);
                if (!inserted) {
                    panic_error(inserted.error());
                }
            }
        }

        constexpr basic_hash_set& operator=(const basic_hash_set& other) noexcept
            requires(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_copy_assignable_v<hasher> &&
                     std::is_nothrow_copy_assignable_v<key_equal>)
        {
            if (this != &other) {
                clear();
                hash_ref()        = other.hash_ref();
                equal_ref()       = other.equal_ref();
                max_load_percent_ = other.max_load_percent_;
                auto resized      = rehash(other.bucket_count());
                if (!resized) {
                    panic_error(resized.error());
                }
                for (const auto& value : other) {
                    auto inserted = insert(value);
                    if (!inserted) {
                        panic_error(inserted.error());
                    }
                }
            }
            return *this;
        }

        constexpr basic_hash_set(basic_hash_set&& other) noexcept
            requires(std::is_nothrow_move_constructible_v<storage_type> &&
                     std::is_nothrow_move_constructible_v<hasher> &&
                     std::is_nothrow_move_constructible_v<key_equal>)
            : composition<detail::hash_set_storage_tag, Storage>(std::move(other.storage())),
              composition<detail::hash_set_hash_tag, Hash>(std::move(other.hash_ref())),
              composition<detail::hash_set_equal_tag, KeyEqual>(std::move(other.equal_ref())),
              size_(other.size_),
              max_load_percent_(other.max_load_percent_) {
            other.size_ = 0;
        }

        constexpr basic_hash_set& operator=(basic_hash_set&& other) noexcept
            requires(std::is_nothrow_move_assignable_v<storage_type> &&
                     std::is_nothrow_move_assignable_v<hasher> &&
                     std::is_nothrow_move_assignable_v<key_equal>)
        {
            if (this != &other) {
                clear();
                storage()         = std::move(other.storage());
                hash_ref()        = std::move(other.hash_ref());
                equal_ref()       = std::move(other.equal_ref());
                size_             = other.size_;
                max_load_percent_ = other.max_load_percent_;
                other.size_       = 0;
            }
            return *this;
        }

        constexpr ~basic_hash_set() noexcept {
            clear();
        }

        [[nodiscard]] static constexpr expected<basic_hash_set, error_code> try_create(
            size_type bucket_count, hasher hash, key_equal equal, storage_type storage) noexcept {
            basic_hash_set result(empty_tag{}, std::move(storage), std::move(hash),
                                  std::move(equal));
            auto initialized = result.storage().init(bucket_count);
            if (!initialized) {
                return expected<basic_hash_set, error_code>(unexpect, initialized.error());
            }
            return result;
        }

        [[nodiscard]] static constexpr expected<basic_hash_set, error_code> try_create() noexcept
            requires(std::is_nothrow_default_constructible_v<storage_type> &&
                     std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
        {
            return try_create(1, hasher{}, key_equal{}, storage_type{});
        }

        [[nodiscard]] static constexpr expected<basic_hash_set, error_code> try_create(
            storage_type storage) noexcept
            requires(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>)
        {
            return try_create(1, hasher{}, key_equal{}, std::move(storage));
        }

        [[nodiscard]] static constexpr expected<basic_hash_set, error_code> try_create(
            const basic_hash_set& other, storage_type storage) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            auto result = try_create(other.bucket_count(), other.hash_function(), other.key_eq(),
                                     std::move(storage));
            if (!result) {
                return result;
            }
            result->max_load_percent_ = other.max_load_percent_;
            for (const auto& value : other) {
                auto inserted = result->insert(value);
                if (!inserted) {
                    return expected<basic_hash_set, error_code>(unexpect, inserted.error());
                }
            }
            return result;
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            for (size_type i = 0; i < bucket_count(); ++i) {
                if (storage().buckets()[i] != nullptr) {
                    return iterator(this, i, storage().buckets()[i]);
                }
            }
            return end();
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_cast<basic_hash_set*>(this)->begin();
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(this, bucket_count(), nullptr);
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_cast<basic_hash_set*>(this)->end();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return storage().max_size();
        }
        [[nodiscard]] constexpr size_type bucket_count() const noexcept {
            return storage().bucket_count();
        }
        [[nodiscard]] constexpr size_type bucket(const key_type& key) const noexcept {
            return bucket_index(key, bucket_count());
        }
        [[nodiscard]] constexpr size_type bucket_size(size_type index) const noexcept {
            if (index >= bucket_count()) {
                return 0;
            }
            size_type result = 0;
            for (node_type* node = storage().buckets()[index]; node; node = node->next) {
                ++result;
            }
            return result;
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

        [[nodiscard]] constexpr iterator find(const key_type& key) noexcept {
            if (bucket_count() == 0) {
                return end();
            }
            const size_type index = bucket_index(key, bucket_count());
            for (node_type* node = storage().buckets()[index]; node; node = node->next) {
                if (equal_ref()(node->value, key)) {
                    return iterator(this, index, node);
                }
            }
            return end();
        }
        [[nodiscard]] constexpr const_iterator find(const key_type& key) const noexcept {
            return const_cast<basic_hash_set*>(this)->find(key);
        }
        [[nodiscard]] constexpr bool contains(const key_type& key) const noexcept {
            return find(key) != end();
        }
        [[nodiscard]] constexpr size_type count(const key_type& key) const noexcept {
            return contains(key) ? 1 : 0;
        }
        [[nodiscard]] constexpr std::pair<iterator, iterator> equal_range(
            const key_type& key) noexcept {
            iterator first = find(key);
            if (first == end()) {
                return {first, first};
            }
            iterator last = first;
            ++last;
            return {first, last};
        }

        template <class... Args>
        [[nodiscard]] constexpr expected<std::pair<iterator, bool>, error_code> emplace(
            Args&&... args) noexcept {
            key_type value(std::forward<Args>(args)...);
            iterator existing = find(value);
            if (existing != end()) {
                return std::pair<iterator, bool>{existing, false};
            }
            auto room = ensure_for_insert();
            if (!room) {
                return expected<std::pair<iterator, bool>, error_code>(unexpect, room.error());
            }
            auto created = storage().create_node(std::move(value));
            if (!created) {
                return expected<std::pair<iterator, bool>, error_code>(unexpect, created.error());
            }
            const size_type index      = bucket_index((*created)->value, bucket_count());
            (*created)->next           = storage().buckets()[index];
            storage().buckets()[index] = *created;
            ++size_;
            return std::pair<iterator, bool>{iterator(this, index, *created), true};
        }
        [[nodiscard]] constexpr auto insert(const value_type& value) noexcept
            requires std::is_nothrow_copy_constructible_v<value_type>
        {
            return emplace(value);
        }
        [[nodiscard]] constexpr auto insert(value_type&& value) noexcept {
            return emplace(std::move(value));
        }

        constexpr iterator erase(const_iterator position) noexcept {
            if (position.set_ != this || position.node_ == nullptr ||
                position.bucket_ >= bucket_count())
            {
                return end();
            }
            node_type** link = &storage().buckets()[position.bucket_];
            while (*link != nullptr && *link != position.node_) {
                link = &(*link)->next;
            }
            if (*link == nullptr) {
                return end();
            }
            iterator next = position;
            ++next;
            node_type* erased = *link;
            *link             = erased->next;
            storage().destroy_node(erased);
            --size_;
            return next;
        }
        constexpr size_type erase(const key_type& key) noexcept {
            iterator found = find(key);
            if (found == end()) {
                return 0;
            }
            erase(found);
            return 1;
        }
        constexpr void clear() noexcept {
            for (size_type i = 0; i < bucket_count(); ++i) {
                node_type* node = storage().buckets()[i];
                while (node != nullptr) {
                    node_type* next = node->next;
                    storage().destroy_node(node);
                    node = next;
                }
                storage().buckets()[i] = nullptr;
            }
            size_ = 0;
        }

        constexpr expected<void, error_code> rehash(size_type count) noexcept {
            const size_type minimum = required_buckets(size_);
            if (count < minimum) {
                count = minimum;
            }
            return storage().rehash(count, [this](const key_type& key, size_type buckets) noexcept {
                return bucket_index(key, buckets);
            });
        }
        constexpr expected<void, error_code> reserve(size_type count) noexcept {
            if (count > max_size()) {
                return expected<void, error_code>(
                    unexpect, storage_type::fixed_capacity ? error_code::OVERFLOW_ERROR
                                                           : error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            const size_type required = required_buckets(count);
            return required > bucket_count() ? rehash(required) : expected<void, error_code>{};
        }

        [[nodiscard]] constexpr hasher hash_function() const noexcept {
            return hash_ref();
        }
        [[nodiscard]] constexpr key_equal key_eq() const noexcept {
            return equal_ref();
        }
        [[nodiscard]] constexpr auto get_allocator() const noexcept
            requires requires { storage().get_allocator(); }
        {
            return storage().get_allocator();
        }

        constexpr void swap(basic_hash_set& other) noexcept
            requires(std::is_nothrow_move_constructible_v<storage_type> &&
                     std::is_nothrow_move_assignable_v<storage_type> &&
                     std::is_nothrow_move_constructible_v<hasher> &&
                     std::is_nothrow_move_assignable_v<hasher> &&
                     std::is_nothrow_move_constructible_v<key_equal> &&
                     std::is_nothrow_move_assignable_v<key_equal>)
        {
            basic_hash_set temporary(std::move(other));
            other = std::move(*this);
            *this = std::move(temporary);
        }
    };

    template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
              class Allocator = allocator<Key>>
    using hash_set = basic_hash_set<Key, dynamic_hash_set_storage<Key, Allocator>, Hash, KeyEqual>;

    template <class Key, size_t N, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using static_hash_set = basic_hash_set<Key, static_hash_set_storage<Key, N>, Hash, KeyEqual>;
}  // namespace tay
