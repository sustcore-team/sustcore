#include <tay/algo/find.h>
#include <tay/algo/misc.h>

namespace {
    struct record {
        int key;
        int payload;
    };

    constexpr bool algorithms_work() {
        int values[] = {3, 1, 1, 2, 2, 4};
        if (tay::find(values, 2) != values + 3 || !tay::contains(values, 4)) {
            return false;
        }

        auto removed_end = tay::remove(values, 1);
        if (removed_end != values + 4) {
            return false;
        }

        auto unique_end = tay::unique(values, removed_end);
        if (unique_end != values + 3) {
            return false;
        }

        if (tay::reverse(values, unique_end) != unique_end) {
            return false;
        }

        record records[] = {{2, 20}, {1, 10}, {3, 30}};
        return tay::find(records, 1, &record::key) == records + 1;
    }
}  // namespace

static_assert(algorithms_work());

int main() {
    return algorithms_work() ? 0 : 1;
}
