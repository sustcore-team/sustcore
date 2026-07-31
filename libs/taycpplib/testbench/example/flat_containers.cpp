#include <tay/flat.h>

#include <cstdio>

int main() {
    tay::flat_set<int> ids;
    static_cast<void>(ids.insert(30));
    static_cast<void>(ids.insert(10));
    static_cast<void>(ids.insert(20));
    static_cast<void>(ids.insert(20));

    std::printf("flat_set: {");
    bool first = true;
    for (int id : ids) {
        std::printf("%s%d", first ? "" : ", ", id);
        first = false;
    }
    std::printf("}\n");

    tay::flat_map<int, const char*> names;
    static_cast<void>(names.try_emplace(2, "two"));
    static_cast<void>(names.try_emplace(1, "one"));
    static_cast<void>(names.insert_or_assign(2, "TWO"));

    std::printf("flat_map:\n");
    for (auto entry : names) {
        std::printf("  %d -> %s\n", entry.first, entry.second);
    }
    return 0;
}
