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
#include <arch/loongarch64/device/clock.h>
#include <arch/loongarch64/device/platform.h>
#include <arch/loongarch64/trait.h>
#include <device/int.h>
#include <device/model.h>
#include <device/platform.h>
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
    bind_current_hart(boot_hart_id);
    bootinfo_ptr = bootinfo;

    loggers::SUSTCORE::INFO("进入 LoongArch64 内核 C 入口点! hart id: %d", boot_hart_id);
    env_setup();
    while (true) {
    }
}

void Initialization::pre_init(void) {}

void Initialization::post_init(void) {}

Result<void> Initialization::init_clock() {
    auto &device_model = device::DeviceModel::inst();
    auto &irqman       = device_model.interrupt();
    auto *ctx          = env::hart_ctx;
    assert(ctx != nullptr);

    auto *platform = device_model.platform();
    if (platform == nullptr) {
        loggers::SUSTCORE::ERROR("全局 Platform 不可用!");
        unexpect_return(ErrCode::NULLPTR);
    }
    assert(platform->is<la64::LoongArch64Platform>());

    auto *clock_source =
        platform->as<la64::LoongArch64Platform>()->clock_source();
    if (clock_source == nullptr) {
        loggers::SUSTCORE::ERROR("全局 ClockSource 不可用!");
        unexpect_return(ErrCode::NULLPTR);
    }

    auto clock_virq = device_model.clock_virq();
    if (clock_virq == 0) {
        loggers::SUSTCORE::ERROR("DeviceModel 未提供有效 clock_virq");
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    ctx->alarm()       = new la64::CSRTimer(clock_source, clock_virq);
    ctx->time_keeper() = new device::TimeKeeper(clock_source, ctx->alarm());
    auto enable_timer_res = irqman.enable_irq(clock_virq);
    if (!enable_timer_res.has_value()) {
        loggers::SUSTCORE::ERROR("启用 LoongArch timer 中断失败!");
        propagate_return(enable_timer_res);
    }

    loggers::SUSTCORE::INFO("hart %u 已初始化 CSRTimer",
                            static_cast<unsigned>(ctx->hart_id()));
    void_return();
}

void Idle::idle() {
    while (true) {
    }
}
