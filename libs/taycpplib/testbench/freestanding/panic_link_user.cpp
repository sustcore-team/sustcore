#include <tay/expected.h>

extern "C" int main() {
    tay::expected<int, int> failed = tay::Err(1);
    return failed.value();
}
