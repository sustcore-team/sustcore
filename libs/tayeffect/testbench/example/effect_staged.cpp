#include <tay/staged.h>

#include <cstdio>

namespace {
    enum class error { controller_busy };
    enum class irq_state { disabled, enabled };

    struct descriptor final {
        irq_state state = irq_state::disabled;
    };

    // ---------- Compile-time effect node ----------
    struct UnmaskOp final {
        unsigned irq;

        template <typename Environment>
        tay::expected<void, error> run(Environment& environment) const noexcept {
            return environment.unmask(irq);
        }
    };

    template <typename Operation>
    using Effect = tay::effect::program<Operation>;

    [[nodiscard]] auto unmask(const unsigned irq) {
        return Effect<UnmaskOp>{irq};
    }

    // ---------- Runtime-injected effect environment ----------
    struct IrqManager final {
        bool busy;

        tay::expected<void, error> unmask(const unsigned irq) noexcept {
            if (busy) {
                return tay::expected<void, error>(tay::unexpect, error::controller_busy);
            }
            std::printf("unmask irq %u\n", irq);
            return {};
        }
    };

    // ---------- Staged plan-perform-commit operation ----------
    struct HandlerEnable final : tay::staged::operation<HandlerEnable> {
        struct plan_type final {
            descriptor* target;
            unsigned irq;
            IrqManager* manager;
        };

        using receipt_type = void;
        using result_type  = void;
        using error_type   = error;

        tay::expected<plan_type, error> plan(descriptor& target, const unsigned irq,
                                             IrqManager& manager) const {
            return plan_type{&target, irq, &manager};
        }

        auto perform(const plan_type& plan) const {
            return unmask(plan.irq);
        }

        IrqManager& effect_environment(const plan_type& plan) const noexcept {
            return *plan.manager;
        }

        void commit(const plan_type& plan) const noexcept {
            plan.target->state = irq_state::enabled;
        }
    };

    constinit HandlerEnable enable_handler{};
}  // namespace

int main() {
    descriptor desc;
    IrqManager manager{false};

    // execute() runs the static effect program with manager, then commits.
    const auto enabled = enable_handler.execute(desc, 5U, manager);
    std::printf("descriptor: %s\n", desc.state == irq_state::enabled ? "enabled" : "disabled");
    return enabled ? 0 : 1;
}
