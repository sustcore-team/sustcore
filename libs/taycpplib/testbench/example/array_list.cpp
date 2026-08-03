/**
 * @file array_list.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::array_list 的连续存储和元素操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/array_list.h>

#include <cstdio>

namespace {
    using number_list = tay::array_list<int>;

    template <class Result>
    bool succeeded(const Result& result, const char* operation) {
        if (result) {
            return true;
        }
        std::printf("%s failed with error code %u\n", operation,
                    static_cast<unsigned>(result.error()));
        return false;
    }

    void print(const char* label, const number_list& values) {
        std::printf("%-12s [", label);
        for (number_list::size_type index = 0; index < values.size(); ++index) {
            std::printf("%s%d", index == 0 ? "" : ", ", values[index]);
        }
        std::printf("] size=%zu capacity=%zu\n", values.size(), values.capacity());
    }
}  // namespace

int main() {
    number_list values{10, 20, 40};
    print("created:", values);

    auto inserted = values.insert(values.begin() + 2, 30);
    if (!succeeded(inserted, "insert") || !succeeded(values.push_back(50), "push_back")) {
        return 1;
    }
    print("expanded:", values);

    const int extra[] = {60, 70};
    if (!succeeded(values.insert(values.end(), extra, extra + 2), "range insert") ||
        !succeeded(values.erase(values.begin(), values.begin() + 2), "erase"))
    {
        return 1;
    }
    print("edited:", values);

    if (!succeeded(values.reserve(16), "reserve")) {
        return 1;
    }
    print("reserved:", values);

    auto outside = values.at(values.size());
    if (!outside) {
        std::printf("at(size) returned error code %u\n", static_cast<unsigned>(outside.error()));
    }
    return 0;
}
