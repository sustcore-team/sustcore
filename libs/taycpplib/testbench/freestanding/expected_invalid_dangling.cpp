#include <tay/expected.h>

tay::expected<int &, int> invalid = tay::Ok(42);
