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

#include <sus/tostring.h>

#include <cstddef>
#include <cstring>

namespace util {
    // 字符串类
    class [[deprecated("Use std::string")]] string {
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

        [[nodiscard]]
        constexpr const char *c_str() const noexcept {
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

        [[nodiscard]]
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

    inline string to_mstring(const char *str) {
        return {str};
    }
    inline string to_mstring(const string &str) {
        return str;
    }

    inline string to_mstring(int val) {
        char _buf[50];
        itoa(val, (char *)_buf, 10);
        return {(char *)_buf};
    }

    inline string to_mstring(unsigned int val) {
        char _buf[50];
        uitoa(val, (char *)_buf, 10);
        return {(char *)_buf};
    }

    inline string to_mstring(long long val) {
        char _buf[50];
        lltoa(val, (char *)_buf, 10);
        return {(char *)_buf};
    }

    inline string to_mstring(unsigned long long val) {
        char _buf[60];
        ulltoa(val, (char *)_buf, 10);
        return {(char *)_buf};
    }

    inline string to_mstring(short val) {
        return to_mstring((int)val);
    }

    inline string to_mstring(unsigned short val) {
        return to_mstring((unsigned int)val);
    }

    inline string to_mstring(long val) {
        return to_mstring((long long)val);
    }

    inline string to_mstring(unsigned long val) {
        return to_mstring((unsigned long long)val);
    }

    inline string to_mstring(char val) {
        char _buf[2] = {val, '\0'};
        return {(char *)_buf};
    }

    inline string to_mstring(unsigned char val) {
        return to_mstring((int)val);
    }

    inline string to_mstring(bool val) {
        return val ? "true" : "false";
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast,
    // cppcoreguidelines-pro-bounds-array-to-pointer-decay,
    // cppcoreguidelines-pro-bounds-constant-array-index)
    inline string to_mstring(const void *ptr) {
        char _buf[20];
        ulltoa((unsigned long long)ptr, _buf, 16);
        char _padbuf[20];
        size_t len = strlen(_buf);
        // padding to 16 characters
        for (size_t i = 0; i < 16 - len; i++) {
            _padbuf[i] = '0';
        }
        for (size_t i = 0; i < len; i++) {
            _padbuf[16 - len + i] = _buf[i];
        }
        _padbuf[16] = '\0';
        return {(char *)_padbuf};
    }
    // NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast,
    // cppcoreguidelines-pro-bounds-array-to-pointer-decay,
    // cppcoreguidelines-pro-bounds-constant-array-index)

}  // namespace util