/**
 * @file fifo.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::fifo 的环形队列操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/fifo.h>

#include <cstddef>
#include <cstdio>

int main() {
    tay::static_fifo<int, 3> queue;
    static_cast<void>(queue.push(10));
    static_cast<void>(queue.push(20));
    std::printf("pop: %d\n", *queue.pop());
    static_cast<void>(queue.push(30));
    static_cast<void>(queue.push(40));

    std::printf("wrapped FIFO: [");
    bool first = true;
    for (int value : queue) {
        std::printf("%s%d", first ? "" : ", ", value);
        first = false;
    }
    std::printf("]\n");

    auto bytes = tay::byte_fifo<>::try_create(8);
    if (!bytes)
        return 1;
    const std::byte message[] = {std::byte{'O'}, std::byte{'K'}};
    static_cast<void>(bytes->try_write(tay::array_view<const std::byte>(message, 2)));
    std::byte output[2]{};
    static_cast<void>(bytes->try_read(tay::array_view<std::byte>(output, 2)));
    std::printf("byte FIFO: %c%c\n", static_cast<char>(output[0]), static_cast<char>(output[1]));
    return 0;
}
