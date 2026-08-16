/**
 * @file pairing_heap_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 intrusive pairing heap 的 freestanding 编译和链接契约。
 * @version 0.1.0-dev.1
 * @date 2026-08-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/intrusive.h>
#include <tay/pairing_heap.h>
#include <tay/utility.h>

#include <functional>
#include <type_traits>

namespace {
    struct node {
        unsigned key;
        tay::intrusive_pairing_heap_hook<node> hook;
    };

    using locate_node =
        tay::locate_member<node, tay::intrusive_pairing_heap_hook<node>, &node::hook>;
    using node_heap =
        tay::intrusive_pairing_heap<node, locate_node,
                                    tay::projected_compare<std::ranges::less, unsigned node::*>>;

    static_assert(!std::is_default_constructible_v<node_heap>);
    static_assert(std::is_same_v<decltype(std::declval<node_heap &>().top()), node *>);
}  // namespace

void pairing_heap_freestanding_contract() {
    node first{.key = 2};
    node second{.key = 1};
    node_heap heap(locate_node{}, node_heap::compare_type(std::ranges::less{}, &node::key));
    heap.push(first);
    heap.push(second);
    static_cast<void>(heap.remove(first.hook));
    static_cast<void>(heap.pop_min());
}
