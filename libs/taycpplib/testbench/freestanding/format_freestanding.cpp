#include <tay/format.h>

#include <cstddef>

namespace {
    char output[128]{};
    std::size_t output_size = 0;

    struct boot_status {
        int code;
        bool ready;
    };

    int write_output(const char* data, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) {
            output[output_size++] = data[index];
        }
        return static_cast<int>(size);
    }
}  // namespace

namespace tay {
    template <>
    struct formatter<boot_status> {
        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const boot_status& status,
                                                FormatContext& context) const {
            context.write("boot_status{code=");
            context.template format<int>(status.code);
            context.write(", ready=");
            context.format(status.ready);
            context.put('}');
            return context.out();
        }
    };
}  // namespace tay

int main() {
    int value          = 42;
    boot_status status = {7, true};
    auto result        = tay::format_to<8>(
        write_output, "boot={} hex={:x} ptr={} status={}", true, value, &value,
        status);
    char iterator_output[8]{};
    auto iterator_result =
        tay::format_to_iter_s(iterator_output, sizeof(iterator_output),
                              "{}:{}", true, value);
    return result && *result == output_size && iterator_result ? 0 : 1;
}
