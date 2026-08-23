/**
 * @file catalog.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备目录发布与只读查询实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/context.h>
#include <device/catalog.h>
#include <device/fdt.h>

namespace device {
    namespace {
        constinit Catalog catalog_instance;
        constinit const PlatformFacts empty_platform{};

        [[nodiscard]] bool contains_device(const Catalog::Data &state, FwId id) noexcept {
            for (const auto &device : state.devices)
                if (device.id == id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_cpu(const Catalog::Data &state, u32_t cpu_id) noexcept {
            for (const auto &cpu : state.cpus)
                if (cpu.cpu_id == cpu_id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_cpu(const Catalog::Data &state, FwId id) noexcept {
            for (const auto &cpu : state.cpus)
                if (cpu.fw_id == id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_irq_ctrl(const Catalog::Data &state, FwId id) noexcept {
            for (const auto &irq_ctrl : state.irq_ctrls)
                if (irq_ctrl.id == id)
                    return true;
            return false;
        }
    }  // namespace

    Catalog::Data &Catalog::state() noexcept {
        return state_;
    }

    const Catalog::Data &Catalog::state() const noexcept {
        return state_;
    }

    bool Catalog::ready() const noexcept {
        return published_.load(std::memory_order_acquire);
    }

    const DeviceDesc *Catalog::find_device(FwId id) const noexcept {
        if (!ready())
            return nullptr;
        const auto &snapshot = state();
        for (const auto &device : snapshot.devices)
            if (device.id == id)
                return &device;
        return nullptr;
    }

    const CpuDesc *Catalog::cpu(u32_t cpu_id) const noexcept {
        if (!ready())
            return nullptr;
        const auto &snapshot = state();
        for (const auto &cpu : snapshot.cpus)
            if (cpu.cpu_id == cpu_id)
                return &cpu;
        return nullptr;
    }

    const CpuDesc *Catalog::bsp() const noexcept {
        if (!ready())
            return nullptr;
        const auto &snapshot = state();
        return cpu(snapshot.bsp_cpu_id);
    }

    const IrqCtrlDesc *Catalog::find_irq_ctrl(FwId id) const noexcept {
        if (!ready())
            return nullptr;
        const auto &snapshot = state();
        for (const auto &irq_ctrl : snapshot.irq_ctrls)
            if (irq_ctrl.id == id)
                return &irq_ctrl;
        return nullptr;
    }

    const PlatformFacts &Catalog::platform() const noexcept {
        if (!ready())
            return empty_platform;
        return state().platform;
    }

    size_t Catalog::device_count() const noexcept {
        return ready() ? state().devices.size() : 0;
    }

    size_t Catalog::cpu_count() const noexcept {
        return ready() ? state().cpus.size() : 0;
    }

    size_t Catalog::irq_ctrl_count() const noexcept {
        return ready() ? state().irq_ctrls.size() : 0;
    }

    const DeviceDesc *Catalog::devices_begin() const noexcept {
        return ready() ? state().devices.begin() : nullptr;
    }

    const DeviceDesc *Catalog::devices_end() const noexcept {
        return ready() ? state().devices.end() : nullptr;
    }

    const CpuDesc *Catalog::cpus_begin() const noexcept {
        return ready() ? state().cpus.begin() : nullptr;
    }

    const CpuDesc *Catalog::cpus_end() const noexcept {
        return ready() ? state().cpus.end() : nullptr;
    }

    const IrqCtrlDesc *Catalog::irq_ctrl_begin() const noexcept {
        return ready() ? state().irq_ctrls.begin() : nullptr;
    }

    const IrqCtrlDesc *Catalog::irq_ctrl_end() const noexcept {
        return ready() ? state().irq_ctrls.end() : nullptr;
    }

    tay::expected<void, CatalogError> CatalogBuilder::add_device(
        const DeviceDesc &device) noexcept {
        if (!device.id.valid())
            return tay::Err(CatalogError::InvalidDescriptor(device.id));
        if (contains_device(state_, device.id))
            return tay::Err(CatalogError::DuplicateFirmwareId(device.id));
        if (state_.devices.full())
            return tay::Err(CatalogError::CapacityExhausted(CatalogError::EntryKind::DEVICE));
        return state_.devices.push_back(device).transform_error(
            [](tay::error_code) noexcept -> CatalogError {
                return CatalogError::CapacityExhausted(CatalogError::EntryKind::DEVICE);
            });
    }

    tay::expected<void, CatalogError> CatalogBuilder::add_cpu(const CpuDesc &cpu) noexcept {
        if (!cpu.fw_id.valid())
            return tay::Err(CatalogError::InvalidDescriptor(cpu.fw_id));
        if (contains_cpu(state_, cpu.fw_id))
            return tay::Err(CatalogError::DuplicateFirmwareId(cpu.fw_id));
        if (contains_cpu(state_, cpu.cpu_id))
            return tay::Err(CatalogError::DuplicateLogicalCpu(cpu.cpu_id));
        if (state_.cpus.full())
            return tay::Err(CatalogError::CapacityExhausted(CatalogError::EntryKind::CPU));
        return state_.cpus.push_back(cpu).transform_error(
            [](tay::error_code) noexcept -> CatalogError {
                return CatalogError::CapacityExhausted(CatalogError::EntryKind::CPU);
            });
    }

    tay::expected<void, CatalogError> CatalogBuilder::add_irq_ctrl(
        const IrqCtrlDesc &irq_ctrl) noexcept {
        if (!irq_ctrl.id.valid())
            return tay::Err(CatalogError::InvalidDescriptor(irq_ctrl.id));
        if (contains_irq_ctrl(state_, irq_ctrl.id))
            return tay::Err(CatalogError::DuplicateFirmwareId(irq_ctrl.id));
        if (state_.irq_ctrls.full())
            return tay::Err(CatalogError::CapacityExhausted(CatalogError::EntryKind::IRQ_CTRL));
        return state_.irq_ctrls.push_back(irq_ctrl).transform_error(
            [](tay::error_code) noexcept -> CatalogError {
                return CatalogError::CapacityExhausted(CatalogError::EntryKind::IRQ_CTRL);
            });
    }

    void CatalogBuilder::set_platform(PlatformFacts platform) noexcept {
        state_.platform = platform;
    }

    void CatalogBuilder::set_bsp(u32_t cpu_id) noexcept {
        state_.bsp_cpu_id = cpu_id;
    }

    tay::expected<void, CatalogError> initialize() noexcept {
        auto &catalog = catalog_instance;
        auto &state   = catalog.state();
        if (catalog.ready())
            return {};

        state.devices.clear();
        state.cpus.clear();
        state.irq_ctrls.clear();
        state.platform = {};

        CatalogBuilder builder(state);
        const auto &context = boot::context();
        fdt::Enumerator enumerator;
        const auto result = enumerator.enumerate(builder, FwInput{.kind         = FwKind::FDT,
                                                                  .data         = context.fdt,
                                                                  .size         = context.fdt_sz,
                                                                  .boot_context = &context});
        if (!result) {
            state.devices.clear();
            state.cpus.clear();
            state.irq_ctrls.clear();
            state.platform   = {};
            const auto cause = kernel::from_tay_error(result.error());
            return tay::Err(CatalogError::BackendFailed(
                cause ? *cause : kernel::KernelError::TayError::INTERNAL));
        }
        if (state.cpus.empty()) {
            state.devices.clear();
            state.irq_ctrls.clear();
            state.platform = {};
            return tay::Err(CatalogError::NoCpuDiscovered());
        }
        catalog.published_.store(true, std::memory_order_release);
        return {};
    }

    Catalog &catalog() noexcept {
        return catalog_instance;
    }
}  // namespace device
