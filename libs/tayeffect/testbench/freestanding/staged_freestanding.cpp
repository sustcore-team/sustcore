#include <tay/staged.h>

namespace {
    enum class error { failed };
    struct update final : tay::staged::operation<update> {
        struct plan_type {
            int* target;
            int next;
        };
        using receipt_type = void;
        using result_type  = int;
        using error_type   = error;
        tay::expected<plan_type, error> plan(int& value) const {
            return plan_type{&value, value + 1};
        }
        tay::expected<void, error> perform(const plan_type&) const {
            return {};
        }
        int commit(const plan_type& plan) const noexcept {
            return *plan.target = plan.next;
        }
    };
    constinit update update_value{};
}  // namespace

extern "C" int tayeffect_staged_freestanding_contract() {
    int value   = 0;
    auto result = update_value.execute(value);
    return result ? *result : -1;
}
