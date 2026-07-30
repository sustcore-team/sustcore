#include <tay/format.h>
#include <tay/path.h>

#include <cstdio>

int main() {
    auto source = tay::path<>::try_create("/srv/app/../data/report.txt");
    auto base   = tay::path<>::try_create("/srv");
    if (!source || !base) {
        return 1;
    }
    auto normalized = source->try_normalize();
    if (!normalized) {
        return 1;
    }
    auto relative = normalized->try_relative_to(*base);
    if (!relative) {
        return 1;
    }
    auto output = tay::format("{} -> {}", *normalized, *relative);
    if (!output) {
        return 1;
    }
    std::puts(output->c_str());
    return 0;
}
