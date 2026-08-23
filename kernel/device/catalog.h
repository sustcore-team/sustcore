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
#include <device/fw.h>
#include <error/catalog.h>
#include <sustcore/addr.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/static_vector.h>

#include <atomic>
#include <cstddef>

namespace boot {
    struct Context;
}

namespace device {
    constexpr size_t MAX_FW_DEVICES      = 128;
    constexpr size_t MAX_FW_CPUS         = 64;
    constexpr size_t MAX_IRQ_CTRLS       = 32;
    constexpr size_t MAX_DEVICE_NAME     = 64;
    constexpr size_t MAX_COMPATIBLE_NAME = 96;

    enum class DeviceClass : u8_t { GENERIC, IRQ_CTRL, TIMER, RTC };
    enum class BindPolicy : u8_t {
        USER_PREFERRED,
        USER_ONLY,
        KERNEL_PREFERRED,
        KERNEL_REQUIRED,
    };
    enum class IrqCtrlRole : u8_t { CPU_LOCAL, ROOT, CASCADED, MSI, IOMMU };

    struct MmioRes {
        PhyArea area{};
        bool present = false;
    };

    struct DeviceDesc {
        FwId id{};
        FwId parent{};
        DeviceClass kind       = DeviceClass::GENERIC;
        BindPolicy bind_policy = BindPolicy::USER_PREFERRED;
        bool enabled           = false;
        char name[MAX_DEVICE_NAME]{};
        char compatible[MAX_COMPATIBLE_NAME]{};
        MmioRes first_mmio{};
    };

    struct CpuDesc {
        u32_t cpu_id = 0;
        u64_t hw_id  = 0;
        FwId fw_id{};
        bool enabled = false;
        char model[MAX_DEVICE_NAME]{};
    };

    struct IrqCtrlDesc {
        FwId id{};
        IrqCtrlRole role = IrqCtrlRole::ROOT;
        FwId parent{};
        u32_t irq_cells = 0;
        char compatible[MAX_COMPATIBLE_NAME]{};
        MmioRes first_mmio{};
    };

    /** @brief 启动器已经规范化的内存图和固件提供的全局时基。 */
    struct PlatformFacts {
        const MemoryRegion *mem_regions = nullptr;
        size_t mem_region_count         = 0;
        u64_t timebase_hz               = 0;
    };

    struct FwInput {
        FwKind kind                       = FwKind::NONE;
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
        struct Data {
            tay::static_vector<DeviceDesc, MAX_FW_DEVICES> devices;
            tay::static_vector<CpuDesc, MAX_FW_CPUS> cpus;
            tay::static_vector<IrqCtrlDesc, MAX_IRQ_CTRLS> irq_ctrls;
            PlatformFacts platform{};
            u32_t bsp_cpu_id = 0;
        };

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] const DeviceDesc *find_device(FwId id) const noexcept;
        [[nodiscard]] const CpuDesc *cpu(u32_t cpu_id) const noexcept;
        [[nodiscard]] const CpuDesc *bsp() const noexcept;
        [[nodiscard]] const IrqCtrlDesc *find_irq_ctrl(FwId id) const noexcept;
        [[nodiscard]] const PlatformFacts &platform() const noexcept;
        [[nodiscard]] size_t device_count() const noexcept;
        [[nodiscard]] size_t cpu_count() const noexcept;
        [[nodiscard]] size_t irq_ctrl_count() const noexcept;

        [[nodiscard]] const DeviceDesc *devices_begin() const noexcept;
        [[nodiscard]] const DeviceDesc *devices_end() const noexcept;
        [[nodiscard]] const CpuDesc *cpus_begin() const noexcept;
        [[nodiscard]] const CpuDesc *cpus_end() const noexcept;
        [[nodiscard]] const IrqCtrlDesc *irq_ctrl_begin() const noexcept;
        [[nodiscard]] const IrqCtrlDesc *irq_ctrl_end() const noexcept;

    private:
        [[nodiscard]] Data &state() noexcept;
        [[nodiscard]] const Data &state() const noexcept;

        Data state_{};
        std::atomic<bool> published_{false};

        friend class CatalogBuilder;
        friend tay::expected<void, CatalogError> initialize() noexcept;
    };

    /** @brief 仅在启动枚举 transaction 内可写的目录构造器。 */
    class CatalogBuilder final {
    public:
        [[nodiscard]] tay::expected<void, CatalogError> add_device(
            const DeviceDesc &device) noexcept;
        [[nodiscard]] tay::expected<void, CatalogError> add_cpu(const CpuDesc &cpu) noexcept;
        [[nodiscard]] tay::expected<void, CatalogError> add_irq_ctrl(
            const IrqCtrlDesc &irq_ctrl) noexcept;
        void set_platform(PlatformFacts platform) noexcept;
        void set_bsp(u32_t cpu_id) noexcept;

    private:
        explicit CatalogBuilder(Catalog::Data &state) noexcept : state_(state) {}

        Catalog::Data &state_;

        friend tay::expected<void, CatalogError> initialize() noexcept;
    };

    /** @brief FDT、ACPI 等后端共同实现的固件输入合同。 */
    class FwEnumerator {
    public:
        virtual ~FwEnumerator() = default;
        [[nodiscard]] virtual tay::expected<void, tay::error_code> enumerate(CatalogBuilder &,
                                                                             FwInput) noexcept = 0;
    };

    /**
     * @brief 从 boot::Context 的持久固件输入构造并发布目录。
     * @pre 仅 BSP 在 HEAP_READY 后、FIRMWARE_READY 前调用一次。
     */
    [[nodiscard]] tay::expected<void, CatalogError> initialize() noexcept;
    [[nodiscard]] Catalog &catalog() noexcept;
}  // namespace device
