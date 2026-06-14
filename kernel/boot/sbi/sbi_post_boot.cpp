/**
 * @file sbi_post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI 启动代码第二部分
 * @version alpha-1.0.0
 * @date 2026-06-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/sbi/sbi_paging.h>
#include <sbi/sbi.h>

#include <cstring>

#define _SBI_FUNCTION  SECTION(".sbi_post_boot.text")
#define _SBI_DATA      SECTION(".sbi_post_boot.data")
#define _SBI_STRING(x) _SBI_DATA constexpr const char x[]

namespace sbi {
    _SBI_STRING(SBI_POST_BOOT_MSG) = "SBI引导程序第二部分启动!\n";
    _SBI_STRING(SBI_KERNEL_ENTRY_MSG) = "SBI引导程序进入内核入口!\n";

    extern "C" void _start(size_t hart_id, addr_t dtb_ptr);

    _SBI_FUNCTION void sbi_writes(const char *str) {
        int len = strlen(str);
        for (int i = 0; i < len; i++) {
            sbi_dbcn_console_write_byte(str[i]);
        }
    }

    extern "C" _SBI_FUNCTION void _sbi_post_start(size_t hart_id,
                                                  addr_t dtb_ptr) {
        sbi_writes(SBI_POST_BOOT_MSG);
        sbi_writes(SBI_KERNEL_ENTRY_MSG);
        _start(hart_id, dtb_ptr);
        while (true);
    }
}  // namespace sbi
