/**
 * @file static_vector.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::static_vector 的固定容量存储。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/static_vector.h>

#include <cstdio>

int main() {
    tay::static_vector<int, 5> values{10, 20, 40};
    static_cast<void>(values.insert(values.begin() + 2, 30));
    static_cast<void>(values.push_back(50));

    std::printf("static_vector size=%zu capacity=%zu: [", values.size(), values.capacity());
    for (size_t i = 0; i < values.size(); ++i) {
        std::printf("%s%d", i == 0 ? "" : ", ", values[i]);
    }
    std::printf("]\n");

    auto overflow = values.push_back(60);
    std::printf("push beyond capacity: %s (error=%d)\n", overflow ? "success" : "rejected",
                overflow ? 0 : static_cast<int>(overflow.error()));
    return 0;
}
