#include <tay/effect.h>

#include <utility>

namespace {
    enum class error { failed };
    struct io {};
    struct request {
        using effect_type = io;
        using value_type  = int;
        int value;
    };
    struct handler {
        tay::expected<int, error> handle(request&& value) noexcept {
            return value.value;
        }
    };
    struct policy {
        using handled_effects = tay::effect::set<io>;
    };
}  // namespace

extern "C" int tayeffect_effect_freestanding_contract() {
    handler effects;
    auto program = tay::effect::transform(tay::effect::perform<error>(request{4}),
                                          [](int value) { return value + 1; });
    auto result  = tay::effect::endpoint<policy>::close(std::move(program), effects);
    return result ? *result : -1;
}
