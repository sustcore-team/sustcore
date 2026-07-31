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

void tay_storage_freestanding_contract() {
    tay::static_vector<int, 4> values;
    (void)values.push_back(1);
    tay::static_bitmap<64> bits;
    (void)bits.set(4);
    tay::static_fifo<int, 4> fifo;
    (void)fifo.push(1);
    tay::static_hash_set<int, 4> set;
    (void)set.insert(1);
    tay::static_slot_map<int, 4> slots;
    (void)slots.emplace(1);
    int sorted[] = {1, 2, 3};
    (void)tay::binary_search(sorted, 2);
    tay::make_heap(sorted);
}
