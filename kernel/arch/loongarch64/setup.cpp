/**
 * @file setup.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 内核早期入口接线
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/mem/pageman.h>
#include <arch/loongarch64/trait.h>
#include <env.h>
#include <logger.h>
#include <sus/logger.h>
#include <sustcore/boot.h>

#include <cstddef>

using namespace la64;

size_t hart_id;
void *dtb_ptr;
BootInfoHeader *bootinfo_ptr;

extern env::PaddedHartContext __hart_context[MAX_HARTS];
extern void env_setup();
extern "C" void c_setup_main(size_t boot_hart_id, BootInfoHeader *bootinfo);

namespace {
    void bind_current_hart(size_t boot_hart_id) {
        if (boot_hart_id >= MAX_HARTS) {
            panic("hart id 超出 MAX_HARTS");
        }

        hart_id      = boot_hart_id;
        env::hart_ctx = &__hart_context[boot_hart_id].ctx;
        env::hart_ctx->set_hart_id(boot_hart_id);
    }
}  // namespace

void EarlySerial::serial_write_char(char ch) {
    *reinterpret_cast<volatile unsigned char *>(0xFFFF'FFC0'1FE0'01E0ULL) =
        static_cast<unsigned char>(ch);
}

void EarlySerial::serial_write_string(size_t len, const char *str) {
    for (size_t i = 0; i < len; ++i) {
        serial_write_char(str[i]);
    }
}

extern "C" void c_setup_main(size_t boot_hart_id, BootInfoHeader *bootinfo) {
    // TODO: figure out how to get the correct hart id
    boot_hart_id = 0;
    bind_current_hart(boot_hart_id);
    bootinfo_ptr = bootinfo;

    loggers::SUSTCORE::INFO("进入 LoongArch64 内核 C 入口点! hart id: %d", boot_hart_id);
    env_setup();
    while (true) {
    }
}
