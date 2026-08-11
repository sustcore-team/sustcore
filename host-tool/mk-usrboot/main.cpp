/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief mk-usrboot Host 工具的输入读取、流程编排与错误报告
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <sys/stat.h>
#include <tay/array_list.h>
#include <tay/string.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "cmdline.h"
#include "elf_parser.h"
#include "usrboot_generator.h"

namespace mku {
    namespace {
        class input_file {
        public:
            input_file(const input_file &)            = delete;
            input_file &operator=(const input_file &) = delete;

            explicit input_file(FILE *stream) noexcept : stream_(stream) {}

            ~input_file() {
                if (stream_ != nullptr) {
                    std::fclose(stream_);
                }
            }

            [[nodiscard]] FILE *get() const noexcept {
                return stream_;
            }

        private:
            FILE *stream_;
        };

        using byte_buffer = tay::array_list<std::byte>;

        void print_view(FILE *stream, std::string_view value) {
            if (!value.empty()) {
                std::fwrite(value.data(), 1, value.size(), stream);
            }
        }

        [[nodiscard]] result<byte_buffer> read_input(std::string_view path) noexcept {
            auto path_result = tay::string<>::try_create(path.data(), path.size());
            if (!path_result) {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "input path allocation failed"});
            }
            FILE *stream = std::fopen(path_result->data(), "rb");
            if (stream == nullptr) {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "cannot open input", errno});
            }
            input_file input(stream);
            struct stat status;
            if (fstat(fileno(input.get()), &status) != 0) {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "cannot stat input", errno});
            }
            if (status.st_size < 0 || static_cast<std::uintmax_t>(status.st_size) > SIZE_MAX) {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "input is too large"});
            }

            auto buffer_result = byte_buffer::try_create(static_cast<std::size_t>(status.st_size));
            if (!buffer_result) {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "input allocation failed"});
            }
            auto buffer = std::move(*buffer_result);
            if (buffer.size() != 0 &&
                std::fread(buffer.data(), 1, buffer.size(), input.get()) != buffer.size())
            {
                return tay::unexpected<Error>(Error{ErrorCode::IO, "cannot read input", errno});
            }
            return buffer;
        }

        void print_error(std::string_view stage, const Error &error) {
            std::fputs("mk-usrboot: ", stderr);
            print_view(stderr, stage);
            std::fputs(": ", stderr);
            print_view(stderr, error.message);
            if (error.system_error != 0) {
                std::fputs(": ", stderr);
                std::fputs(std::strerror(error.system_error), stderr);
            }
            std::fputc('\n', stderr);
        }

        void print_command_line_error(const CommandLineError &error) {
            std::fputs("mk-usrboot: ", stderr);
            switch (error.code) {
                case CommandLineErrorCode::UNKNOWN_OPTION:
                    std::fputs("unknown option: ", stderr);
                    print_view(stderr, error.argument);
                    break;
                case CommandLineErrorCode::MISSING_OUTPUT_PATH:
                    std::fputs("option -o requires an output path", stderr);
                    break;
                case CommandLineErrorCode::DUPLICATE_OUTPUT:
                    std::fputs("option -o may only be specified once", stderr);
                    break;
                case CommandLineErrorCode::MULTIPLE_INPUTS:
                    std::fputs("only one input may be specified: ", stderr);
                    print_view(stderr, error.argument);
                    break;
                case CommandLineErrorCode::MISSING_INPUT:
                    std::fputs("missing input", stderr);
                    break;
                case CommandLineErrorCode::MISSING_OUTPUT:
                    std::fputs("missing output", stderr);
                    break;
            }
            std::fputc('\n', stderr);
        }
    }  // namespace
}  // namespace mku

int main(int argc, char **argv) {
    using namespace mku;

    auto options_result = parse_command_line(argc, argv);
    if (!options_result) {
        print_command_line_error(options_result.error());
        print_usage();
        return 2;
    }
    const auto &options = *options_result;
    if (options.show_help) {
        print_usage();
        return 0;
    }

    auto input_result = read_input(options.input);
    if (!input_result) {
        print_error("input", input_result.error());
        return 1;
    }
    auto input = std::move(*input_result);
    const byte_view input_view{input.data(), input.size()};

    auto configuration_result = parse_elf(input_view);
    if (!configuration_result) {
        print_error("ELF", configuration_result.error());
        return 1;
    }
    auto output_result = generate_usrboot(input_view, *configuration_result, options.output);
    if (!output_result) {
        print_error("output", output_result.error());
        return 1;
    }
    return 0;
}
