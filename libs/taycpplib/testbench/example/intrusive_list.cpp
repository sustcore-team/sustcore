/**
 * @file intrusive_list.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 非拥有式侵入链表。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/list.h>

#include <cstdio>

namespace {
    struct number;
    using number_hook = tay::intrusive_list_hook<number*, number*>;

    struct number {
        int value;
        number_hook hook;
    };

    using locate_number = tay::locate_member<number, number_hook, &number::hook>;
    using number_list   = tay::intrusive_list<number, locate_number>;

    void print(const char* label, number_list& values) {
        std::printf("%-12s [", label);
        bool first = true;
        for (number* value : values) {
            std::printf("%s%d", first ? "" : ", ", value->value);
            first = false;
        }
        std::printf("]\n");
    }
}  // namespace

int main() {
    number ten{10};
    number twenty{20};
    number thirty{30};
    number forty{40};
    number fifty{50};

    number_list values;
    values.push_front(&twenty);
    values.push_front(&ten);
    values.push_back(&forty);
    print("created:", values);

    values.insert(values.iterator_to(&forty), &thirty);
    static_cast<void>(values.erase(values.iterator_to(&twenty)));
    print("edited:", values);

    number_list tail;
    tail.push_back(&fifty);
    values.splice(values.end(), tail);
    print("spliced:", values);

    values.clear();
    return 0;
}
