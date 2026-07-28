#include <tay/algo/sort.h>

namespace {
    struct record {
        int key;
        int payload;
    };

    constexpr bool sort_works() {
        int values[] = {4, 1, 3, 2};
        if (tay::sort(values) != values + 4) {
            return false;
        }
        for (int index = 0; index < 4; ++index) {
            if (values[index] != index + 1) {
                return false;
            }
        }

        record records[] = {{2, 20}, {1, 10}, {3, 30}};
        tay::sort(records, {}, &record::key);
        return records[0].key == 1 && records[1].key == 2 &&
               records[2].key == 3;
    }
}  // namespace

static_assert(sort_works());

int main() {
    return sort_works() ? 0 : 1;
}
