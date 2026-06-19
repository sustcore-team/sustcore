/**
 * @file goldfish.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief goldfish RTC 设备驱动
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/model.h>
#include <driver/rtc/goldfish.h>
#include <logger.h>
#include <sustcore/errcode.h>

namespace driver {
    namespace {
        constexpr FDTDeviceId GOLDFISH_RTC_FDT_IDS[] = {
            {.compatible = "google,goldfish-rtc", .driver_flag = 0},
            {.compatible = nullptr, .driver_flag = 0},
        };
        constexpr DeviceId GOLDFISH_RTC_DEVICE_ID = {
            .fdt_ids = GOLDFISH_RTC_FDT_IDS,
            .pci_ids = nullptr,
        };
    }  // namespace

    GoldfishRTC::GoldfishRTC(DevRes res, char *base, virq_t virq) noexcept
        : DriverBase(std::move(res)),
          regs(reinterpret_cast<volatile Goldfish *>(base)),
          _alarm_handler(),
          _virq(virq)
    {
        // res shouldn't be used after this point cuz it has been moved to the base class
        assert (_mmios.size() >= 1);
        assert (_virqs.size() >= 1);

        // to flush the rtc registers
        [[maybe_unused]] auto t = read_time();

        auto register_res =
            _virqs[0]->register_handler(this_call(this,
                                                  &GoldfishRTC::handle_alarm_irq));
        assert (register_res.has_value());
    }

    [[nodiscard]]
    units::time GoldfishRTC::read_time() const noexcept {
        sus_u64 time = (static_cast<sus_u64>(regs->time_high) << 32) | regs->time_low;
        return units::time::from_nanoseconds(time);
    }

    /**
     * @brief 设置 RTC 闹钟与到期处理函数.
     *
     * @param when 闹钟触发时间.
     * @param handler 到期时调用的处理函数.
     */
    void GoldfishRTC::set_alarm(units::time when, AlarmHandler handler) noexcept {
        _alarm_handler = std::move(handler);

        const auto formatted_alarm =
            units::rt_time::from_time(when).to_formatted_time();
        loggers::SUSTCORE::INFO("设置闹钟时间: %04u-%02u-%02u %02u:%02u:%02u",
                                formatted_alarm.year, formatted_alarm.month,
                                formatted_alarm.day, formatted_alarm.hour,
                                formatted_alarm.minute, formatted_alarm.second);

        sus_u64 nanos   = when.to_nanoseconds();
        sus_u32 nanos_hi = static_cast<sus_u32>(nanos >> 32);
        sus_u32 nanos_lo = static_cast<sus_u32>(nanos & 0xFFFFFFFF);

        regs->irq_enable      = 0x0;
        regs->clear_interrupt = 0x1;
        regs->alarm_high      = nanos_hi;
        regs->alarm_low       = nanos_lo;
        regs->irq_enable      = 0x1;

        auto enable_res = _virqs[0]->enable();
        if (!enable_res.has_value()) {
            loggers::SUSTCORE::ERROR("Goldfish RTC 启用 alarm virq 失败: %s",
                                     to_cstring(enable_res.error()));
            return;
        }

        loggers::SUSTCORE::DEBUG("Goldfish RTC alarm 已设置: virq=%llu status=0x%08x",
                                 static_cast<unsigned long long>(_virq),
                                 regs->alarm_status);
    }

    /**
     * @brief 处理 RTC 闹钟中断.
     *
     * @param event RTC 对应的中断事件.
     */
    void GoldfishRTC::handle_alarm_irq(const IrqEvent &event) noexcept {
        (void)event;
        auto now = read_time();
        regs->clear_interrupt = 0x1;
        if (_alarm_handler) {
            _alarm_handler(now);
        } else {
            loggers::SUSTCORE::DEBUG("Goldfish RTC 收到 alarm 中断但未设置 handler");
        }
        Result<void> ack_res = device::DeviceModel::inst().interrupt().ack(event);
        if (!ack_res.has_value()) {
            loggers::SUSTCORE::ERROR("Goldfish RTC 中断确认失败: %s",
                                     to_cstring(ack_res.error()));
        }
    }

    /**
     * @brief 获取该工厂支持的主 compatible.
     *
     * @return std::string_view compatible 字符串.
     */
    const DeviceId &GoldfishRTCFactory::device_id() const noexcept {
        return GOLDFISH_RTC_DEVICE_ID;
    }

    /**
     * @brief 基于统一设备节点创建串口设备包装对象.
     *
     * @param node 统一设备节点.
     * @param model 设备模型.
     * @return Result<DriverBase*> 创建结果.
     */
    [[nodiscard]]
    Result<DriverBase *> GoldfishRTCFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        b64 driver_flag) const {
        (void)model;
        (void)driver_flag;
        loggers::DEVICE::INFO("开始创建设备驱动: goldfish-rtc name=%s",
                              node.name());
        // 获取 mmio & virqs
        auto virqs = device::DevResManager::get_virq_resource(node);
        auto mmios = device::DevResManager::get_mmio_resource(node);

        // 以第一个 virq 作为 RTC virq
        if (virqs.empty())
        {
            loggers::DEVICE::ERROR("Goldfish RTC 设备缺失 VIRQ 资源!");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        virq_t virq = virqs[0]->virq();

        // 以第一个 mmio 作为 RTC 寄存器基址
        if (mmios.empty()) {
            loggers::DEVICE::ERROR("Goldfish RTC 设备缺少 MMIO 资源!");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        // 校验 mmio
        auto *mmio = mmios[0].get();
        assert(mmio != nullptr);
        assert(mmio->region().size() >= GoldfishRTC::RTC_REG_SIZE);

        // 映射
        auto map_res = device::MMIOManager::inst().map_to_kernel(*mmio);
        if (!map_res.has_value()) {
            loggers::DEVICE::ERROR("Goldfish RTC MMIO 资源映射失败!");
            unexpect_return(map_res.error());
        }
        char *base = map_res.value().as<char>();

        return new GoldfishRTC(
            DriverBase::DevRes(node, std::move(virqs), std::move(mmios)),
            base, virq);
    }
}  // namespace driver
