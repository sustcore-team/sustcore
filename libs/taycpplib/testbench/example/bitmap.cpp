#include <tay/bitmap.h>

#include <cstdio>

int main() {
    tay::static_bitmap<80> ready;
    static_cast<void>(ready.set(1));
    static_cast<void>(ready.set(7));
    static_cast<void>(ready.set(65));

    tay::static_bitmap<80> enabled;
    enabled.set();
    static_cast<void>(enabled.reset(7));

    auto active = ready & enabled;
    std::printf("ready=%zu active=%zu first=%zu\n",
                ready.count(), active.count(), active.find_first_set());

    auto dynamic = tay::bitmap<>::try_create(130);
    if (!dynamic) {
        std::printf("dynamic bitmap allocation failed: %d\n",
                    static_cast<int>(dynamic.error()));
        return 1;
    }
    static_cast<void>(dynamic->set(129));
    std::printf("dynamic bits=%zu last=%s\n", dynamic->size(),
                (*dynamic)[129] ? "set" : "clear");
    return 0;
}
