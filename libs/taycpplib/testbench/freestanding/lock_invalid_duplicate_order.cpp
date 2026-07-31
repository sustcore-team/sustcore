#include <tay/lock.h>

namespace {
    struct first_guard {};
    struct second_guard {};

    using invalid_guard =
        tay::context_lock_guard<tay::spinlock,
                                tay::guard_stage<100, first_guard>,
                                tay::guard_stage<100, second_guard>>;
}  // namespace

void duplicate_context_guard_order_is_invalid(tay::spinlock& lock) {
    invalid_guard held{lock};
}
