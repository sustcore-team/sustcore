/**
 * @file genconf.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ELF Parser 与 Usrboot Generator 之间的无格式耦合中间模型
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/array.h>
#include <tay/expected.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mku {
    /** @brief mk-usrboot 各处理阶段共享的失败分类。 */
    enum class ErrorCode {
        IO,
        INVALID_ELF,
        INVALID_CONFIGURATION,
        OUTPUT,
    };

    /** @brief 共享错误信息。message 是非拥有视图。 */
    struct Error {
        ErrorCode code;
        std::string_view message;
        int system_error = 0;
    };

    template <class Value>
    using result = tay::expected<Value, Error>;

    using ub_addr64 = std::uint64_t;
    using ub_off64  = std::uint64_t;
    using ub_sz64   = std::uint64_t;

    /** @brief 输入文件的非拥有只读字节视图。 */
    using byte_view = tay::array_view<const std::byte>;

    /** @brief 与具体目标格式无关的段访问权限。 */
    struct SegmentPermissions {
        bool readable   = false;
        bool writable   = false;
        bool executable = false;
    };

    /** @brief Generator 所需的单个可加载段描述。 */
    struct GeneratorSegment {
        SegmentPermissions permissions;
        ub_addr64 virtual_address = 0;
        ub_off64 file_offset      = 0;
        ub_sz64 file_size         = 0;
        ub_sz64 memory_size       = 0;
    };

    /**
     * @brief 从输入格式提取的生成参数。
     *
     * 配置只保存入口和三个段的元数据，不拥有或引用输入文件内容。
     */
    struct GeneratorConfiguration {
        ub_addr64 entry = 0;
        tay::static_array<GeneratorSegment, 3> segments{};
    };
}  // namespace mku
