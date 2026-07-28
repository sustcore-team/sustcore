#include <tay/expected.h>

int main() {
    tay::expected<int, int> failed = tay::Err(7);
    return failed.value();
}
