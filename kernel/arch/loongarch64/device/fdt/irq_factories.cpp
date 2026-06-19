/**
 * @file irq_factories.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch FDT IRQ 工厂实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/cpuic.h>
#include <arch/loongarch64/device/eiointc.h>
#include <arch/loongarch64/device/fdt/irq_factories.h>
#include <arch/loongarch64/device/platform.h>
#include <arch/loongarch64/device/platic.h>
#include <device/fdt/decode.h>
#include <device/fdt/internal.h>
#include <driver/model.h>

namespace {
    constexpr driver::FDTDeviceId LOONGARCH_CPUIC_IDS[] = {
        {.compatible = ::fdt::LOONGARCH_CPUIC_COMPATIBLE, .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
    constexpr driver::FDTDeviceId LOONGARCH_EIOINTC_IDS[] = {
        {.compatible = la64::EioIntcChip::COMPATIBLE_STRING, .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
    constexpr driver::FDTDeviceId LOONGARCH_PLATIC_IDS[] = {
        {.compatible = la64::PlaticChip::COMPATIBLE_STRING, .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
}  // namespace

namespace la64::fdt {
    namespace {
        class LoongArchCpuICIrqFactory final : public device::IIrqChipFactory {
        public:
            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = LOONGARCH_CPUIC_IDS,
                    .pci_ids = nullptr,
                };
                return IDS;
            }

            [[nodiscard]]
            Result<driver::DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model,
                b64 driver_flag) const override {
                (void)driver_flag;
                auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
                if (_provider == nullptr || fdt_node == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }

                auto device_owner_res = ::la64::CpuICChip::create(
                    driver::DriverBase::DevRes(node, {}, {}),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<driver::intc_t>(fdt_node->raw_node().phandle)
                        : device::INVALID_ICTRL_ID,
                    0);
                propagate(device_owner_res);

                auto &cpuic = *device_owner_res.value();
                auto domain_res = model.interrupt().get_domain(cpuic.identifier());
                propagate(domain_res);
                auto *platform = model.platform();
                if (platform != nullptr &&
                    platform->is<::la64::LoongArch64Platform>())
                {
                    platform->as<::la64::LoongArch64Platform>()
                        ->set_global_intc(cpuic.identifier());
                }
                if (fdt_node->raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_node->raw_node().phandle, domain_res.value().get());
                    propagate(irq_domain_res);
                }

                for (driver::hwirq_t hwirq = 0;
                     hwirq < ::la64::CpuICChip::MAX_HW_IRQ; ++hwirq)
                {
                    auto virq_res = model.interrupt().allocate_virq(
                        domain_res.value().get().id(), hwirq);
                    propagate(virq_res);
                    if (hwirq == ::la64::CpuICChip::TIMER_IRQ) {
                        model.set_clock_virq(virq_res.value());
                    }
                }
                return static_cast<driver::DriverBase *>(device_owner_res.value().get());
            }

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            const ::fdt::FDTProvider *_provider = nullptr;
        };

        class LoongArchEioIntcIrqFactory final : public device::IIrqChipFactory {
        public:
            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = LOONGARCH_EIOINTC_IDS,
                    .pci_ids = nullptr,
                };
                return IDS;
            }

            [[nodiscard]]
            Result<driver::DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model,
                b64 driver_flag) const override {
                (void)driver_flag;
                auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
                if (_provider == nullptr || fdt_node == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }

                auto refs_res = _provider->parse_interrupts(fdt_node->raw_node());
                propagate(refs_res);
                driver::hwirq_t parent_hwirq =
                    refs_res.value().empty() ? 0 : refs_res.value().front().hwirq;
                auto virq_values_res =
                    _provider->parse_interrupt_virqs_view(
                        fdt_node->raw_node(), model.interrupt());
                propagate(virq_values_res);
                std::vector<util::owner<device::VIrqResource *>> virqs;
                virqs.reserve(virq_values_res.value().size());
                for (auto virq : virq_values_res.value()) {
                    virqs.push_back(device::VIrqResource::make(virq));
                }

                auto device_owner_res = ::la64::EioIntcChip::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs),
                        device::DevResManager::get_mmio_resource(node)),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<driver::intc_t>(fdt_node->raw_node().phandle)
                        : device::INVALID_ICTRL_ID,
                    parent_hwirq);
                propagate(device_owner_res);

                auto &chip = *device_owner_res.value();
                auto domain_res = model.interrupt().get_domain(chip.identifier());
                propagate(domain_res);
                if (fdt_node->raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_node->raw_node().phandle, domain_res.value().get());
                    propagate(irq_domain_res);
                }
                auto attach_res = chip.attach_to_parent_domain(
                    model.interrupt(), domain_res.value().get());
                propagate(attach_res);
                return static_cast<driver::DriverBase *>(device_owner_res.value().get());
            }

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            const ::fdt::FDTProvider *_provider = nullptr;
        };

        class LoongArchPlaticIrqFactory final : public device::IIrqChipFactory {
        public:
            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = LOONGARCH_PLATIC_IDS,
                    .pci_ids = nullptr,
                };
                return IDS;
            }

            [[nodiscard]]
            Result<driver::DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model,
                b64 driver_flag) const override {
                (void)driver_flag;
                auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
                if (_provider == nullptr || fdt_node == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }

                auto parent_phandle_res =
                    _provider->resolve_interrupt_parent(fdt_node->raw_node());
                if (!parent_phandle_res.has_value()) {
                    unexpect_return(parent_phandle_res.error());
                }
                auto virq_values_res =
                    _provider->parse_interrupt_virqs_view(
                        fdt_node->raw_node(), model.interrupt());
                propagate(virq_values_res);
                std::vector<util::owner<device::VIrqResource *>> virqs;
                virqs.reserve(virq_values_res.value().size());
                for (auto virq : virq_values_res.value()) {
                    virqs.push_back(device::VIrqResource::make(virq));
                }

                auto device_owner_res = ::la64::PlaticChip::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs),
                        device::DevResManager::get_mmio_resource(node)),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<driver::intc_t>(fdt_node->raw_node().phandle)
                        : device::INVALID_ICTRL_ID,
                    static_cast<driver::intc_t>(parent_phandle_res.value()));
                propagate(device_owner_res);

                auto &chip = *device_owner_res.value();
                auto domain_res = model.interrupt().get_domain(chip.identifier());
                propagate(domain_res);
                if (fdt_node->raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_node->raw_node().phandle, domain_res.value().get());
                    propagate(irq_domain_res);
                }
                auto attach_res = chip.attach_to_parent_domain(
                    model.interrupt(), domain_res.value().get());
                propagate(attach_res);
                return static_cast<driver::DriverBase *>(device_owner_res.value().get());
            }

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            const ::fdt::FDTProvider *_provider = nullptr;
        };
    }  // namespace

    void register_irq_factories(const ::fdt::FDTProvider &provider) noexcept {
        auto *cpuic_factory   = new LoongArchCpuICIrqFactory();
        auto *eiointc_factory = new LoongArchEioIntcIrqFactory();
        auto *platic_factory  = new LoongArchPlaticIrqFactory();
        if (cpuic_factory == nullptr || eiointc_factory == nullptr ||
            platic_factory == nullptr)
        {
            return;
        }
        cpuic_factory->bind_provider(provider);
        eiointc_factory->bind_provider(provider);
        platic_factory->bind_provider(provider);
        [[maybe_unused]] auto cpuic_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(cpuic_factory));
        [[maybe_unused]] auto eiointc_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(eiointc_factory));
        [[maybe_unused]] auto platic_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(platic_factory));
    }
}  // namespace la64::fdt
