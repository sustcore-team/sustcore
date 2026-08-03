#include <tay/effect.h>

#include <utility>

struct effect {};
struct request {
    using effect_type = effect;
    using value_type  = int;
};
struct policy {
    using handled_effects = tay::effect::set<effect>;
};
struct handler {
    tay::expected<int, int> handle(request &&) {
        return 1;
    }
};

void invalid_handler_contract() {
    handler effects;
    auto program = tay::effect::perform<int>(request{});
    (void)tay::effect::endpoint<policy>::close(std::move(program), effects);
}
