#include <tay/array_list.h>

#include <cassert>
#include <type_traits>

namespace {
    struct move_only {
        int value;

        explicit move_only(int initial) noexcept : value(initial) {}
        move_only(const move_only&)            = delete;
        move_only& operator=(const move_only&) = delete;
        move_only(move_only&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        move_only& operator=(move_only&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
        ~move_only() noexcept = default;
    };
}  // namespace

static_assert(std::is_same_v<tay::array_list<int, tay::allocator<int>>,
                             tay::array_list<int, tay::allocator<int>>>);

int main() {
    using list_type = tay::array_list<int, tay::allocator<int>>;

    list_type values{1, 2, 4};
    assert(values.size() == 3);
    assert(values.capacity() >= values.size());
    assert(values.push_back(5));
    auto inserted = values.insert(values.begin() + 2, 3);
    assert(inserted && **inserted == 3);
    assert(values.size() == 5);

    const int middle[] = {7, 8};
    auto range         = values.insert(values.begin() + 1, middle, middle + 2);
    assert(range && **range == 7);
    assert(values.size() == 7);
    assert(values[0] == 1 && values[1] == 7 && values[2] == 8);

    auto erased = values.erase(values.begin() + 1, values.begin() + 3);
    assert(erased && **erased == 2);
    assert(values.size() == 5);
    assert(values.pop_back());
    assert(values.back() == 4);

    auto missing = values.at(100);
    assert(!missing && missing.error() == tay::error_code::OUT_OF_RANGE);
    assert(values.resize(8, 9));
    assert(values.size() == 8 && values.back() == 9);
    assert(values.shrink_to_fit());
    assert(values.capacity() == values.size());

    auto copied = list_type::try_create(values);
    assert(copied && *copied == values);
    list_type assigned;
    assert(assigned.assign(values.begin(), values.end()));
    assert(assigned == values);

    tay::array_list<move_only, tay::allocator<move_only>> objects;
    assert(objects.emplace_back(1));
    assert(objects.emplace_back(2));
    assert(objects.insert(objects.begin() + 1, move_only(3)));
    assert(objects[0].value == 1);
    assert(objects[1].value == 3);
    assert(objects[2].value == 2);
    return 0;
}
