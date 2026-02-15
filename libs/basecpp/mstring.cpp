/**
 * @file mstring.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/mstring.h>

namespace util {
    string::string(const char *begin, const char *end)
        : D_length(end - begin), D_data(new char[D_length + 1]) {
        strcpy_s(D_data, D_length, begin);
        D_data[D_length] = '\0';
    }
    string::string(const char* str)
        : D_length(strlen(str)), D_data(new char[D_length + 1]) {
        strcpy_s(D_data, D_length + 1, str);
    }
    string::string() : string("") {}
    string::~string() {
        clear_data();
    }

    string::string(const string& str)
        : D_length(str.D_length), D_data(new char[str.D_length + 1]) {
        strcpy_s(D_data, D_length + 1, str.D_data);
    }
    string string::operator=(const string& str) {
        if (this != &str) {
            clear_data();
            D_length = str.D_length;
            D_data   = new char[D_length + 1];
            strcpy_s(D_data, D_length + 1, str.D_data);
        }
        return *this;
    }

    string::string(string&& str) : D_length(str.D_length), D_data(str.D_data) {
        str.D_data   = nullptr;
        str.D_length = 0;
    }

    string string::operator=(string&& str) {
        if (this != &str) {
            clear_data();
            D_length     = str.D_length;
            D_data       = str.D_data;
            str.D_data   = nullptr;
            str.D_length = 0;
        }
        return *this;
    }

    bool string::operator==(const string& str) const {
        if (D_length != str.D_length) {
            return false;
        }
        return strcmp(D_data, str.D_data) == 0;
    }

    string_builder::string_builder(size_t bufsz)
        : D_bufsz(bufsz), D_length(0), D_buf(new char[bufsz]) {}

    string_builder::string_builder(const char* str)
        : string_builder(strlen(str) + 1) {
        append(str);
    }
    string_builder::string_builder(string str)
        : string_builder(str.size() + 1) {
        append(str.c_str());
    }

    string_builder::~string_builder() {
        delete[] D_buf;
    }

    void string_builder::append(const char* str) {
        size_t str_len = strlen(str);
        if (D_length + str_len + 1 > D_bufsz) {
            // 超出缓冲区大小
            return;
        }
        strcpy_s(D_buf + D_length, D_bufsz - D_length, str);
        D_length += str_len;
    }

    void string_builder::append(string str) {
        size_t str_len = str.length();
        if (D_length + str_len + 1 > D_bufsz) {
            // 超出缓冲区大小
            return;
        }
        strcpy_s(D_buf + D_length, D_bufsz - D_length, str.c_str());
        D_length += str_len;
    }

    void string_builder::append(char ch) {
        if (D_length + 2 > D_bufsz) {
            // 超出缓冲区大小
            return;
        }
        D_buf[D_length]     = ch;
        D_length++;
        D_buf[D_length]     = '\0';
    }
}  // namespace util