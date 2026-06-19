/**
 * @file serial.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 串口设备与驱动
 * @version alpha-1.0.0
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/resource.h>
#include <driver/base.h>
#include <driver/factory.h>
#include <driver/int/base.h>
#include <vfs/device.h>

#include <optional>
#include <string>
#include <vector>

namespace driver {
    /**
     * @brief 串口驱动
     */
    class SerialDevice final : public DriverBase {
    private:
        static constexpr std::string_view NS16550A_COMPATIBLE = "ns16550a";
        static constexpr std::string_view CLOCK_FREQUENCY_PROP = "clock-frequency";

    public:
        /**
         * @brief 销毁驱动.
         */
        ~SerialDevice() override = default;

        /**
         * @brief 获取 UART 输入时钟频率.
         *
         * @return units::frequency 时钟频率.
         */
        [[nodiscard]]
        units::frequency clock_frequency() const noexcept;

        /**
         * @brief 向 UART 发送一个字符.
         *
         * @param ch 待发送字符.
         */
        void writec(char ch) noexcept;

        /**
         * @brief 向 UART 连续发送字符串数据.
         *
         * @param str 待发送缓冲区.
         * @param len 缓冲区长度.
         */
        void write(const char *str, size_t len) noexcept;

        // 挂载到 devfs 上
        [[nodiscard]]
        Result<void> mount(CapIdx devdir) noexcept override;

    private:
        /**
         * @brief 构造一个串口驱动.
         *
         * @param res 统一串口设备资源.
         * @param clock_frequency UART 输入时钟频率.
         * @param base 已映射的 UART MMIO 基址.
         */
        SerialDevice(DevRes res, units::frequency clock_frequency,
                     char *base) noexcept;

        using uart_t = sus_u32;
        constexpr static size_t UART_RBR  = 0x00;
        constexpr static size_t UART_THR  = 0x00;
        constexpr static size_t UART_DLL  = 0x00;
        constexpr static size_t UART_DLH  = 0x04;
        constexpr static size_t UART_IER  = 0x04;
        constexpr static size_t UART_IIR  = 0x08;
        constexpr static size_t UART_FCR  = 0x08;
        constexpr static size_t UART_LCR  = 0x0C;
        constexpr static size_t UART_MCR  = 0x10;
        constexpr static size_t UART_LSR  = 0x14;
        constexpr static size_t UART_MSR  = 0x18;
        constexpr static size_t UART_SCH  = 0x1C;
        constexpr static size_t UART_USR  = 0x7C;
        constexpr static size_t UART_TFL  = 0x80;
        constexpr static size_t UART_RFL  = 0x84;
        constexpr static size_t UART_HALT = 0xA4;
        constexpr static size_t UART_REG_SIZE = 0xA8;

        units::frequency _clock_frequency = units::frequency::from_hz(0);
        
        
        struct UART
        {
            union {
                uart_t rbr;
                uart_t thr;
                uart_t dll;
            };
            union {
                uart_t dlh;
                uart_t ier;
            };
            union {
                uart_t iir;
                uart_t fcr;
            };
            uart_t lcr;
            uart_t mcr;
            uart_t lsr;
            uart_t msr;
            uart_t sch;
            char padding1[92];
            uart_t usr;
            uart_t tfl;
            uart_t rfl;
            char padding2[28];
            uart_t halt;
        };

        static_assert(offsetof(UART, rbr) == UART_RBR, "RBR offset mismatch!");
        static_assert(offsetof(UART, thr) == UART_THR, "THR offset mismatch!");
        static_assert(offsetof(UART, dll) == UART_DLL, "DLL offset mismatch!");
        static_assert(offsetof(UART, dlh) == UART_DLH, "DLH offset mismatch!");
        static_assert(offsetof(UART, ier) == UART_IER, "IER offset mismatch!");
        static_assert(offsetof(UART, iir) == UART_IIR, "IIR offset mismatch!");
        static_assert(offsetof(UART, fcr) == UART_FCR, "FCR offset mismatch!");
        static_assert(offsetof(UART, lcr) == UART_LCR, "LCR offset mismatch!");
        static_assert(offsetof(UART, mcr) == UART_MCR, "MCR offset mismatch!");
        static_assert(offsetof(UART, lsr) == UART_LSR, "LSR offset mismatch!");
        static_assert(offsetof(UART, msr) == UART_MSR, "MSR offset mismatch!");
        static_assert(offsetof(UART, sch) == UART_SCH, "SCH offset mismatch!");
        static_assert(offsetof(UART, usr) == UART_USR, "USR offset mismatch!");
        static_assert(offsetof(UART, tfl) == UART_TFL, "TFL offset mismatch!");
        static_assert(offsetof(UART, rfl) == UART_RFL, "RFL offset mismatch!");
        static_assert(offsetof(UART, halt) == UART_HALT, "HALT offset mismatch!");
        static_assert(sizeof(UART) == UART_REG_SIZE,
                      "UART struct size mismatch!");

        volatile UART *_base;

        friend class SerialDeviceFactory;
    };

    /**
     * @brief FDT 串口普通设备工厂.
     */
    class SerialDeviceFactory final : public IDeviceFactory {
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
