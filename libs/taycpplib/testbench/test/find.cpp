#include <tay/algo/find.h>

#include <cassert>
#include <cstddef>

namespace {
    struct record {
        int key;
        int payload;

        constexpr bool operator==(const record&) const = default;
    };

    template <typename T, std::size_t Size>
    constexpr bool prefix_equal(const T (&values)[Size], const T* expected,
                                std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            if (!(values[index] == expected[index])) {
                return false;
            }
        }
        return true;
    }

    constexpr bool find_algorithms_work() {
        int values[] = {4, 1, 3, 1, 2, 1};
        if (tay::find(values, 3) != values + 2 ||
            tay::find(values + 0, values + 6, 9) != values + 6)
        {
            return false;
        }
        if (tay::find_if(values, [](int value) { return value % 2 == 0; }) !=
            values)
        {
            return false;
        }
        if (!tay::contains(values, 2) || tay::contains(values, 8)) {
            return false;
        }

        auto new_end         = tay::remove(values, 1);
        const int expected[] = {4, 3, 2};
        return new_end == values + 3 && prefix_equal(values, expected, 3);
    }
}  // namespace

static_assert(find_algorithms_work());

int main() {
    int values[] = {1, 2, 3, 4, 5, 6};
    auto new_end =
        tay::remove_if(values, [](int value) { return value % 2 != 0; });
    const int expected[] = {2, 4, 6};
    assert(new_end == values + 3);
    assert(prefix_equal(values, expected, 3));

    record records[] = {{3, 30}, {1, 10}, {2, 20}, {1, 11}};
    assert(tay::find(records, 2, &record::key) == records + 2);
    assert(tay::find_if(
               records, [](int key) { return key < 2; }, &record::key) ==
           records + 1);
    assert(tay::contains(records, 3, &record::key));

    auto records_end                = tay::remove(records, 1, &record::key);
    const record expected_records[] = {{3, 30}, {2, 20}};
    assert(records_end == records + 2);
    assert(prefix_equal(records, expected_records, 2));

    return 0;
}
