/**
 * @file pairing_heap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay intrusive pairing heap 的堆序、删除与生命周期语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/intrusive.h>
#include <tay/pairing_heap.h>
#include <tay/utility.h>

#include <cassert>
#include <functional>
#include <type_traits>

namespace {
    struct node {
        int key;
        int identity;
        tay::intrusive_pairing_heap_hook<node> hook;
    };

    struct key_less {
        bool descending = false;

        [[nodiscard]] constexpr bool operator()(int left, int right) const noexcept {
            return descending ? left > right : left < right;
        }
    };

    using locate_node =
        tay::locate_member<node, tay::intrusive_pairing_heap_hook<node>, &node::hook>;
    using node_order = tay::projected_compare<key_less, int node::*>;
    using node_heap  = tay::intrusive_pairing_heap<node, locate_node, node_order>;
    using node_queue = tay::intrusive_priority_queue<node, locate_node, node_order>;

    static_assert(std::is_same_v<node_heap, node_queue>);
    static_assert(!std::is_copy_constructible_v<node_heap>);
    static_assert(!std::is_move_constructible_v<node_heap>);
}  // namespace

int main() {
    node nodes[] = {
        {.key = 7, .identity = 0}, {.key = 1, .identity = 1}, {.key = 5, .identity = 2},
        {.key = 3, .identity = 3}, {.key = 1, .identity = 4}, {.key = 9, .identity = 5},
        {.key = 4, .identity = 6},
    };
    node *addresses[] = {&nodes[0], &nodes[1], &nodes[2], &nodes[3],
                         &nodes[4], &nodes[5], &nodes[6]};

    node_heap heap(locate_node{}, node_order(key_less{}, &node::key));
    assert(heap.empty());
    assert(heap.top() == nullptr);
    for (node &value : nodes) {
        heap.push(value);
        assert(heap.linked(value));
    }
    assert(heap.size() == 7);
    assert(heap.top()->key == 1);
    for (size_t index = 0; index < 7; ++index) {
        assert(addresses[index] == &nodes[index]);
    }

    assert(heap.remove(nodes[2]) == &nodes[2]);
    assert(!heap.linked(nodes[2]));
    assert(heap.remove(nodes[3].hook) == &nodes[3]);
    assert(!heap.linked(nodes[3].hook));
    assert(heap.size() == 5);

    const int expected[] = {1, 1, 4, 7, 9};
    for (int key : expected) {
        const node_heap &const_heap = heap;
        assert(const_heap.top()->key == key);
        node *removed = heap.pop_min();
        assert(removed->key == key);
        assert(!removed->hook.linked);
        assert(removed->hook.owner == nullptr);
    }
    assert(heap.empty());

    for (node &value : nodes) {
        heap.push(&value);
    }
    heap.clear();
    assert(heap.empty());
    assert(heap.size() == 0);
    for (const node &value : nodes) {
        assert(!value.hook.linked);
        assert(value.hook.heap == nullptr);
        assert(value.hook.parent == nullptr);
        assert(value.hook.first_child == nullptr);
        assert(value.hook.previous == nullptr);
        assert(value.hook.next == nullptr);
    }

    {
        node_heap descending_heap(locate_node{},
                                  node_order(key_less{.descending = true}, &node::key));
        for (node &value : nodes) {
            descending_heap.push(value);
        }
        assert(descending_heap.top()->key == 9);
        assert(descending_heap.pop_min() == &nodes[5]);
    }
    for (const node &value : nodes) {
        assert(!value.hook.linked);
    }

    node stress_nodes[96]{};
    bool present[96]{};
    node_heap stress_heap(locate_node{}, node_order(key_less{}, &node::key));
    for (size_t index = 0; index < 96; ++index) {
        stress_nodes[index].key      = static_cast<int>((index * 37 + 11) % 23);
        stress_nodes[index].identity = static_cast<int>(index);
        present[index]               = true;
        stress_heap.push(stress_nodes[index]);
    }
    for (size_t index = 0; index < 96; index += 4) {
        node *removed = (index % 8 == 0) ? stress_heap.remove(stress_nodes[index])
                                         : stress_heap.remove(stress_nodes[index].hook);
        assert(removed == &stress_nodes[index]);
        present[index] = false;
    }

    while (!stress_heap.empty()) {
        int expected_key = 23;
        for (size_t index = 0; index < 96; ++index) {
            if (present[index] && stress_nodes[index].key < expected_key) {
                expected_key = stress_nodes[index].key;
            }
        }
        node *removed = stress_heap.pop_min();
        assert(removed->key == expected_key);
        assert(removed == &stress_nodes[removed->identity]);
        assert(present[removed->identity]);
        present[removed->identity] = false;
    }
    return 0;
}
