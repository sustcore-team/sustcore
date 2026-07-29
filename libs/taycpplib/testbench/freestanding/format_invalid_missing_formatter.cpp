#include <tay/format.h>

struct unsupported {};

int write_output(const char *, std::size_t);

void invalid_format() {
    (void)tay::format_to(write_output, "{}", unsupported{});
}
