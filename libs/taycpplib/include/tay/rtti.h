/**
 * @file rtti.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供 Tay C++ 库的运行时类型信息设施。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace tay {
    template <typename T, typename _Base, typename _TypeId>
    concept derived_class_trait =
        std::is_enum_v<_TypeId> && std::is_base_of_v<_Base, T> && requires(_Base *base) {
            {
                base->type_id()
            } -> std::convertible_to<_TypeId>;
            {
                T::IDENTIFIER
            } -> std::convertible_to<_TypeId>;
        };

    template <typename _Base, typename _TypeId>
        requires std::is_enum_v<_TypeId>
    class rtti_base {
    protected:
        virtual _TypeId type_id() const = 0;

    public:
        using rtti_base_type = _Base;
        using rtti_type_id   = _TypeId;

        template <typename T>
            requires derived_class_trait<T, _Base, _TypeId>
        bool is() const {
            return type_id() == T::IDENTIFIER;
        }

        template <typename T>
            requires derived_class_trait<T, _Base, _TypeId>
        static T *cast(_Base *base) {
            if (base->template is<T>()) {
                return static_cast<T *>(base);
            }
            return nullptr;
        }

        template <typename T>
            requires derived_class_trait<T, _Base, _TypeId>
        static const T *cast(const _Base *base) {
            if (base->template is<T>()) {
                return static_cast<const T *>(base);
            }
            return nullptr;
        }

        template <typename T>
            requires derived_class_trait<T, _Base, _TypeId>
        T *as() {
            if (is<T>()) {
                return static_cast<T *>(this);
            }
            return nullptr;
        }

        template <typename T>
            requires derived_class_trait<T, _Base, _TypeId>
        const T *as() const {
            if (is<T>()) {
                return static_cast<const T *>(this);
            }
            return nullptr;
        }
    };

    template <typename T, typename U>
    U *dyn_cast(T *ptr) {
        return T::rtti_base_type::template cast<U>(ptr);
    }

    template <typename T, typename U>
    const U *dyn_cast(const T *ptr) {
        return T::rtti_base_type::template cast<U>(ptr);
    }
}  // namespace tay
