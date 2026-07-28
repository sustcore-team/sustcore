#include <tay/range.h>

int main() {
    constexpr tay::range<int> outer{1, 8};
    constexpr tay::range<int> inner{3, 6};
    static_assert(outer.size() == 7 && tay::within(outer, inner));
    static_assert(tay::intersection(outer, tay::range<int>{5, 10}) ==
                  tay::range<int>{5, 8});

    return 0;
}
