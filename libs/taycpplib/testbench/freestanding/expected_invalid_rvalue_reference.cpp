#include <tay/expected.h>

tay::expected<int &&, int> invalid(tay::unexpect, 1);
