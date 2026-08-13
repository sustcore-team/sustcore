/**
 * @file storage.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 容器存储策略和容量管理。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/algo/binary_search.h>
#include <tay/algo/heap.h>
#include <tay/array.h>
#include <tay/bitmap.h>
#include <tay/fifo.h>
#include <tay/flat.h>
#include <tay/functional.h>
#include <tay/list.h>
#include <tay/set.h>
#include <tay/slot_map.h>
#include <tay/static_vector.h>
#include <tay/tree.h>

#include <cassert>
#include <cstddef>

namespace {
    struct list_node;
    using list_hook = tay::intrusive_list_hook<list_node*, list_node*>;
    struct list_node {
        int value;
        list_hook list;
        tay::compact_intrusive_tree_hook<list_node> tree;
    };
    using locate_list = tay::locate_member<list_node, list_hook, &list_node::list>;
    using locate_tree = tay::locate_member<list_node, tay::compact_intrusive_tree_hook<list_node>,
                                           &list_node::tree>;

    int double_value(int value) noexcept {
        return value * 2;
    }
}  // namespace

int main() {
    tay::static_vector<int, 4> vector{1, 3};
    assert(vector.insert(vector.begin() + 1, 2));
    assert(vector.size() == 3 && vector[1] == 2);
    assert(vector.resize(4, 4));
    assert(!vector.push_back(5));

    tay::static_array<int, 3> inline_array;
    inline_array.fill(7);
    assert(inline_array.size() == 3 && inline_array[2] == 7);
    int borrowed[] = {1, 2, 3};
    tay::array_view<int, 3> view(borrowed);
    view[1] = 9;
    assert(borrowed[1] == 9);

    tay::array_view<int> pointer_range(borrowed, borrowed + 3);
    int sum = 0;
    for (int value : pointer_range) {
        sum += value;
    }
    assert(sum == 13);

    pointer_range.foreach ([](int& value) noexcept { ++value; });
    assert(borrowed[0] == 2 && borrowed[1] == 10 && borrowed[2] == 4);

    const tay::array_view<int, 3> const_pointer_range(borrowed, borrowed + 3);
    assert(const_pointer_range.front() == 2 && const_pointer_range.back() == 4);
    int view_sum = 0;
    const_pointer_range.foreach ([&view_sum](const int& value) noexcept { view_sum += value; });
    assert(view_sum == 16);
    auto dynamic_array = tay::array<int, 2>::try_create();
    assert(dynamic_array && dynamic_array->size() == 2);

    tay::static_bitmap<70> bits;
    assert(bits.set(1) && bits.set(69));
    assert(bits.count() == 2 && bits.find_first_set() == 1);
    auto dynamic_bits = tay::bitmap<>::try_create(130);
    assert(dynamic_bits && dynamic_bits->set(129));

    tay::static_fifo<int, 3> queue;
    assert(queue.push(1) && queue.push(2) && queue.push(3));
    assert(queue.full() && *queue.pop() == 1);
    assert(queue.push(4));
    int expected = 2;
    for (int value : queue) assert(value == expected++);

    auto dynamic_queue = tay::fifo<int>::try_create(2);
    assert(dynamic_queue && dynamic_queue->push(1) && dynamic_queue->push(2));
    assert(dynamic_queue->reserve(5));
    assert(*dynamic_queue->pop() == 1 && dynamic_queue->capacity() == 5);

    auto bytes = tay::byte_fifo<>::try_create(5);
    assert(bytes);
    std::byte input[] = {std::byte{1}, std::byte{2}, std::byte{3}};
    assert(bytes->try_write(tay::array_view<const std::byte>(input, 3)));
    std::byte output[2]{};
    assert(bytes->try_read(tay::array_view<std::byte>(output, 2)));
    assert(output[0] == std::byte{1} && bytes->size() == 1);

    int offset    = 3;
    auto callable = [&offset](int value) noexcept { return value + offset; };
    tay::function_ref<int(int) noexcept> ref(callable);
    assert(ref(4) == 7);
    tay::function_ref<int(int) noexcept> direct_ref(double_value);
    assert(direct_ref(4) == 8);
    tay::inplace_function<int(int), 32> function(callable);
    auto copied = function;
    assert(copied(5) == 8);

    tay::static_hash_set<int, 4> fixed_set;
    assert(fixed_set.insert(1) && fixed_set.insert(2));
    assert(fixed_set.contains(2));
    assert(fixed_set.reserve(4));
    assert(fixed_set.insert(3) && fixed_set.insert(4));
    assert(!fixed_set.insert(5));
    auto fixed_copy = fixed_set;
    assert(fixed_copy.size() == 4 && fixed_copy.contains(3));

    tay::flat_set<int> flat_values;
    assert(flat_values.insert(3));
    assert(flat_values.insert(1));
    assert(flat_values.insert(2));
    assert(flat_values.contains(2));

    tay::flat_map<int, int> flat_pairs;
    assert(flat_pairs.try_emplace(2, 20));
    assert(flat_pairs.try_emplace(1, 10));
    assert(flat_pairs.at(2) && *flat_pairs.at(2) == 20);

    tay::static_slot_map<int, 3> slots;
    auto first_handle  = slots.emplace(10);
    auto second_handle = slots.emplace(20);
    assert(first_handle && second_handle);
    assert(slots.erase(*first_handle));
    assert(!slots.contains(*first_handle));
    auto replacement = slots.emplace(30);
    assert(replacement && replacement->index == first_handle->index &&
           replacement->generation != first_handle->generation);

    int sorted[] = {1, 2, 2, 4};
    assert(tay::lower_bound(sorted, 2) == sorted + 1);
    assert(tay::upper_bound(sorted, 2) == sorted + 3);
    assert(tay::binary_search(sorted, 4));
    int heap[] = {3, 1, 4, 2};
    tay::make_heap(heap);
    assert(tay::is_heap(heap));
    tay::pop_heap(heap);
    assert(heap[3] == 4);

    list_node a{1}, b{2}, c{3};
    tay::intrusive_list<list_node, locate_list> list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);
    assert(list.size() == 3);
    const auto& const_list = list;
    int const_sum          = 0;
    for (const list_node* node : const_list) const_sum += node->value;
    assert(const_sum == 6);
    auto reverse = list.rbegin();
    assert((*reverse)->value == 3);
    tay::intrusive_list<list_node, locate_list> moved;
    moved.splice(moved.end(), list, list.iterator_to(&b), list.end());
    assert(list.size() == 1 && moved.size() == 2);

    tay::intrusive_tree<list_node, locate_tree> tree;
    tree.link_back(a, b);
    tree.link_back(a, c);
    assert(tree.parent(b) == &a && tree.depth(c) == 1);
    assert(tree.lca(&b, &c) == &a);
    int preorder_sum = 0;
    for (list_node* node : tree.preorder(a)) preorder_sum += node->value;
    assert(preorder_sum == 6);
    int child_count = 0;
    for ([[maybe_unused]] list_node* node : tree.children(a)) ++child_count;
    assert(child_count == 2);
    tree.unlink(b);
    assert(tree.is_root(b));
    return 0;
}
