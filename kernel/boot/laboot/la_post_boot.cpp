/**
 * @file la_post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief laboot 第二阶段
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/laboot/la_paging.h>
#include <sustcore/boot.h>

#include <cstddef>
#include <cstdint>

#define _LABOOT_POST_FUNCTION  SECTION(".laboot_post.text")
#define _LABOOT_POST_DATA      SECTION(".laboot_post.data")
#define _LABOOT_POST_RODATA    SECTION(".laboot_post.rodata")
#define _LABOOT_POST_STRING(x) _LABOOT_POST_RODATA constexpr const char x[]

namespace laboot::post {
    _LABOOT_POST_DATA volatile uint8_t *SERIAL_BASE =
        reinterpret_cast<volatile uint8_t *>(0x1fe001e0ULL);

    _LABOOT_POST_FUNCTION void serial_putc(char ch) {
        while ((SERIAL_BASE[5] & 0x20) == 0) {
        }
        SERIAL_BASE[0] = static_cast<uint8_t>(ch);
    }

    _LABOOT_POST_FUNCTION void serial_puts(const char *str) {
        for (const char *p = str; *p != '\0'; ++p) {
            serial_putc(*p);
        }
    }
}  // namespace laboot::post

namespace laboot::msg::post {
    _LABOOT_POST_STRING(POST_MSG) = "LABOOT启动第二阶段!\n";
}

namespace laboot {
    using namespace post;
    using namespace msg::post;

    extern "C" _LABOOT_POST_FUNCTION [[noreturn]]
    void _laboot_post_start() {
        serial_puts(POST_MSG);
        while (true) {
        }
    }
}  // namespace laboot
