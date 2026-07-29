#include <tay/format.h>

int write_output(const char *, std::size_t);

void invalid_format() {
    (void)tay::format_to<0>(write_output, "text");
}
