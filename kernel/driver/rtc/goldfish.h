/**
 * @file goldfish.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief goldfish RTC 设备驱动头文件
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/base.h>
#include <driver/factory.h>
#include <sus/types.h>
#include <sus/units.h>

#include <functional>
#include <string_view>

namespace driver {
    /**
     * @brief 串口驱动
     */
    class GoldfishRTC final : public DriverBase {
    private:
        static constexpr std::string_view GOLDFISH_RTC_COMPATIBLE =
            "google,goldfish-rtc";

    public:
        using AlarmHandler = std::function<void(units::time)>;

        /**
         * @brief 销毁驱动.
         */
        ~GoldfishRTC() override = default;

        [[nodiscard]]
        units::time read_time() const noexcept;

        /**
         * @brief 设置 RTC 闹钟与到期处理函数.
         *
         * @param when 闹钟触发时间.
         * @param handler 到期时调用的处理函数.
         */
        void set_alarm(units::time when, AlarmHandler handler) noexcept;

    private:
        /**
         * @brief 构造一个串口驱动.
         *
         * @param node 统一串口设备节点引用指针.
         * @param name 设备名称.
         * @param virq RTC virq
         */
        GoldfishRTC(DevRes res, char *base, virq_t virq) noexcept;

        using rtc_t                                   = sus_u32;
        constexpr static size_t RTC_TIME_LOW          = 0x00;
        constexpr static size_t RTC_TIME_HIGH         = 0x04;
        constexpr static size_t RTC_ALARM_LOW         = 0x08;
        constexpr static size_t RTC_ALARM_HIGH        = 0x0C;
        constexpr static size_t RTC_IRQ_ENABLE        = 0x10;
        constexpr static size_t RTC_CLEAR_ALARM       = 0x14;
        constexpr static size_t TIMER_ALARM_STATUS    = 0x18;
        constexpr static size_t TIMER_CLEAR_INTERRUPT = 0x1C;
        constexpr static size_t RTC_REG_SIZE          = 0x20;

        struct Goldfish {
            rtc_t time_low;
            rtc_t time_high;
            rtc_t alarm_low;
            rtc_t alarm_high;
            rtc_t irq_enable;
            rtc_t clear_alarm;
            rtc_t alarm_status;
            rtc_t clear_interrupt;
        };

        static_assert(sizeof(Goldfish) == RTC_REG_SIZE,
                      "GoldfishRTC register struct size mismatch!");
        static_assert(offsetof(Goldfish, time_low) == RTC_TIME_LOW,
                      "time_low offset mismatch!");
        static_assert(offsetof(Goldfish, time_high) == RTC_TIME_HIGH,
                      "time_high offset mismatch!");
        static_assert(offsetof(Goldfish, alarm_low) == RTC_ALARM_LOW,
                      "alarm_low offset mismatch!");
        static_assert(offsetof(Goldfish, alarm_high) == RTC_ALARM_HIGH,
                      "alarm_high offset mismatch!");
        static_assert(offsetof(Goldfish, irq_enable) == RTC_IRQ_ENABLE,
                      "irq_enable offset mismatch!");
        static_assert(offsetof(Goldfish, clear_alarm) == RTC_CLEAR_ALARM,
                      "clear_alarm offset mismatch!");
        static_assert(offsetof(Goldfish, alarm_status) == TIMER_ALARM_STATUS,
                      "alarm_status offset mismatch!");
        static_assert(offsetof(Goldfish, clear_interrupt) == TIMER_CLEAR_INTERRUPT,
                      "clear_interrupt offset mismatch!");
        static_assert(sizeof(rtc_t) == 4, "rtc_t size mismatch!");
        static_assert(sizeof(Goldfish) == RTC_REG_SIZE,
                      "GoldfishRTC register struct size mismatch!");

        volatile Goldfish *regs = nullptr;
        AlarmHandler _alarm_handler;
        virq_t _virq = 0;

        /**
         * @brief 处理 RTC 闹钟中断.
         *
         * @param event RTC 对应的中断事件.
         */
        void handle_alarm_irq(const IrqEvent &event) noexcept;

        friend class GoldfishRTCFactory;
    };

    /**
     * @brief FDT 串口普通设备工厂.
     */
    class GoldfishRTCFactory final : public IDeviceFactory {
    public:
        [[nodiscard]]
        const DeviceId &device_id() const noexcept override;

        /**
         * @brief 基于统一设备节点创建串口设备包装对象.
         *
         * @param node 统一设备节点.
         * @param model 设备模型.
         * @return Result<DriverBase*> 创建结果.
         */
        [[nodiscard]]
        Result<DriverBase *> create(const device::DeviceNode &node,
                                    device::DeviceModel &model,
                                    b64 driver_flag) const override;
    };
}  // namespace driver
