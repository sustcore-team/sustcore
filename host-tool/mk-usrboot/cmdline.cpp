/**
 * @file cmdline.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief mk-usrboot 命令行语法解析与用法输出
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include "cmdline.h"

#include <cstdio>

namespace mku {
    namespace {
        [[nodiscard]] tay::unexpected<CommandLineError> error(
            CommandLineErrorCode code, std::string_view argument = {}) noexcept {
            return tay::unexpected<CommandLineError>(CommandLineError{code, argument});
        }
    }  // namespace

    void print_usage() {
        std::fputs("usage: mk-usrboot <input> -o <output>\n", stdout);
    }

    tay::expected<CommandLineOptions, CommandLineError> parse_command_line(
        int argc, const char *const *argv) noexcept {
        CommandLineOptions options;
        bool positional_only = false;
        bool has_input       = false;
        bool has_output      = false;

        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (!positional_only && argument == "--") {
                positional_only = true;
                continue;
            }
            if (!positional_only && (argument == "-h" || argument == "--help")) {
                options.show_help = true;
                return options;
            }
            if (!positional_only && argument == "-o") {
                if (has_output) {
                    return error(CommandLineErrorCode::DUPLICATE_OUTPUT, argument);
                }
                if (++index >= argc) {
                    return error(CommandLineErrorCode::MISSING_OUTPUT_PATH, argument);
                }
                options.output = argv[index];
                has_output     = true;
                continue;
            }
            if (!positional_only && !argument.empty() && argument.front() == '-') {
                return error(CommandLineErrorCode::UNKNOWN_OPTION, argument);
            }
            if (has_input) {
                return error(CommandLineErrorCode::MULTIPLE_INPUTS, argument);
            }
            options.input = argument;
            has_input     = true;
        }

        if (!has_input || options.input.empty()) {
            return error(CommandLineErrorCode::MISSING_INPUT);
        }
        if (!has_output || options.output.empty()) {
            return error(CommandLineErrorCode::MISSING_OUTPUT);
        }
        return options;
    }
}  // namespace mku
