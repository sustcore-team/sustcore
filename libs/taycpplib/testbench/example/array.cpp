#include <tay/array.h>

#include <cstdio>

int main() {
    tay::static_array<int, 4> inline_values;
    inline_values.fill(7);

    int borrowed[] = {1, 2, 3, 4};
    tay::array_view<int, 4> view(borrowed);
    view[1] = 20;

    auto dynamic_values = tay::array<int, 3>::try_create();
    if (!dynamic_values) {
        std::printf("dynamic array allocation failed: %d\n",
                    static_cast<int>(dynamic_values.error()));
        return 1;
    }
    dynamic_values->fill(9);

    std::printf("static: [%d, %d, %d, %d]\n",
                inline_values[0], inline_values[1], inline_values[2],
                inline_values[3]);
    std::printf("view changed source: [%d, %d, %d, %d]\n",
                borrowed[0], borrowed[1], borrowed[2], borrowed[3]);
    std::printf("dynamic size=%zu: [%d, %d, %d]\n",
                dynamic_values->size(), (*dynamic_values)[0],
                (*dynamic_values)[1], (*dynamic_values)[2]);
    return 0;
}
