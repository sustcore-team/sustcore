#include <tay/algo/misc.h>

#include <cassert>
#include <cstddef>

namespace {
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

    struct record {
        int key;
        int payload;

        constexpr bool operator==(const record&) const = default;
    };

    struct move_only {
        int value;

        constexpr explicit move_only(int value) : value(value) {}
        constexpr move_only(const move_only&)            = delete;
        constexpr move_only& operator=(const move_only&) = delete;
        constexpr move_only(move_only&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        constexpr move_only& operator=(move_only&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
    };

    constexpr bool misc_algorithms_work() {
        int values[]                = {1, 1, 2, 2, 2, 1};
        auto new_end                = tay::unique(values);
        const int expected_unique[] = {1, 2, 1};
        if (new_end != values + 3 || !prefix_equal(values, expected_unique, 3))
        {
            return false;
        }

        int reversed[]                = {1, 2, 3, 4, 5};
        const int expected_reversed[] = {5, 4, 3, 2, 1};
        return tay::reverse(reversed) == reversed + 5 &&
               prefix_equal(reversed, expected_reversed, 5);
    }
}  // namespace

static_assert(misc_algorithms_work());

int main() {
    int alias_values[] = {1, 1, 2, 2};
    assert(tay::unique(alias_values) == alias_values + 2);

    record records[] = {{1, 10}, {1, 11}, {2, 20}, {2, 21}, {3, 30}};
    auto records_end = tay::unique(records, {}, &record::key);
    const record expected_records[] = {{1, 10}, {2, 20}, {3, 30}};
    assert(records_end == records + 3);
    assert(prefix_equal(records, expected_records, 3));

    move_only values[] = {move_only(1), move_only(2), move_only(3),
                          move_only(4)};
    assert(tay::reverse(values) == values + 4);
    for (std::size_t index = 0; index < 4; ++index) {
        assert(values[index].value == static_cast<int>(4 - index));
    }

    move_only* empty = nullptr;
    assert(tay::reverse(empty, empty) == empty);

    return 0;
}
