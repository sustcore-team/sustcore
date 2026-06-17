/**
 * @file setup.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISCV64启动程序
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/platform.h>
#include <arch/riscv64/trait.h>
#include <device/int.h>
#include <device/model.h>
#include <device/platform.h>
#include <logger.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <env.h>
#include <cstddef>

using namespace rv64;

size_t hart_id;
void *dtb_ptr;
BootInfoHeader *bootinfo_ptr;

void EarlySerial::serial_write_char(char ch) {
    sbi_dbcn_console_write_byte(ch);
}

void EarlySerial::serial_write_string(size_t len, const char *str) {
    sbi_dbcn_console_write(len, convert_pointer(str).as<char>());
}

extern void env_setup();

Result<void> Initialization::init_clock() {
    [[maybe_unused]] auto &device_model = device::DeviceModel::inst();
    [[maybe_unused]] auto &irqman       = device_model.interrupt();
    [[maybe_unused]] auto *ctx          = env::hart_ctx;
    assert(ctx != nullptr);

    loggers::SUSTCORE::INFO("初始化 hart Clint Timer%u",
                            static_cast<unsigned>(ctx->hart_id()));
    auto *platform = device_model.platform();
    if (platform == nullptr) {
        loggers::SUSTCORE::ERROR("全局 Platform 不可用!");
        unexpect_return(ErrCode::NULLPTR);
    }
    assert(platform->is<riscv::Riscv64Platform>());

    auto *clock_source =
        platform->as<riscv::Riscv64Platform>()->clock_source();
    if (clock_source == nullptr) {
        loggers::SUSTCORE::ERROR("全局 ClockSource 不可用!");
        unexpect_return(ErrCode::NULLPTR);
    }

    auto clock_virq = device_model.clock_virq();
    if (clock_virq == 0) {
        loggers::SUSTCORE::ERROR("DeviceModel 未提供有效 clock_virq");
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    ctx->alarm()       = new device::ClintAlarm(clock_source, clock_virq);
    ctx->time_keeper() = new device::TimeKeeper(clock_source, ctx->alarm());
    auto enable_timer_res = irqman.enable_irq(clock_virq);
    if (!enable_timer_res.has_value()) {
        loggers::SUSTCORE::ERROR("启用 CLINT timer 中断失败!");
        propagate_return(enable_timer_res);
    }

    loggers::SUSTCORE::INFO("hart %u 已初始化 ClintAlarm",
                            static_cast<unsigned>(ctx->hart_id()));
    void_return();
}

extern "C" void c_setup(void) {
    loggers::SUSTCORE::INFO("进入内核 C 入口点!");
    env_setup();
    while (true);
}

void Initialization::pre_init(void) {}

void Idle::idle()
{
    while(true);
}

void Initialization::post_init(void) 
{
}
