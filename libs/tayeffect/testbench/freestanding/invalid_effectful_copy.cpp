#include <tay/effect.h>

struct effect {};
struct request {
    using effect_type = effect;
    using value_type  = void;
};

void invalid_effectful_copy() {
    auto program = tay::effect::perform<int>(request{});
    auto copy    = program;
    (void)copy;
}
