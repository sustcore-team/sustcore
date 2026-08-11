/**
 * @file cmdline.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief mk-usrboot 命令行选项、错误类型与解析接口
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/expected.h>

#include <string_view>

namespace mku {
    /** @brief 命令行解析失败的分类。 */
    enum class CommandLineErrorCode {
        UNKNOWN_OPTION,
        MISSING_OUTPUT_PATH,
        DUPLICATE_OUTPUT,
        MULTIPLE_INPUTS,
        MISSING_INPUT,
        MISSING_OUTPUT,
    };

    /** @brief 命令行错误及其关联的非拥有参数视图。 */
    struct CommandLineError {
        CommandLineErrorCode code;
        std::string_view argument;
    };

    /** @brief 解析后的输入、输出路径和帮助选项。路径视图借用自 argv。 */
    struct CommandLineOptions {
        std::string_view input;
        std::string_view output;
        bool show_help = false;
    };

    /**
     * @brief 解析 mk-usrboot 命令行。
     * @return 成功时返回借用 argv 字符串的选项，失败时返回错误分类与关联参数。
     */
    [[nodiscard]] tay::expected<CommandLineOptions, CommandLineError> parse_command_line(
        int argc, const char *const *argv) noexcept;

    /** @brief 向标准输出打印命令行用法。 */
    void print_usage();
}  // namespace mku
