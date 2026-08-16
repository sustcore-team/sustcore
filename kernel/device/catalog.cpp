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
#include <synchronized.h>

namespace device {
    namespace {
        constinit kernel::synchronized<Catalog::State> catalog_state;
        Catalog catalog_instance;

        [[nodiscard]] bool contains_device(const Catalog::State &state, FirmwareId id) noexcept {
            for (const auto &device : state.devices)
                if (device.id == id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_cpu(const Catalog::State &state, u32_t logical_id) noexcept {
            for (const auto &cpu : state.cpus)
                if (cpu.logical_id == logical_id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_cpu(const Catalog::State &state, FirmwareId id) noexcept {
            for (const auto &cpu : state.cpus)
                if (cpu.firmware_id == id)
                    return true;
            return false;
        }

        [[nodiscard]] bool contains_controller(const Catalog::State &state,
                                               FirmwareId id) noexcept {
            for (const auto &controller : state.controllers)
                if (controller.id == id)
                    return true;
            return false;
        }
    }  // namespace

    Catalog::State &Catalog::state() noexcept {
        return *catalog_state.lock();
    }

    const Catalog::State &Catalog::state() const noexcept {
        return *catalog_state.lock();
    }

    bool Catalog::ready() const noexcept {
        return state().ready;
    }

    const DeviceDescriptor *Catalog::find_device(FirmwareId id) const noexcept {
        const auto &snapshot = state();
        for (const auto &device : snapshot.devices)
            if (device.id == id)
                return &device;
        return nullptr;
    }

    const CpuDescriptor *Catalog::cpu(u32_t logical_id) const noexcept {
        const auto &snapshot = state();
        for (const auto &cpu : snapshot.cpus)
            if (cpu.logical_id == logical_id)
                return &cpu;
        return nullptr;
    }

    const CpuDescriptor *Catalog::bsp() const noexcept {
        const auto &snapshot = state();
        return snapshot.ready ? cpu(snapshot.bsp_logical_id) : nullptr;
    }

    const InterruptControllerDescriptor *Catalog::find_controller(FirmwareId id) const noexcept {
        const auto &snapshot = state();
        for (const auto &controller : snapshot.controllers)
            if (controller.id == id)
                return &controller;
        return nullptr;
    }

    const PlatformFacts &Catalog::platform() const noexcept {
        return state().platform;
    }

    size_t Catalog::device_count() const noexcept {
        return state().devices.size();
    }

    size_t Catalog::cpu_count() const noexcept {
        return state().cpus.size();
    }

    size_t Catalog::controller_count() const noexcept {
        return state().controllers.size();
    }

    const DeviceDescriptor *Catalog::devices_begin() const noexcept {
        return state().devices.begin();
    }

    const DeviceDescriptor *Catalog::devices_end() const noexcept {
        return state().devices.end();
    }

    const CpuDescriptor *Catalog::cpus_begin() const noexcept {
        return state().cpus.begin();
    }

    const CpuDescriptor *Catalog::cpus_end() const noexcept {
        return state().cpus.end();
    }

    const InterruptControllerDescriptor *Catalog::controllers_begin() const noexcept {
        return state().controllers.begin();
    }

    const InterruptControllerDescriptor *Catalog::controllers_end() const noexcept {
        return state().controllers.end();
    }

    tay::expected<void, CatalogError> CatalogBuilder::add_device(
        const DeviceDescriptor &device) noexcept {
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

    tay::expected<void, CatalogError> CatalogBuilder::add_cpu(const CpuDescriptor &cpu) noexcept {
        if (!cpu.firmware_id.valid())
            return tay::Err(CatalogError::InvalidDescriptor(cpu.firmware_id));
        if (contains_cpu(state_, cpu.firmware_id))
            return tay::Err(CatalogError::DuplicateFirmwareId(cpu.firmware_id));
        if (contains_cpu(state_, cpu.logical_id))
            return tay::Err(CatalogError::DuplicateLogicalCpu(cpu.logical_id));
        if (state_.cpus.full())
            return tay::Err(CatalogError::CapacityExhausted(CatalogError::EntryKind::CPU));
        return state_.cpus.push_back(cpu).transform_error(
            [](tay::error_code) noexcept -> CatalogError {
                return CatalogError::CapacityExhausted(CatalogError::EntryKind::CPU);
            });
    }

    tay::expected<void, CatalogError> CatalogBuilder::add_controller(
        const InterruptControllerDescriptor &controller) noexcept {
        if (!controller.id.valid())
            return tay::Err(CatalogError::InvalidDescriptor(controller.id));
        if (contains_controller(state_, controller.id))
            return tay::Err(CatalogError::DuplicateFirmwareId(controller.id));
        if (state_.controllers.full())
            return tay::Err(
                CatalogError::CapacityExhausted(CatalogError::EntryKind::INTERRUPT_CONTROLLER));
        return state_.controllers.push_back(controller)
            .transform_error([](tay::error_code) noexcept -> CatalogError {
                return CatalogError::CapacityExhausted(
                    CatalogError::EntryKind::INTERRUPT_CONTROLLER);
            });
    }

    void CatalogBuilder::set_platform(PlatformFacts platform) noexcept {
        state_.platform = platform;
    }

    void CatalogBuilder::set_bsp(u32_t logical_id) noexcept {
        state_.bsp_logical_id = logical_id;
    }

    tay::expected<void, CatalogError> initialize() noexcept {
        auto locked = catalog_state.lock();
        if (locked->ready)
            return {};

        CatalogBuilder builder(*locked);
        const auto &context = boot::context();
        fdt::Enumerator enumerator;
        const auto result = enumerator.enumerate(builder, FirmwareInput{.kind = FirmwareKind::FDT,
                                                                        .data = context.fdt,
                                                                        .size = context.fdt_sz,
                                                                        .boot_context = &context});
        if (!result) {
            locked->devices.clear();
            locked->cpus.clear();
            locked->controllers.clear();
            locked->platform = {};
            const auto cause = kernel::from_tay_error(result.error());
            return tay::Err(CatalogError::BackendFailed(
                cause ? *cause : kernel::KernelError::TayError::INTERNAL));
        }
        if (locked->cpus.empty())
            return tay::Err(CatalogError::NoCpuDiscovered());
        locked->ready = true;
        return {};
    }

    Catalog &catalog() noexcept {
        return catalog_instance;
    }
}  // namespace device
