#include <tay/slot_map.h>

#include <cstdio>

int main() {
    tay::static_slot_map<const char*, 3> objects;
    auto alpha = objects.emplace("alpha");
    auto beta = objects.emplace("beta");
    if (!alpha || !beta) return 1;

    std::printf("alpha handle=(%zu, %zu), value=%s\n",
                alpha->index, alpha->generation, *objects.get(*alpha));

    static_cast<void>(objects.erase(*alpha));
    auto gamma = objects.emplace("gamma");
    if (!gamma) return 1;

    std::printf("old alpha valid: %s\n",
                objects.contains(*alpha) ? "yes" : "no");
    std::printf("gamma handle=(%zu, %zu), reused-index=%s\n",
                gamma->index, gamma->generation,
                gamma->index == alpha->index ? "yes" : "no");

    std::printf("dense values:");
    for (const char* value : objects) std::printf(" %s", value);
    std::printf("\n");
    return 0;
}
