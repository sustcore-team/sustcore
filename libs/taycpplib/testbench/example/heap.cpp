#include <tay/algo/heap.h>
#include <tay/static_vector.h>

#include <cstdio>

int main() {
    tay::static_vector<int, 8> values{3, 1, 4, 2};
    tay::make_heap(values);
    std::printf("after make_heap: top=%d, valid=%s\n",
                values.front(), tay::is_heap(values) ? "yes" : "no");

    static_cast<void>(values.push_back(9));
    tay::push_heap(values);
    std::printf("after push_heap(9): top=%d\n", values.front());

    tay::pop_heap(values);
    const int maximum = values.back();
    static_cast<void>(values.pop_back());
    std::printf("pop_heap removed=%d, new-top=%d, valid=%s\n",
                maximum, values.front(),
                tay::is_heap(values) ? "yes" : "no");
    return 0;
}
