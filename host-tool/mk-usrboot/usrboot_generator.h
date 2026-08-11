/**
 * @file usrboot_generator.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 根据通用生成配置发布 Usrboot 文件的接口
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <string_view>

#include "genconf.h"

namespace mku {
    /**
     * @brief 根据配置从输入视图提取段内容并原子发布 Usrboot 文件。
     * @param input 调用期间有效的非拥有输入字节视图。
     * @param configuration 不拥有输入内容的生成参数。
     * @param output_path 最终输出路径；失败时不会替换已有文件。
     */
    [[nodiscard]] result<void> generate_usrboot(byte_view input,
                                                const GeneratorConfiguration &configuration,
                                                std::string_view output_path) noexcept;
}  // namespace mku
