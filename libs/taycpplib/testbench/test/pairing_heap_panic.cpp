/**
 * @file pairing_heap_panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay intrusive pairing heap 拒绝重复链接。
 * @version 0.1.0-dev.1
 * @date 2026-08-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/intrusive.h>
#include <tay/pairing_heap.h>

namespace {
    struct node {
        int key;
        tay::intrusive_pairing_heap_hook<node> hook;
    };

    struct node_less {
        [[nodiscard]] constexpr bool operator()(const node &left, const node &right) noexcept {
            return left.key < right.key;
        }
    };

    using locate_node =
        tay::locate_member<node, tay::intrusive_pairing_heap_hook<node>, &node::hook>;
    using node_heap = tay::intrusive_pairing_heap<node, locate_node, node_less>;
}  // namespace

int main() {
    node value{.key = 1};
    node_heap first;
    node_heap second;
    first.push(value);
    second.push(value);
    return 0;
}
