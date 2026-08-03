#include <tay/effect.h>

struct effect {};
struct request {
    using effect_type = effect;
    using value_type  = void;
};
struct policy {
    using handled_effects = tay::effect::set<effect>;
};
struct handler {
    tay::expected<void, int> handle(request &&) noexcept {
        return {};
    }
};

void invalid_close_lvalue() {
    handler effects;
    auto program = tay::effect::perform<int>(request{});
    (void)tay::effect::endpoint<policy>::close(program, effects);
}
