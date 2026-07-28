#include <tay/algobase.h>

int main() {
    static_assert(tay::min(2, 3) == 2);
    static_assert(tay::max(2, 3) == 3);
    static_assert(tay::abs(-4) == 4);
    static_assert(tay::clamp(8, 1, 5) == 5);

    return 0;
}
