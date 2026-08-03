#include <tay/staged.h>

#include <cassert>
#include <utility>

namespace {
    enum class error { busy };
    enum class state { disabled, enabled };
    struct irq_control {};
    struct descriptor {
        state current = state::disabled;
    };

    struct unmask_request {
        using effect_type = irq_control;
        using value_type  = void;
        unsigned irq;
    };

    struct irq_handler {
        bool fail;
        int* calls;
        tay::expected<void, error> handle(unmask_request&&) noexcept {
            ++*calls;
            return fail ? tay::expected<void, error>(tay::unexpect, error::busy)
                        : tay::expected<void, error>();
        }
    };

    struct irq_policy {
        using handled_effects = tay::effect::set<irq_control>;
    };

    struct static_irq_environment final {
        bool fail;
        int* calls;

        tay::expected<void, error> unmask(unsigned) noexcept {
            ++*calls;
            return fail ? tay::expected<void, error>(tay::unexpect, error::busy)
                        : tay::expected<void, error>();
        }
    };

    struct static_unmask_op final {
        unsigned irq;

        template <typename Environment>
        tay::expected<void, error> run(Environment& environment) const noexcept {
            return environment.unmask(irq);
        }
    };

    struct handler_enable final : tay::staged::operation<handler_enable> {
        struct plan_type {
            descriptor* target;
            unsigned irq;
            bool fail;
            int* calls;
        };
        using receipt_type    = void;
        using result_type     = void;
        using error_type      = error;
        using effect_endpoint = tay::effect::endpoint<irq_policy>;

        tay::expected<plan_type, error> plan(descriptor& target, unsigned irq, bool fail,
                                             int& calls) const {
            return plan_type{&target, irq, fail, &calls};
        }
        auto perform(const plan_type& plan) const {
            return tay::effect::perform<error>(unmask_request{plan.irq});
        }
        irq_handler effect_handler(const plan_type& plan) const noexcept {
            return {plan.fail, plan.calls};
        }
        void commit(const plan_type& plan) const noexcept {
            plan.target->current = state::enabled;
        }
    };

    struct static_handler_enable final : tay::staged::operation<static_handler_enable> {
        struct plan_type {
            descriptor* target;
            unsigned irq;
            bool fail;
            int* calls;
        };
        using receipt_type = void;
        using result_type  = void;
        using error_type   = error;

        tay::expected<plan_type, error> plan(descriptor& target, unsigned irq, bool fail,
                                             int& calls) const {
            return plan_type{&target, irq, fail, &calls};
        }
        auto perform(const plan_type& plan) const {
            return tay::effect::program<static_unmask_op>{plan.irq};
        }
        static_irq_environment effect_environment(const plan_type& plan) const noexcept {
            return {plan.fail, plan.calls};
        }
        void commit(const plan_type& plan) const noexcept {
            plan.target->current = state::enabled;
        }
    };

    constinit handler_enable enable{};
    constinit static_handler_enable static_enable{};
}  // namespace

int main() {
    descriptor target;
    int calls         = 0;
    const auto failed = enable.execute(target, 4U, true, calls);
    assert(!failed && target.current == state::disabled && calls == 1);
    const auto succeeded = enable.execute(target, 4U, false, calls);
    assert(succeeded && target.current == state::enabled && calls == 2);

    descriptor static_target;
    int static_calls         = 0;
    const auto static_failed = static_enable.execute(static_target, 5U, true, static_calls);
    assert(!static_failed && static_target.current == state::disabled && static_calls == 1);
    const auto static_succeeded = static_enable.execute(static_target, 5U, false, static_calls);
    assert(static_succeeded && static_target.current == state::enabled && static_calls == 2);
}
