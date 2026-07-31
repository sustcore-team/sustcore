#include <tay/algo/binary_search.h>

#include <cstdio>

namespace {
    struct record {
        int key;
        const char* name;
    };
}

int main() {
    record records[] = {
        {1, "one"}, {2, "two-a"}, {2, "two-b"}, {4, "four"}};
    auto project_key = [](const record& value) { return value.key; };

    auto lower = tay::lower_bound(records, 2, std::ranges::less{}, project_key);
    auto upper = tay::upper_bound(records, 2, std::ranges::less{}, project_key);
    auto range = tay::equal_range(records, 2, std::ranges::less{}, project_key);

    std::printf("lower_bound(2): %s\n", lower->name);
    std::printf("upper_bound(2) index: %td\n", upper - records);
    std::printf("equal_range(2) count: %td\n", range.second - range.first);
    std::printf("binary_search(3): %s\n",
                tay::binary_search(records, 3, std::ranges::less{}, project_key)
                    ? "found" : "missing");
    return 0;
}
