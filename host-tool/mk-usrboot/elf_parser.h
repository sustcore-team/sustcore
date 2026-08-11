/**
 * @file elf_parser.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ELF64 输入校验与通用生成配置解析接口
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "genconf.h"

namespace mku {
    /**
     * @brief 校验 ELF64 输入并提取格式无关的生成配置。
     * @param input 调用期间有效的非拥有输入字节视图。
     */
    [[nodiscard]] result<GeneratorConfiguration> parse_elf(byte_view input) noexcept;
}  // namespace mku
