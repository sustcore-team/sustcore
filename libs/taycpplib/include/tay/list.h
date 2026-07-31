/**
 * @file list.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief intrusive linked list
 * @version 0.1.0-dev.1
 * @date 2026-07-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/intrusive.h>
#include <tay/panic.h>
#include <tay/utility.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    template <typename OwnerPointer, typename BorrowPointer>
    struct intrusive_list_hook {
        using owner_pointer  = OwnerPointer;
        using borrow_pointer = BorrowPointer;

        constexpr intrusive_list_hook() noexcept
            : next(nullptr), previous(nullptr), in_list(false) {}

        owner_pointer next;
        borrow_pointer previous;
        bool in_list;
    };

    namespace detail {
        struct intrusive_list_locate_tag {};

        constexpr void intrusive_list_require(bool condition,
                                              const char *message) noexcept {
            if (!condition) {
                tay::panic(message);
            }
        }
    }  // namespace detail

    template <typename T, typename Locate>
    class intrusive_list
        : private composition<detail::intrusive_list_locate_tag, Locate> {
    private:
        using locate_tag = detail::intrusive_list_locate_tag;
        using hook =
            std::remove_reference_t<std::invoke_result_t<Locate &, T &>>;

    public:
        using value_type     = T;
        using locate_type    = Locate;
        using owner_pointer  = typename hook::owner_pointer;
        using borrow_pointer = typename hook::borrow_pointer;
        using traits = intrusive_traits<T, owner_pointer, borrow_pointer>;
        using const_borrow_pointer = typename traits::const_borrow_pointer;

    private:
        constexpr hook &hook_of(borrow_pointer pointer) noexcept {
            return get<locate_tag>(this)(*pointer);
        }

        constexpr const hook &hook_of(const_borrow_pointer pointer) const
            noexcept {
            return get<locate_tag>(this)(*pointer);
        }

        static constexpr void require(bool condition,
                                      const char *message) noexcept {
            detail::intrusive_list_require(condition, message);
        }

    public:
        class iterator {
            friend class intrusive_list;
            friend class const_iterator;

        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using iterator_concept  = std::bidirectional_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = borrow_pointer;
            using reference         = borrow_pointer;
            using pointer           = borrow_pointer;

        private:
            constexpr hook &hook_of(borrow_pointer current) noexcept {
                return owner_->hook_of(current);
            }

        public:
            constexpr iterator() noexcept = default;

            explicit constexpr iterator(intrusive_list *owner,
                                        borrow_pointer current) noexcept
                : owner_(owner), current_(current) {}

            [[nodiscard]] constexpr borrow_pointer operator*() const noexcept {
                return current_;
            }

            [[nodiscard]] constexpr borrow_pointer operator->() const noexcept {
                return current_;
            }

            [[nodiscard]] friend constexpr bool operator==(
                const iterator &left, const iterator &right) noexcept {
                return left.owner_ == right.owner_ &&
                       left.current_ == right.current_;
            }

            [[nodiscard]] friend constexpr bool operator!=(
                const iterator &left, const iterator &right) noexcept {
                return !(left == right);
            }

            constexpr iterator &operator++() noexcept {
                require(current_ != nullptr,
                        "intrusive_list cannot increment end iterator");
                current_ = traits::decay(hook_of(current_).next);
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                iterator copy = *this;
                ++*this;
                return copy;
            }

            constexpr iterator &operator--() noexcept {
                require(owner_ != nullptr,
                        "intrusive_list iterator has no owner");
                if (current_ == nullptr) {
                    current_ = owner_->back_;
                } else {
                    current_ = hook_of(current_).previous;
                }
                require(current_ != nullptr,
                        "intrusive_list cannot decrement begin iterator");
                return *this;
            }

            constexpr iterator operator--(int) noexcept {
                iterator copy = *this;
                --*this;
                return copy;
            }

        private:
            intrusive_list *owner_ = nullptr;
            borrow_pointer current_ = nullptr;
        };

        class reverse_iterator {
            friend class intrusive_list;
            intrusive_list *owner_ = nullptr;
            borrow_pointer current_ = nullptr;
            constexpr reverse_iterator(intrusive_list *owner,
                                       borrow_pointer current) noexcept
                : owner_(owner), current_(current) {}

        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using iterator_concept = std::bidirectional_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = borrow_pointer;
            using reference = borrow_pointer;
            using pointer = borrow_pointer;
            constexpr reverse_iterator() noexcept = default;
            [[nodiscard]] constexpr borrow_pointer operator*() const noexcept {
                return current_;
            }
            [[nodiscard]] constexpr borrow_pointer operator->() const noexcept {
                return current_;
            }
            constexpr reverse_iterator& operator++() noexcept {
                require(current_ != nullptr,
                        "intrusive_list cannot increment rend iterator");
                current_ = owner_->hook_of(current_).previous;
                return *this;
            }
            constexpr reverse_iterator operator++(int) noexcept {
                auto copy = *this;
                ++*this;
                return copy;
            }
            friend constexpr bool operator==(const reverse_iterator& left,
                                             const reverse_iterator& right)
                noexcept {
                return left.owner_ == right.owner_ &&
                       left.current_ == right.current_;
            }
        };

        class const_iterator {
            friend class intrusive_list;
            const intrusive_list *owner_ = nullptr;
            const_borrow_pointer current_ = nullptr;
            constexpr const_iterator(const intrusive_list *owner,
                                     const_borrow_pointer current) noexcept
                : owner_(owner), current_(current) {}

        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using iterator_concept = std::bidirectional_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = const_borrow_pointer;
            using reference = const_borrow_pointer;
            using pointer = const_borrow_pointer;
            constexpr const_iterator() noexcept = default;
            constexpr const_iterator(const iterator& other) noexcept
                : owner_(other.owner_), current_(other.current_) {}
            [[nodiscard]] constexpr reference operator*() const noexcept {
                return current_;
            }
            [[nodiscard]] constexpr pointer operator->() const noexcept {
                return current_;
            }
            constexpr const_iterator& operator++() noexcept {
                require(current_ != nullptr,
                        "intrusive_list cannot increment end iterator");
                current_ = traits::decay(owner_->hook_of(current_).next);
                return *this;
            }
            constexpr const_iterator operator++(int) noexcept {
                auto copy = *this;
                ++*this;
                return copy;
            }
            constexpr const_iterator& operator--() noexcept {
                if (current_ == nullptr) {
                    current_ = owner_->back_;
                } else {
                    current_ = owner_->hook_of(current_).previous;
                }
                require(current_ != nullptr,
                        "intrusive_list cannot decrement begin iterator");
                return *this;
            }
            friend constexpr bool operator==(const const_iterator& left,
                                             const const_iterator& right)
                noexcept {
                return left.owner_ == right.owner_ &&
                       left.current_ == right.current_;
            }
        };

        constexpr intrusive_list() noexcept
            requires std::is_nothrow_default_constructible_v<Locate>
            : front_(nullptr), back_(nullptr) {}

        constexpr explicit intrusive_list(Locate locate) noexcept
            : composition<detail::intrusive_list_locate_tag, Locate>(
                  std::move(locate)),
              front_(nullptr), back_(nullptr) {}

        intrusive_list(const intrusive_list &)            = delete;
        intrusive_list &operator=(const intrusive_list &) = delete;
        intrusive_list(intrusive_list &&)                 = delete;
        intrusive_list &operator=(intrusive_list &&)      = delete;

        [[nodiscard]] constexpr iterator iterator_to(
            borrow_pointer pointer) noexcept {
            require(pointer != nullptr,
                    "intrusive_list iterator_to received a null pointer");
            require(hook_of(pointer).in_list,
                    "intrusive_list element is not linked");
            return iterator(this, pointer);
        }

        constexpr iterator push_front(owner_pointer element) noexcept {
            require(static_cast<bool>(element),
                    "intrusive_list cannot insert a null pointer");
            borrow_pointer borrow = traits::decay(element);
            require(!hook_of(borrow).in_list,
                    "intrusive_list element is already linked");
            require(!static_cast<bool>(hook_of(borrow).next),
                    "intrusive_list element has a stale next pointer");
            require(!static_cast<bool>(hook_of(borrow).previous),
                    "intrusive_list element has a stale previous pointer");

            if (!static_cast<bool>(front_)) {
                back_ = borrow;
            } else {
                hook_of(borrow).next     = std::move(front_);
                hook_of(front_).previous = borrow;
            }
            front_                  = std::move(element);
            hook_of(borrow).in_list = true;
            ++size_;
            return iterator(this, borrow);
        }

        constexpr iterator push_back(owner_pointer element) noexcept {
            require(static_cast<bool>(element),
                    "intrusive_list cannot insert a null pointer");
            borrow_pointer borrow = traits::decay(element);
            require(!hook_of(borrow).in_list,
                    "intrusive_list element is already linked");
            require(!static_cast<bool>(hook_of(borrow).next),
                    "intrusive_list element has a stale next pointer");
            require(!static_cast<bool>(hook_of(borrow).previous),
                    "intrusive_list element has a stale previous pointer");

            if (!static_cast<bool>(back_)) {
                front_ = std::move(element);
            } else {
                hook_of(borrow).previous = back_;
                hook_of(back_).next      = std::move(element);
            }
            back_                   = borrow;
            hook_of(borrow).in_list = true;
            ++size_;
            return iterator(this, borrow);
        }

        constexpr iterator insert(iterator before,
                                  owner_pointer element) noexcept {
            if (before.current_ == nullptr) {
                return push_back(std::move(element));
            }
            if (before.current_ == traits::decay(front_)) {
                return push_front(std::move(element));
            }

            require(static_cast<bool>(element),
                    "intrusive_list cannot insert a null pointer");
            require(hook_of(before.current_).in_list,
                    "intrusive_list insertion iterator is not linked");

            borrow_pointer borrow = traits::decay(element);
            require(!hook_of(borrow).in_list,
                    "intrusive_list element is already linked");
            require(!static_cast<bool>(hook_of(borrow).next),
                    "intrusive_list element has a stale next pointer");
            require(!static_cast<bool>(hook_of(borrow).previous),
                    "intrusive_list element has a stale previous pointer");

            borrow_pointer previous = hook_of(before.current_).previous;
            owner_pointer next      = std::move(hook_of(previous).next);
            hook_of(previous).next  = std::move(element);
            hook_of(traits::decay(next)).previous = borrow;
            hook_of(borrow).previous              = previous;
            hook_of(borrow).next                  = std::move(next);
            hook_of(borrow).in_list               = true;
            ++size_;
            return iterator(this, borrow);
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return !static_cast<bool>(front_);
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr bool linked(borrow_pointer pointer) noexcept {
            return pointer != nullptr && hook_of(pointer).in_list;
        }

        [[nodiscard]] constexpr borrow_pointer front() noexcept {
            return traits::decay(front_);
        }

        [[nodiscard]] constexpr const_borrow_pointer front() const noexcept {
            return traits::decay(front_);
        }

        [[nodiscard]] constexpr borrow_pointer back() noexcept {
            return back_;
        }

        [[nodiscard]] constexpr const_borrow_pointer back() const noexcept {
            return back_;
        }

        constexpr owner_pointer pop_front() noexcept {
            require(static_cast<bool>(front_),
                    "intrusive_list cannot pop from an empty list");
            return erase(iterator(this, traits::decay(front_)));
        }

        constexpr owner_pointer pop_back() noexcept {
            require(static_cast<bool>(back_),
                    "intrusive_list cannot pop from an empty list");
            return erase(iterator(this, back_));
        }

        constexpr owner_pointer erase(iterator position) noexcept {
            require(position.current_ != nullptr,
                    "intrusive_list cannot erase end iterator");
            require(hook_of(position.current_).in_list,
                    "intrusive_list element is not linked");

            owner_pointer next = std::move(hook_of(position.current_).next);
            borrow_pointer previous = hook_of(position.current_).previous;

            if (!static_cast<bool>(next)) {
                require(back_ == position.current_,
                        "intrusive_list has an inconsistent back pointer");
                back_ = previous;
            } else {
                borrow_pointer next_borrow = traits::decay(next);
                require(hook_of(next_borrow).previous == position.current_,
                        "intrusive_list has inconsistent links");
                hook_of(next_borrow).previous = previous;
            }

            owner_pointer erased;
            if (!static_cast<bool>(previous)) {
                require(traits::decay(front_) == position.current_,
                        "intrusive_list has an inconsistent front pointer");
                erased = std::move(front_);
                front_ = std::move(next);
            } else {
                require(
                    traits::decay(hook_of(previous).next) == position.current_,
                    "intrusive_list has inconsistent links");
                erased                 = std::move(hook_of(previous).next);
                hook_of(previous).next = std::move(next);
            }

            require(traits::decay(erased) == position.current_,
                    "intrusive_list removed an unexpected element");
            hook_of(position.current_).next     = nullptr;
            hook_of(position.current_).previous = nullptr;
            hook_of(position.current_).in_list  = false;
            --size_;
            return erased;
        }

        constexpr owner_pointer remove(borrow_pointer pointer) noexcept {
            return erase(iterator_to(pointer));
        }

        constexpr iterator erase(iterator first, iterator last) noexcept {
            while (first != last) {
                iterator next = first;
                ++next;
                static_cast<void>(erase(first));
                first = next;
            }
            return last;
        }

        constexpr void clear() noexcept {
            while (!empty()) {
                static_cast<void>(pop_front());
            }
        }

        constexpr void splice(iterator position,
                              intrusive_list &other) noexcept {
            if (other.empty()) {
                return;
            }
            if (this == &other) {
                return;
            }
            require(position.owner_ == this,
                    "intrusive_list splice iterator belongs to another list");

            borrow_pointer other_front = traits::decay(other.front_);
            require(hook_of(other_front).in_list,
                    "intrusive_list splice source is inconsistent");
            require(!static_cast<bool>(hook_of(other_front).previous),
                    "intrusive_list splice source has a previous element");

            if (empty()) {
                front_ = std::move(other.front_);
                back_ = other.back_;
            } else if (position.current_ == nullptr) {
                hook_of(other_front).previous = back_;
                hook_of(back_).next = std::move(other.front_);
                back_ = other.back_;
            } else if (position.current_ == traits::decay(front_)) {
                owner_pointer displaced = std::move(front_);
                front_ = std::move(other.front_);
                hook_of(other.back_).next = std::move(displaced);
                hook_of(position.current_).previous = other.back_;
            } else {
                borrow_pointer previous = hook_of(position.current_).previous;
                owner_pointer displaced = std::move(hook_of(previous).next);
                hook_of(previous).next = std::move(other.front_);
                hook_of(other_front).previous = previous;
                hook_of(other.back_).next = std::move(displaced);
                hook_of(position.current_).previous = other.back_;
            }
            size_ += other.size_;
            other.front_ = nullptr;
            other.back_  = nullptr;
            other.size_ = 0;
        }

        constexpr void splice(iterator position, intrusive_list& other,
                              iterator element) noexcept {
            iterator next = element;
            ++next;
            if (this == &other &&
                (position == element || position == next)) {
                return;
            }
            splice(position, other, element, next);
        }

        constexpr void splice(iterator position, intrusive_list& other,
                              iterator first, iterator last) noexcept {
            if (this == &other) {
                for (iterator current = first; current != last; ++current) {
                    if (current == position) {
                        return;
                    }
                }
            }
            while (first != last) {
                iterator next = first;
                ++next;
                owner_pointer element = other.erase(first);
                insert(position, std::move(element));
                first = next;
            }
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            return iterator(this, traits::decay(front_));
        }

        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(this, nullptr);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_iterator(this, traits::decay(front_));
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(this, nullptr);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return begin();
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return end();
        }

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(this, back_);
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(this, nullptr);
        }

    private:
        owner_pointer front_;
        borrow_pointer back_;
        std::size_t size_ = 0;
    };
}  // namespace tay
