#include <tay/effect.h>

#include <utility>

struct effect {};
struct request {
    using effect_type = effect;
    using value_type  = void;
};
struct policy {
    using handled_effects = tay::effect::set<>;
};
struct handler {
    tay::expected<void, int> handle(request &&) noexcept {
        return {};
    }
};

void invalid_unhandled_effect() {
    handler effects;
    auto program = tay::effect::perform<int>(request{});
    (void)tay::effect::endpoint<policy>::close(std::move(program), effects);
}
