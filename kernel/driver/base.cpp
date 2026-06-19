/**
 * @file base.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 驱动基类
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/resource.h>
#include <driver/base.h>
#include <logger.h>
#include <device/fdt/device_node.h>

namespace driver {
    /**
     * @brief 从统一设备节点读取 UART 输入时钟.
     *
     * @param node 统一设备节点.
     * @return Result<sus_u32> 读取结果.
     */
    [[nodiscard]]
    Result<sus_u64> DriverBase::__load_integral(const device::DeviceNode &node,
                                                std::string_view prop_name,
                                                size_t integral_sz) noexcept {
        if (integral_sz > sizeof(sus_u64)) {
            loggers::DEVICE::WARN(
                "要求的整数值大小过大: %d 大于 sizeof(sus_u64) = %d!",
                integral_sz, sizeof(sus_u64));
            integral_sz = sizeof(sus_u64);
        }

        auto *fdt_node = node.as<fdt::FDTDeviceNode>();
        if (fdt_node == nullptr) {
            loggers::DEVICE::ERROR("节点 %s 不支持 FDT 属性访问",
                                   node.name());
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto prop_res = fdt_node->property(prop_name);
        if (!prop_res.has_value()) {
            loggers::DEVICE::ERROR("串口节点缺少 %s 属性", prop_name.data());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const auto &prop = prop_res.value();
        if (prop.type() != fdt::DevicePropView::PropType::INTEGER) {
            loggers::DEVICE::ERROR("串口节点 %s 类型非法", prop_name.data());
            unexpect_return(ErrCode::INVALID_PROPERTY_TYPE);
        }
        return prop.as_integer(integral_sz);
    }
    /**
     * @brief 驱动对象基本类
     *
     * @param node 设备节点非拥有指针.
     */
    DriverBase::DriverBase(DevRes res) noexcept
        : _node(res.node),
          _virqs(std::move(res.virqs)),
          _mmios(std::move(res.mmios)) {}

    DriverBase::~DriverBase() noexcept {
        for (auto &virq_resource : _virqs) {
            if (virq_resource == nullptr || !virq_resource->registered()) {
                continue;
            }
            auto unregister_res = virq_resource->unregister_handler();
            if (!unregister_res.has_value()) {
                loggers::INTERRUPT::ERROR(
                    "注销父域 handler 失败: virq=%llu err=%s",
                    static_cast<unsigned long long>(virq_resource->virq()),
                    to_cstring(unregister_res.error()));
            } else {
                loggers::INTERRUPT::DEBUG(
                    "已注销父域 handler: virq=%llu",
                    static_cast<unsigned long long>(virq_resource->virq()));
            }
        }

        for (auto &mmio_resource : _mmios) {
            if (mmio_resource == nullptr || !mmio_resource->mapped()) {
                continue;
            }
            auto unmap_res =
                device::MMIOManager::inst().unmap_from_kernel(*mmio_resource);
            const auto &mmio = mmio_resource->region();
            if (!unmap_res.has_value()) {
                loggers::DEVICE::ERROR("解除 MMIO 映射失败: kva=%p err=%s",
                                       mmio.begin.addr(),
                                       to_cstring(unmap_res.error()));
                continue;
            }
            loggers::DEVICE::DEBUG("已解除 MMIO 映射: [%p,%p)",
                                   mmio.begin.addr(), mmio.end.addr());
        }

        _virqs.clear();
        _mmios.clear();
    }
}  // namespace driver
