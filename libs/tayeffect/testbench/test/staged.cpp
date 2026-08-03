#include <tay/staged.h>

#include <cassert>
#include <utility>

namespace {
    enum class error { plan, perform };

    struct move_only_receipt {
        int value;
        explicit move_only_receipt(int initial) : value(initial) {}
        move_only_receipt(const move_only_receipt&)            = delete;
        move_only_receipt& operator=(const move_only_receipt&) = delete;
        move_only_receipt(move_only_receipt&&)                 = default;
        move_only_receipt& operator=(move_only_receipt&&)      = default;
    };

    struct receipt_result_operation final : tay::staged::operation<receipt_result_operation> {
        struct plan_type {
            int* target;
            int next;
            bool perform_fails;
        };
        using receipt_type = move_only_receipt;
        using result_type  = int;
        using error_type   = error;

        tay::expected<plan_type, error> plan(int& target, int delta, bool plan_fails,
                                             bool perform_fails) const {
            if (plan_fails) {
                return tay::expected<plan_type, error>(tay::unexpect, error::plan);
            }
            return plan_type{&target, target + delta, perform_fails};
        }
        tay::expected<receipt_type, error> perform(const plan_type& plan) const {
            if (plan.perform_fails) {
                return tay::expected<receipt_type, error>(tay::unexpect, error::perform);
            }
            return receipt_type{plan.next};
        }
        int commit(const plan_type& plan, receipt_type&& receipt) const noexcept {
            *plan.target = receipt.value;
            return *plan.target;
        }
    };

    struct void_void_operation final : tay::staged::operation<void_void_operation> {
        using plan_type    = int*;
        using receipt_type = void;
        using result_type  = void;
        using error_type   = error;
        tay::expected<plan_type, error> plan(int& value) const {
            return &value;
        }
        tay::expected<void, error> perform(const plan_type&) const {
            return {};
        }
        void commit(const plan_type& value) const noexcept {
            ++*value;
        }
    };

    struct void_result_operation final : tay::staged::operation<void_result_operation> {
        using plan_type    = int;
        using receipt_type = void;
        using result_type  = int;
        using error_type   = error;
        tay::expected<int, error> plan(int value) const {
            return value;
        }
        tay::expected<void, error> perform(const int&) const {
            return {};
        }
        int commit(const int& value) const noexcept {
            return value + 1;
        }
    };

    struct receipt_void_operation final : tay::staged::operation<receipt_void_operation> {
        using plan_type    = int*;
        using receipt_type = move_only_receipt;
        using result_type  = void;
        using error_type   = error;
        tay::expected<plan_type, error> plan(int& value) const {
            return &value;
        }
        tay::expected<receipt_type, error> perform(const plan_type&) const {
            return receipt_type{8};
        }
        void commit(const plan_type& value, receipt_type&& receipt) const noexcept {
            *value = receipt.value;
        }
    };

    constinit receipt_result_operation receipt_result{};
    constinit void_void_operation void_void{};
    constinit void_result_operation void_result{};
    constinit receipt_void_operation receipt_void{};
}  // namespace

int main() {
    int value                  = 10;
    const auto planned_failure = receipt_result.execute(value, 2, true, false);
    assert(!planned_failure && value == 10);
    const auto performed_failure = receipt_result.execute(value, 2, false, true);
    assert(!performed_failure && value == 10);
    const auto success = receipt_result.execute(value, 2, false, false);
    assert(success && *success == 12 && value == 12);

    int void_value = 0;
    assert(void_void.execute(void_value));
    assert(void_value == 1);
    const auto scalar = void_result.execute(6);
    assert(scalar && *scalar == 7);
    assert(receipt_void.execute(void_value));
    assert(void_value == 8);
}
