#include <tay/staged.h>

struct invalid_operation final : tay::staged::operation<invalid_operation> {
    using plan_type    = int;
    using receipt_type = void;
    using result_type  = void;
    using error_type   = int;
    tay::expected<int, int> plan() const {
        return 1;
    }
    tay::expected<void, int> perform(const int &) const {
        return {};
    }
    void commit(const int &) const {}
};

void invalid_staged_commit() {
    invalid_operation operation;
    (void)operation.execute();
}
