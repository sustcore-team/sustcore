/**
 * @file catalog.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 固件设备、CPU 与平台事实的只读启动期目录。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>
#include <device/catalog_error.h>
#include <device/catalog_types.h>
#include <sustcore/addr.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/static_vector.h>

#include <cstddef>

namespace boot {
    struct Context;
}

namespace device {
    constexpr size_t MAX_FIRMWARE_DEVICES      = 128;
    constexpr size_t MAX_FIRMWARE_CPUS         = 64;
    constexpr size_t MAX_INTERRUPT_CONTROLLERS = 32;
    constexpr size_t MAX_DEVICE_NAME           = 64;
    constexpr size_t MAX_COMPATIBLE_NAME       = 96;

    enum class DeviceClass : u8_t { GENERIC, INTERRUPT_CONTROLLER, TIMER, RTC };
    enum class BindingPolicy : u8_t {
        USER_PREFERRED,
        USER_ONLY,
        KERNEL_PREFERRED,
        KERNEL_REQUIRED,
    };
    enum class ControllerRole : u8_t { CPU_LOCAL, ROOT, CASCADED, MSI, IOMMU };

    struct MmioResource {
        PhyArea area{};
        bool present = false;
    };

    struct DeviceDescriptor {
        FirmwareId id{};
        FirmwareId parent{};
        DeviceClass device_class     = DeviceClass::GENERIC;
        BindingPolicy binding_policy = BindingPolicy::USER_PREFERRED;
        bool enabled                 = false;
        char name[MAX_DEVICE_NAME]{};
        char compatible[MAX_COMPATIBLE_NAME]{};
        MmioResource first_mmio{};
    };

    struct CpuDescriptor {
        u32_t logical_id  = 0;
        u64_t hardware_id = 0;
        FirmwareId firmware_id{};
        bool enabled = false;
        char model[MAX_DEVICE_NAME]{};
    };

    struct InterruptControllerDescriptor {
        FirmwareId id{};
        ControllerRole role = ControllerRole::ROOT;
        FirmwareId parent{};
        u32_t interrupt_cells = 0;
        char compatible[MAX_COMPATIBLE_NAME]{};
        MmioResource first_mmio{};
    };

    /** @brief 启动器已经规范化的内存图和固件提供的全局时基。 */
    struct PlatformFacts {
        const MemoryRegion *memory_regions = nullptr;
        size_t memory_region_count         = 0;
        u64_t timebase_frequency_hz        = 0;
    };

    struct FirmwareInput {
        FirmwareKind kind                 = FirmwareKind::NONE;
        const void *data                  = nullptr;
        size_t size                       = 0;
        const boot::Context *boot_context = nullptr;
    };

    /**
     * @brief 四类枚举数据的不可变快照。
     * @note initialize() 发布后内容不再变化；运行时总线发现必须使用后续 generation API。
     */
    class Catalog final {
    public:
        // Builder 与实现文件共享的启动期存储；对普通消费者只暴露只读查询 API。
        struct State {
            tay::static_vector<DeviceDescriptor, MAX_FIRMWARE_DEVICES> devices;
            tay::static_vector<CpuDescriptor, MAX_FIRMWARE_CPUS> cpus;
            tay::static_vector<InterruptControllerDescriptor, MAX_INTERRUPT_CONTROLLERS>
                controllers;
            PlatformFacts platform{};
            u32_t bsp_logical_id = 0;
            bool ready           = false;
        };

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] const DeviceDescriptor *find_device(FirmwareId id) const noexcept;
        [[nodiscard]] const CpuDescriptor *cpu(u32_t logical_id) const noexcept;
        [[nodiscard]] const CpuDescriptor *bsp() const noexcept;
        [[nodiscard]] const InterruptControllerDescriptor *find_controller(
            FirmwareId id) const noexcept;
        [[nodiscard]] const PlatformFacts &platform() const noexcept;
        [[nodiscard]] size_t device_count() const noexcept;
        [[nodiscard]] size_t cpu_count() const noexcept;
        [[nodiscard]] size_t controller_count() const noexcept;

        [[nodiscard]] const DeviceDescriptor *devices_begin() const noexcept;
        [[nodiscard]] const DeviceDescriptor *devices_end() const noexcept;
        [[nodiscard]] const CpuDescriptor *cpus_begin() const noexcept;
        [[nodiscard]] const CpuDescriptor *cpus_end() const noexcept;
        [[nodiscard]] const InterruptControllerDescriptor *controllers_begin() const noexcept;
        [[nodiscard]] const InterruptControllerDescriptor *controllers_end() const noexcept;

    private:
        [[nodiscard]] State &state() noexcept;
        [[nodiscard]] const State &state() const noexcept;

        friend class CatalogBuilder;
    };

    /** @brief 仅在启动枚举 transaction 内可写的目录构造器。 */
    class CatalogBuilder final {
    public:
        [[nodiscard]] tay::expected<void, CatalogError> add_device(
            const DeviceDescriptor &device) noexcept;
        [[nodiscard]] tay::expected<void, CatalogError> add_cpu(const CpuDescriptor &cpu) noexcept;
        [[nodiscard]] tay::expected<void, CatalogError> add_controller(
            const InterruptControllerDescriptor &controller) noexcept;
        void set_platform(PlatformFacts platform) noexcept;
        void set_bsp(u32_t logical_id) noexcept;

    private:
        explicit CatalogBuilder(Catalog::State &state) noexcept : state_(state) {}

        Catalog::State &state_;

        friend tay::expected<void, CatalogError> initialize() noexcept;
    };

    /** @brief FDT、ACPI 等后端共同实现的固件输入合同。 */
    class FirmwareEnumerator {
    public:
        virtual ~FirmwareEnumerator() = default;
        [[nodiscard]] virtual tay::expected<void, tay::error_code> enumerate(
            CatalogBuilder &, FirmwareInput) noexcept = 0;
    };

    /**
     * @brief 从 boot::Context 的持久固件输入构造并发布目录。
     * @pre 仅 BSP 在 HEAP_READY 后、FIRMWARE_READY 前调用一次。
     */
    [[nodiscard]] tay::expected<void, CatalogError> initialize() noexcept;
    [[nodiscard]] Catalog &catalog() noexcept;
}  // namespace device
