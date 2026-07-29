#include <tay/format.h>

struct custom_value {};

namespace tay {
    template <>
    struct formatter<custom_value> {
        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            auto position = context.begin();
            if (position != context.end() && *position == 'x') {
                ++position;
            }
            return position;
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const custom_value&,
                                                FormatContext& context) const {
            return context.out();
        }
    };
}  // namespace tay

int write_output(const char*, std::size_t);

void invalid_format() {
    (void)tay::format_to(write_output, "{:xy}", custom_value{});
}
