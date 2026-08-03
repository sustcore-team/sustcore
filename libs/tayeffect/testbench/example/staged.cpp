#include <tay/staged.h>

#include <cstdio>

namespace {
    enum class error { overflow };

    struct counter_update final : tay::staged::operation<counter_update> {
        struct plan_type {
            int* destination;
            int next;
        };
        using receipt_type = void;
        using result_type  = int;
        using error_type   = error;

        tay::expected<plan_type, error> plan(int& current, int delta) const {
            if (delta > 0 && current > 100 - delta) {
                return tay::expected<plan_type, error>(tay::unexpect, error::overflow);
            }
            return plan_type{&current, current + delta};
        }

        tay::expected<void, error> perform(const plan_type&) const {
            return {};
        }

        int commit(const plan_type& plan) const noexcept {
            *plan.destination = plan.next;
            return plan.next;
        }
    };

    constinit counter_update update_counter{};
}  // namespace

int main() {
    int counter        = 7;
    const auto updated = update_counter.execute(counter, 5);
    if (!updated)
        return 1;
    std::printf("counter: %d\n", *updated);
    return 0;
}
