/**
 * @file bitmap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供基于存储策略的静态和动态位图。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/allocator.h>
#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace tay {
    template <class Allocator = allocator<u64_t>>
    class dynamic_bitmap_storage {
    public:
        using word_type      = u64_t;
        using allocator_type = Allocator;
        using size_type      = size_t;

    private:
        [[no_unique_address]] allocator_type allocator_{};
        word_type* words_     = nullptr;
        size_type word_count_ = 0;
        size_type bit_count_  = 0;

    public:
        constexpr dynamic_bitmap_storage() noexcept                      = default;
        dynamic_bitmap_storage(const dynamic_bitmap_storage&)            = delete;
        dynamic_bitmap_storage& operator=(const dynamic_bitmap_storage&) = delete;
        constexpr dynamic_bitmap_storage(dynamic_bitmap_storage&& other) noexcept
            : allocator_(std::move(other.allocator_)),
              words_(other.words_),
              word_count_(other.word_count_),
              bit_count_(other.bit_count_) {
            other.words_      = nullptr;
            other.word_count_ = other.bit_count_ = 0;
        }
        constexpr dynamic_bitmap_storage& operator=(dynamic_bitmap_storage&& other) noexcept {
            if (this != &other) {
                reset();
                allocator_        = std::move(other.allocator_);
                words_            = other.words_;
                word_count_       = other.word_count_;
                bit_count_        = other.bit_count_;
                other.words_      = nullptr;
                other.word_count_ = other.bit_count_ = 0;
            }
            return *this;
        }
        constexpr ~dynamic_bitmap_storage() noexcept {
            reset();
        }

        [[nodiscard]] static constexpr expected<dynamic_bitmap_storage, error_code> try_create(
            size_type bit_count, const allocator_type& allocator = {}) noexcept {
            dynamic_bitmap_storage result;
            result.allocator_  = allocator;
            result.bit_count_  = bit_count;
            result.word_count_ = (bit_count + 63) / 64;
            if (result.word_count_ == 0) {
                return result;
            }
            auto memory = allocator_traits<allocator_type>::try_allocate(result.allocator_,
                                                                         result.word_count_);
            if (!memory) {
                return expected<dynamic_bitmap_storage, error_code>(unexpect, memory.error());
            }
            result.words_ = *memory;
            for (size_type i = 0; i < result.word_count_; ++i) {
                result.words_[i] = 0;
            }
            return result;
        }

        [[nodiscard]] constexpr word_type* words() noexcept {
            return words_;
        }
        [[nodiscard]] constexpr const word_type* words() const noexcept {
            return words_;
        }
        [[nodiscard]] constexpr size_type word_count() const noexcept {
            return word_count_;
        }
        [[nodiscard]] constexpr size_type bit_count() const noexcept {
            return bit_count_;
        }
        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return allocator_;
        }

    private:
        constexpr void reset() noexcept {
            if (words_ != nullptr) {
                allocator_traits<allocator_type>::deallocate(allocator_, words_, word_count_);
            }
            words_      = nullptr;
            word_count_ = bit_count_ = 0;
        }
    };

    template <size_t N>
    class static_bitmap_storage {
    public:
        using word_type                              = u64_t;
        using size_type                              = size_t;
        static constexpr size_type static_word_count = (N + 63) / 64;

    private:
        word_type words_[static_word_count == 0 ? 1 : static_word_count]{};

    public:
        [[nodiscard]] constexpr word_type* words() noexcept {
            return words_;
        }
        [[nodiscard]] constexpr const word_type* words() const noexcept {
            return words_;
        }
        [[nodiscard]] static constexpr size_type word_count() noexcept {
            return static_word_count;
        }
        [[nodiscard]] static constexpr size_type bit_count() noexcept {
            return N;
        }
    };

    template <class Storage>
    concept bitmap_storage = requires(Storage& storage, const Storage& const_storage) {
        typename Storage::word_type;
        {
            storage.words()
        };
        {
            const_storage.word_count()
        } -> std::convertible_to<size_t>;
        {
            const_storage.bit_count()
        } -> std::convertible_to<size_t>;
    };

    template <bitmap_storage Storage>
    class basic_bitmap {
    public:
        using storage_type                     = Storage;
        using word_type                        = typename storage_type::word_type;
        using size_type                        = size_t;
        inline static constexpr size_type npos = size_type(-1);

        class reference {
            basic_bitmap* bitmap_;
            size_type index_;

        public:
            constexpr reference(basic_bitmap& bitmap, size_type index) noexcept
                : bitmap_(&bitmap), index_(index) {}
            constexpr reference& operator=(bool value) noexcept {
                bitmap_->set_unchecked(index_, value);
                return *this;
            }
            constexpr reference& operator=(const reference& other) noexcept {
                return *this = static_cast<bool>(other);
            }
            [[nodiscard]] constexpr operator bool() const noexcept {
                return bitmap_->test_unchecked(index_);
            }
            constexpr reference& flip() noexcept {
                bitmap_->flip_unchecked(index_);
                return *this;
            }
        };

    private:
        storage_type storage_;

        constexpr explicit basic_bitmap(storage_type storage) noexcept
            : storage_(std::move(storage)) {}

        constexpr void mask_tail() noexcept {
            const size_type remainder = size() % 64;
            if (remainder != 0 && storage_.word_count() != 0) {
                storage_.words()[storage_.word_count() - 1] &= (word_type{1} << remainder) - 1;
            }
        }
        [[nodiscard]] constexpr bool test_unchecked(size_type index) const noexcept {
            return (storage_.words()[index / 64] & (word_type{1} << (index % 64))) != 0;
        }
        constexpr void set_unchecked(size_type index, bool value) noexcept {
            const word_type mask = word_type{1} << (index % 64);
            word_type& word      = storage_.words()[index / 64];
            word                 = value ? word | mask : word & ~mask;
        }
        constexpr void flip_unchecked(size_type index) noexcept {
            storage_.words()[index / 64] ^= word_type{1} << (index % 64);
        }

    public:
        constexpr basic_bitmap() noexcept
            requires std::is_nothrow_default_constructible_v<storage_type>
        = default;

        constexpr explicit basic_bitmap(size_type bit_count) noexcept
            requires requires { storage_type::try_create(bit_count); }
        {
            auto storage = storage_type::try_create(bit_count);
            if (!storage) {
                tay::panic("bitmap allocation failed");
            }
            storage_ = std::move(*storage);
        }

        [[nodiscard]] static constexpr expected<basic_bitmap, error_code> try_create(
            size_type bit_count) noexcept
            requires requires { storage_type::try_create(bit_count); }
        {
            auto storage = storage_type::try_create(bit_count);
            if (!storage) {
                return expected<basic_bitmap, error_code>(unexpect, storage.error());
            }
            return basic_bitmap(std::move(*storage));
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return storage_.bit_count();
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        }
        [[nodiscard]] constexpr bool operator[](size_type index) const noexcept {
            return test_unchecked(index);
        }
        [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
            return reference(*this, index);
        }
        [[nodiscard]] constexpr expected<bool, error_code> test(size_type index) const noexcept {
            if (index >= size()) {
                return expected<bool, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            return test_unchecked(index);
        }
        constexpr expected<void, error_code> set(size_type index, bool value = true) noexcept {
            if (index >= size()) {
                return expected<void, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            set_unchecked(index, value);
            return {};
        }
        constexpr expected<void, error_code> reset(size_type index) noexcept {
            return set(index, false);
        }
        constexpr expected<void, error_code> flip(size_type index) noexcept {
            if (index >= size()) {
                return expected<void, error_code>(unexpect, error_code::OUT_OF_RANGE);
            }
            flip_unchecked(index);
            return {};
        }
        constexpr basic_bitmap& set() noexcept {
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] = ~word_type{0};
            }
            mask_tail();
            return *this;
        }
        constexpr basic_bitmap& reset() noexcept {
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] = 0;
            }
            return *this;
        }
        constexpr basic_bitmap& flip() noexcept {
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] = ~storage_.words()[i];
            }
            mask_tail();
            return *this;
        }
        [[nodiscard]] constexpr size_type count() const noexcept {
            size_type result = 0;
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                result += static_cast<size_type>(__builtin_popcountll(storage_.words()[i]));
            }
            return result;
        }
        [[nodiscard]] constexpr bool any() const noexcept {
            return count() != 0;
        }
        [[nodiscard]] constexpr bool none() const noexcept {
            return !any();
        }
        [[nodiscard]] constexpr bool all() const noexcept {
            return count() == size();
        }
        [[nodiscard]] constexpr size_type find_first_set(size_type start = 0) const noexcept {
            for (size_type i = start; i < size(); ++i) {
                if (test_unchecked(i)) {
                    return i;
                }
            }
            return npos;
        }
        [[nodiscard]] constexpr size_type find_first_clear(size_type start = 0) const noexcept {
            for (size_type i = start; i < size(); ++i) {
                if (!test_unchecked(i)) {
                    return i;
                }
            }
            return npos;
        }
        [[nodiscard]] constexpr word_type* words() noexcept {
            return storage_.words();
        }
        [[nodiscard]] constexpr const word_type* words() const noexcept {
            return storage_.words();
        }

        constexpr basic_bitmap& operator&=(const basic_bitmap& other) noexcept {
            if (size() != other.size()) {
                tay::panic("bitmap extent mismatch");
            }
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] &= other.storage_.words()[i];
            }
            return *this;
        }
        constexpr basic_bitmap& operator|=(const basic_bitmap& other) noexcept {
            if (size() != other.size()) {
                tay::panic("bitmap extent mismatch");
            }
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] |= other.storage_.words()[i];
            }
            return *this;
        }
        constexpr basic_bitmap& operator^=(const basic_bitmap& other) noexcept {
            if (size() != other.size()) {
                tay::panic("bitmap extent mismatch");
            }
            for (size_type i = 0; i < storage_.word_count(); ++i) {
                storage_.words()[i] ^= other.storage_.words()[i];
            }
            return *this;
        }
    };

    template <bitmap_storage Storage>
    [[nodiscard]] constexpr basic_bitmap<Storage> operator&(
        basic_bitmap<Storage> left, const basic_bitmap<Storage>& right) noexcept {
        left &= right;
        return left;
    }
    template <bitmap_storage Storage>
    [[nodiscard]] constexpr basic_bitmap<Storage> operator|(
        basic_bitmap<Storage> left, const basic_bitmap<Storage>& right) noexcept {
        left |= right;
        return left;
    }
    template <bitmap_storage Storage>
    [[nodiscard]] constexpr basic_bitmap<Storage> operator^(
        basic_bitmap<Storage> left, const basic_bitmap<Storage>& right) noexcept {
        left ^= right;
        return left;
    }

    template <class Allocator = allocator<u64_t>>
    using bitmap = basic_bitmap<dynamic_bitmap_storage<Allocator>>;

    template <size_t N>
    using static_bitmap = basic_bitmap<static_bitmap_storage<N>>;
}  // namespace tay
