/**
 * @file heap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 二叉堆算法的用法。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/algo/heap.h>
#include <tay/static_vector.h>

#include <cstdio>

int main() {
    tay::static_vector<int, 8> values{3, 1, 4, 2};
    tay::make_heap(values);
    std::printf("after make_heap: top=%d, valid=%s\n", values.front(),
                tay::is_heap(values) ? "yes" : "no");

    static_cast<void>(values.push_back(9));
    tay::push_heap(values);
    std::printf("after push_heap(9): top=%d\n", values.front());

    tay::pop_heap(values);
    const int maximum = values.back();
    static_cast<void>(values.pop_back());
    std::printf("pop_heap removed=%d, new-top=%d, valid=%s\n", maximum, values.front(),
                tay::is_heap(values) ? "yes" : "no");
    return 0;
}
