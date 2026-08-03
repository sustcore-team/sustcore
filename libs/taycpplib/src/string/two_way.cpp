/**
 * @file two_way.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Two-Way 字符串匹配算法实现
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <limits.h>
#include <tay/attribute.h>
#include <tay/detail/string/two_way.h>

namespace tay::detail {
    namespace {
        using size_type = size_t;

        static_assert(CHAR_BIT == 8);

        constexpr size_type npos             = size_type(-1);
        constexpr size_type byte_value_count = 1U << CHAR_BIT;
        constexpr size_type bits_per_word    = sizeof(size_type) * CHAR_BIT;
        constexpr size_type byteset_words = (byte_value_count + bits_per_word - 1) / bits_per_word;

        struct factorization {
            size_type suffix;
            size_type period;
        };

        template <__search_direction direction>
        [[nodiscard]]
        static ALWAYS_INLINE unsigned char character_at(const char* string, size_type length,
                                                        size_type position) noexcept {
            if constexpr (direction == __search_direction::FORWARD) {
                return static_cast<unsigned char>(string[position]);
            } else {
                return static_cast<unsigned char>(string[length - position - 1]);
            }
        }

        [[nodiscard]]
        static ALWAYS_INLINE bool byte_is_present(const size_type* byteset,
                                                  unsigned char byte) noexcept {
            return (byteset[byte / bits_per_word] & (size_type(1) << (byte % bits_per_word))) != 0;
        }

        static ALWAYS_INLINE void mark_byte(size_type* byteset, unsigned char byte) noexcept {
            byteset[byte / bits_per_word] |= size_type(1) << (byte % bits_per_word);
        }

        template <__search_direction direction>
        [[nodiscard]]
        factorization maximal_suffix(const char* pattern, size_type pattern_length,
                                     bool reverse_order) noexcept {
            size_type suffix    = npos;
            size_type candidate = 0;
            size_type offset    = 1;
            size_type period    = 1;

            while (candidate + offset < pattern_length) {
                const unsigned char candidate_char =
                    character_at<direction>(pattern, pattern_length, candidate + offset);
                const unsigned char suffix_char =
                    character_at<direction>(pattern, pattern_length, suffix + offset);

                if (candidate_char == suffix_char) {
                    if (offset == period) {
                        candidate += period;
                        offset     = 1;
                    } else {
                        ++offset;
                    }
                } else if ((candidate_char < suffix_char) != reverse_order) {
                    candidate += offset;
                    offset     = 1;
                    period     = candidate - suffix;
                } else {
                    suffix    = candidate;
                    candidate = suffix + 1;
                    offset    = 1;
                    period    = 1;
                }
            }

            return {suffix, period};
        }

        template <__search_direction direction>
        [[nodiscard]]
        size_type two_way_search(const char* text, size_type text_length, const char* pattern,
                                 size_type pattern_length) noexcept {
            size_type byteset[byteset_words]{};
            size_type shift_table[byte_value_count];
            for (size_type index = 0; index < pattern_length; ++index) {
                const unsigned char byte = character_at<direction>(pattern, pattern_length, index);
                mark_byte(byteset, byte);
                shift_table[byte] = index + 1;
            }

            const factorization normal = maximal_suffix<direction>(pattern, pattern_length, false);
            const factorization reversed = maximal_suffix<direction>(pattern, pattern_length, true);
            const factorization selected =
                normal.suffix + 1 > reversed.suffix + 1 ? normal : reversed;

            const size_type critical_position = selected.suffix + 1;
            size_type period                  = selected.period;
            size_type memory_after_shift      = 0;

            bool periodic = true;
            // 循环不变量：critical_position + period <= pattern_length。
            // 因此 index < critical_position，且
            // index + period < pattern_length，两次访问都不会越界。
            for (size_type index = 0; index < critical_position; ++index) {
                if (character_at<direction>(pattern, pattern_length, index) !=
                    character_at<direction>(pattern, pattern_length, index + period))
                {
                    periodic = false;
                    break;
                }
            }

            if (periodic) {
                memory_after_shift = pattern_length - period;
            } else {
                const size_type right_length = pattern_length - critical_position;
                period = critical_position > right_length ? critical_position : right_length + 1;
            }

            const size_type last_pattern_index = pattern_length - 1;
            size_type position                 = 0;
            size_type memory                   = 0;
            while (position <= text_length - pattern_length) {
                const unsigned char last_byte =
                    character_at<direction>(text, text_length, position + last_pattern_index);
                if (!byte_is_present(byteset, last_byte)) {
                    position += pattern_length;
                    memory    = 0;
                    continue;
                }

                // 只有 byte_is_present 返回 true 才会到达这里；而字节被
                // 标记进 byteset 时，shift_table 中对应的项也已经赋值，
                // 因此此处读取的 shift_table[last_byte] 必定已被赋值过。
                size_type bad_character_shift = pattern_length - shift_table[last_byte];
                if (bad_character_shift != 0) {
                    if (bad_character_shift < memory) {
                        bad_character_shift = memory;
                    }
                    position += bad_character_shift;
                    memory    = 0;
                    continue;
                }

                size_type index = critical_position > memory ? critical_position : memory;
                while (index < last_pattern_index &&
                       character_at<direction>(pattern, pattern_length, index) ==
                           character_at<direction>(text, text_length, position + index))
                {
                    ++index;
                }

                if (index < last_pattern_index) {
                    position += index - critical_position + 1;
                    memory    = 0;
                    continue;
                }

                index = critical_position;
                while (index > memory &&
                       character_at<direction>(pattern, pattern_length, index - 1) ==
                           character_at<direction>(text, text_length, position + index - 1))
                {
                    --index;
                }
                if (index <= memory) {
                    return position;
                }

                position += period;
                memory    = memory_after_shift;
            }

            return npos;
        }
    }  // namespace

    template <__search_direction direction>
    size_t __str_two_way(const char* text, size_t text_length, const char* pattern,
                         size_t pattern_length) noexcept {
        if (pattern_length == 0) {
            if constexpr (direction == __search_direction::FORWARD) {
                return 0;
            } else {
                return text_length;
            }
        }
        if (pattern_length > text_length) {
            return npos;
        }

        const size_type position =
            two_way_search<direction>(text, text_length, pattern, pattern_length);
        if constexpr (direction == __search_direction::FORWARD) {
            return position;
        } else {
            return position == npos ? npos : text_length - pattern_length - position;
        }
    }

    template size_t __str_two_way<__search_direction::FORWARD>(const char*, size_t, const char*,
                                                               size_t) noexcept;
    template size_t __str_two_way<__search_direction::BACKWARD>(const char*, size_t, const char*,
                                                                size_t) noexcept;
}  // namespace tay::detail
