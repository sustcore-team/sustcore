#include <tay/guard.h>
#include <tay/unique_ptr.h>

#include <utility>

namespace {
    struct record {
        int value;
    };
}  // namespace

int use_raii(bool commit) {
    int cleanups = 0;
    {
        tay::guard cleanup{[&] { ++cleanups; }};
        if (commit) {
            cleanup.release();
        }
    }

    tay::unique_ptr<record> first{new record{3}};
    tay::unique_ptr<record> second{std::move(first)};
    auto values = tay::make_unique<int[]>(2);
    values[0] = second->value;
    values[1] = cleanups;
    return values[0] + values[1];
}
