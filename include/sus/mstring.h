/**
 * @file mstring.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 字符串
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstring>

namespace util {
    // 字符串类
    class string {
    private:
        size_t D_length;
        char *D_data;
        void clear_data() {
            if (D_data != nullptr) {
                delete[] D_data;
                D_data   = nullptr;
                D_length = 0;
            }
        }

    public:
        string();
        string(const char *str);
        string(const char *str, size_t length);
        string(const char *begin, const char *end);
        ~string();

        constexpr char *c_str() const noexcept {
            return D_data;
        }
        constexpr size_t size() const noexcept {
            return D_length;
        }
        constexpr size_t length() const noexcept {
            return D_length;
        }
        constexpr size_t capacity() const noexcept {
            return D_length;
        }

        string substr(size_t pos, size_t count) const;
        string substr(size_t pos) const;

        string(const string &str);
        string &operator=(const string &str);
        string(string &&str);
        string &operator=(string &&str);

        bool operator==(const string &str) const;
        constexpr bool operator!=(const string &str) const {
            return !(*this == str);
        }

        char operator[](size_t index) const {
            return D_data[index];
        }
    };

    // 字符串构建器，只允许局部使用
    class string_builder {
    private:
        size_t D_bufsz;
        size_t D_length;
        char *D_buf;

        void ensure_bufsz(size_t target_bufsz);

    public:
        string_builder(size_t bufsz = 32);
        string_builder(const string_builder &)            = delete;
        string_builder &operator=(const string_builder &) = delete;
        string_builder(string_builder &&)                 = delete;
        string_builder &operator=(string_builder &&)      = delete;
        string_builder(const char *str);
        string_builder(const string &str);

        ~string_builder();

        inline string build() const {
            return string(D_buf);
        }

        size_t size() const noexcept {
            return D_length;
        }
        size_t length() const noexcept {
            return D_length;
        }
        size_t capacity() const noexcept {
            return D_bufsz;
        }

        void reserve(size_t new_bufsz);

        void append(const char *str);
        void append(const char *str, size_t str_len);
        void append(const string &str);
        void append(char ch);

        inline void revert(size_t count) {
            if (count > D_length) {
                D_length = 0;
            } else {
                D_length -= count;
            }
            D_buf[D_length] = '\0';
        }

        char operator[](size_t index) const {
            return D_buf[index];
        }
    };
}  // namespace util