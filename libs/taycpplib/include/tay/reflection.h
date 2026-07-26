/**
 * @file reflection.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ 静态反射
 * @version 0.1.0-dev.1
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concepts>
#include <meta>
#include <string_view>

namespace util {
    /**
     * @brief 自动序列化函数
     *
     * @tparam E 枚举类型
     * @param value 枚举值
     * @return 枚举值名
     */
    template <typename E>
        requires std::is_enum_v<E>
    constexpr std::string_view enum_to_string(E value) {
        // 'template for' 在编译期展开循环
        template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
            // [:e:] 语法将编译期反射对象 e 还原为对应的运行时值
            constexpr auto name = std::meta::identifier_of(e);
            if (value == [:e:]) {
                return name;
            }
        }
        return "<unnamed>";
    }

    template <typename T, typename AnnotationType>
    consteval bool __is_annotated_by() {
        return !std::meta::annotations_of_with_type(^^T, ^^AnnotationType).empty();
    }

    template <typename T, typename AnnotationType>
    concept is_annotated_by = __is_annotated_by<T, AnnotationType>();

    template <typename T, typename AnnotationType>
    consteval AnnotationType get_annotation() {
        auto annos = std::meta::annotations_of_with_type(^^T, ^^AnnotationType);
        return std::meta::extract<AnnotationType>(annos[0]);
    }

    template <std::meta::info Entity, typename AnnotationType>
    consteval bool is_annotated_entity_by() {
        return !std::meta::annotations_of_with_type(Entity, ^^AnnotationType).empty();
    }

    template <std::meta::info Entity, typename AnnotationType>
    consteval AnnotationType get_entity_annotation() {
        auto annos = std::meta::annotations_of_with_type(Entity, ^^AnnotationType);
        return std::meta::extract<AnnotationType>(annos[0]);
    }
}  // namespace util
