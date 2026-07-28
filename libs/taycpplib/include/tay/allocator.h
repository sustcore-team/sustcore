/**
 * @file allocator.h
 * @brief Exception-free allocator adaptation and stateless host allocation.
 */

#pragma once

#include <tay/err.h>
#include <tay/expected.h>
#include <tay/panic.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace tay {
    template <class Pointer, class SizeType = std::size_t>
    struct allocation_result {
        Pointer ptr;
        SizeType count;
    };

    namespace detail {
        [[noreturn]] inline void panic_allocation(error_code error) noexcept {
            switch (error) {
                case error_code::ALLOCATION_SIZE_OVERFLOW:
                    tay::panic("allocation size overflow");
                case error_code::OUT_OF_MEMORY: tay::panic("out of memory");
                default:                        tay::panic("allocator failure");
            }
        }
    }  // namespace detail

    template <class Allocator>
    struct allocator_traits {
    private:
        using standard_traits = std::allocator_traits<Allocator>;

    public:
        using allocator_type     = Allocator;
        using value_type         = typename standard_traits::value_type;
        using pointer            = typename standard_traits::pointer;
        using const_pointer      = typename standard_traits::const_pointer;
        using void_pointer       = typename standard_traits::void_pointer;
        using const_void_pointer = typename standard_traits::const_void_pointer;
        using difference_type    = typename standard_traits::difference_type;
        using size_type          = typename standard_traits::size_type;

        using propagate_on_container_copy_assignment =
            typename standard_traits::propagate_on_container_copy_assignment;
        using propagate_on_container_move_assignment =
            typename standard_traits::propagate_on_container_move_assignment;
        using propagate_on_container_swap =
            typename standard_traits::propagate_on_container_swap;
        using is_always_equal = typename standard_traits::is_always_equal;

        static_assert(std::is_same_v<pointer, value_type*>,
                      "tay allocators currently require raw pointers");

        template <class U>
        using rebind_alloc = typename standard_traits::template rebind_alloc<U>;

        template <class U>
        using rebind_traits = allocator_traits<rebind_alloc<U>>;

        [[nodiscard]]
        static constexpr expected<pointer, error_code> try_allocate(
            allocator_type& allocator, size_type count) noexcept {
            if (count == 0) {
                return pointer{};
            }
            if (count > max_size(allocator)) {
                return expected<pointer, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            static_assert(noexcept(allocator.try_allocate(count)),
                          "try_allocate must be noexcept");
            return allocator.try_allocate(count);
        }

        [[nodiscard]]
        static constexpr pointer allocate(allocator_type& allocator,
                                          size_type count) noexcept {
            auto result = try_allocate(allocator, count);
            if (!result) {
                detail::panic_allocation(result.error());
            }
            return *result;
        }

        [[nodiscard]]
        static constexpr expected<allocation_result<pointer, size_type>,
                                  error_code>
        try_allocate_at_least(allocator_type& allocator,
                              size_type count) noexcept {
            if (count == 0) {
                return allocation_result<pointer, size_type>{pointer{}, 0};
            }
            if (count > max_size(allocator)) {
                return expected<allocation_result<pointer, size_type>,
                                error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }
            if constexpr (requires { allocator.try_allocate_at_least(count); })
            {
                static_assert(noexcept(allocator.try_allocate_at_least(count)),
                              "try_allocate_at_least must be noexcept");
                return allocator.try_allocate_at_least(count);
            } else {
                auto result = try_allocate(allocator, count);
                if (!result) {
                    return expected<allocation_result<pointer, size_type>,
                                    error_code>(unexpect, result.error());
                }
                return allocation_result<pointer, size_type>{*result, count};
            }
        }

        [[nodiscard]]
        static constexpr allocation_result<pointer, size_type>
        allocate_at_least(allocator_type& allocator, size_type count) noexcept {
            auto result = try_allocate_at_least(allocator, count);
            if (!result) {
                detail::panic_allocation(result.error());
            }
            return *result;
        }

        static constexpr void deallocate(allocator_type& allocator,
                                         pointer memory,
                                         size_type count) noexcept {
            if (memory == nullptr) {
                return;
            }
            static_assert(noexcept(allocator.deallocate(memory, count)),
                          "deallocate must be noexcept");
            allocator.deallocate(memory, count);
        }

        template <class T, class... Args>
        static constexpr void construct(
            allocator_type& allocator, T* location,
            Args&&... args) noexcept(noexcept(standard_traits::
                                                  construct(allocator, location,
                                                            std::forward<Args>(
                                                                args)...))) {
            standard_traits::construct(allocator, location,
                                       std::forward<Args>(args)...);
        }

        template <class T>
        static constexpr void
        destroy(allocator_type& allocator, T* location) noexcept(
            noexcept(standard_traits::destroy(allocator, location))) {
            standard_traits::destroy(allocator, location);
        }

        [[nodiscard]]
        static constexpr size_type max_size(
            const allocator_type& allocator) noexcept {
            return standard_traits::max_size(allocator);
        }

        [[nodiscard]]
        static constexpr allocator_type select_on_container_copy_construction(
            const allocator_type&
                allocator) noexcept(noexcept(standard_traits::
                                                 select_on_container_copy_construction(
                                                     allocator))) {
            return standard_traits::select_on_container_copy_construction(
                allocator);
        }
    };

#if defined(TAY_ENV_HOST)
    template <class T>
    class allocator {
    public:
        using value_type                             = T;
        using pointer                                = T*;
        using const_pointer                          = const T*;
        using size_type                              = std::size_t;
        using difference_type                        = std::ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal                        = std::true_type;

        constexpr allocator() noexcept = default;

        template <class U>
        constexpr allocator(const allocator<U>&) noexcept {}

        template <class U>
        struct rebind {
            using other = allocator<U>;
        };

        [[nodiscard]]
        expected<pointer, error_code> try_allocate(size_type count) noexcept {
            if (count == 0) {
                return pointer{};
            }
            if (count > max_size()) {
                return expected<pointer, error_code>(
                    unexpect, error_code::ALLOCATION_SIZE_OVERFLOW);
            }

            void* memory          = nullptr;
            const size_type bytes = count * sizeof(value_type);
            if constexpr (alignof(value_type) >
                          __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                memory = ::operator new(
                    bytes, std::align_val_t(alignof(value_type)), std::nothrow);
            } else {
                memory = ::operator new(bytes, std::nothrow);
            }
            if (memory == nullptr) {
                return expected<pointer, error_code>(unexpect,
                                                     error_code::OUT_OF_MEMORY);
            }
            return static_cast<pointer>(memory);
        }

        [[nodiscard]]
        pointer allocate(size_type count) noexcept {
            auto result = try_allocate(count);
            if (!result) {
                detail::panic_allocation(result.error());
            }
            return *result;
        }

        void deallocate(pointer memory,
                        size_type count [[maybe_unused]]) noexcept {
            if (memory == nullptr) {
                return;
            }
            if constexpr (alignof(value_type) >
                          __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(memory,
                                  std::align_val_t(alignof(value_type)));
            } else {
                ::operator delete(memory);
            }
        }

        [[nodiscard]]
        constexpr size_type max_size() const noexcept {
            return size_type(-1) / sizeof(value_type);
        }
    };

    template <class T, class U>
    constexpr bool operator==(const allocator<T>&,
                              const allocator<U>&) noexcept {
        return true;
    }

    template <class T, class U>
    constexpr bool operator!=(const allocator<T>&,
                              const allocator<U>&) noexcept {
        return false;
    }
#endif
}  // namespace tay
