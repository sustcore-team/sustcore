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

#include <string.h>
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
                D_data = nullptr;
                D_length = 0;
            }
        }
    public:
        string(const char *str);
        string();
        string(const char *begin, const char *end);
        ~string();

        [[nodiscard]]
        constexpr const char* c_str() const noexcept {
            return D_data;
        }
        [[nodiscard]]
        constexpr size_t size() const noexcept {
            return D_length;
        }
        [[nodiscard]]
        constexpr size_t length() const noexcept {
            return D_length;
        }
        [[nodiscard]]
        constexpr size_t capacity() const noexcept {
            return D_length;
        }

        string(const string& str);
        string &operator=(const string& str);
        string(string&& str);
        string &operator=(string&& str);

        [[nodiscard]]
        bool operator==(const string& str) const;
        [[nodiscard]]
        constexpr bool operator!=(const string& str) const {
            return !(*this == str);
        }

        [[nodiscard]]
        char operator[](size_t index) const {
            return D_data[index];
        }
    };

    // 字符串构建器
    class string_builder {
    private:
        size_t D_bufsz;
        size_t D_length;
        char *D_buf;
    public:
        string_builder(size_t bufsz = 32);
        string_builder(const string_builder&) = delete;
        string_builder& operator=(const string_builder&) = delete;
        string_builder(const char *str);
        string_builder(string str);

        ~string_builder();

        [[nodiscard]]
        inline string build() const {
            return {D_buf};
        }

        [[nodiscard]]
        size_t size() const noexcept {
            return D_length;
        }
        [[nodiscard]]
        size_t length() const noexcept {
            return D_length;
        }
        [[nodiscard]]
        size_t capacity() const noexcept {
            return D_bufsz;
        }

        void append(const char *str);
        void append(string str);
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
}