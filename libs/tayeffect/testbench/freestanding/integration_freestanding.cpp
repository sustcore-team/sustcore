#include <tay/staged.h>

namespace {
    enum class error { failed };

    struct write_environment final {
        tay::expected<void, error> write(int* target) noexcept {
            *target = 1;
            return {};
        }
    };

    struct write_op final {
        int* target;

        template <typename Environment>
        tay::expected<void, error> run(Environment& environment) const noexcept {
            return environment.write(target);
        }
    };

    struct op final : tay::staged::operation<op> {
        using plan_type    = int*;
        using receipt_type = void;
        using result_type  = void;
        using error_type   = error;

        tay::expected<plan_type, error> plan(int& value) const {
            return &value;
        }

        auto perform(const plan_type& plan) const {
            return tay::effect::program<write_op>{plan};
        }

        write_environment effect_environment(const plan_type&) const noexcept {
            return {};
        }

        void commit(const plan_type& plan) const noexcept {
            *plan = 2;
        }
    };

    constinit op operation{};
}  // namespace

extern "C" int tayeffect_integration_freestanding_contract() {
    int value = 0;
    return operation.execute(value) ? value : -1;
}
