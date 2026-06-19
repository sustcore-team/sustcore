/**
 * @file irq_factories.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V FDT IRQ 工厂实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/clint.h>
#include <arch/riscv64/device/fdt/irq_factories.h>
#include <arch/riscv64/device/intc.h>
#include <arch/riscv64/device/plic.h>
#include <device/fdt/decode.h>
#include <device/fdt/internal.h>
#include <driver/model.h>

namespace {
    constexpr size_t CLINT_MAX_HW_IRQ = 16;

    constexpr driver::FDTDeviceId RISCV_CPU_INTC_IDS[] = {
        {.compatible = "", .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
    constexpr driver::FDTDeviceId CLINT_IDS[] = {
        {.compatible = fdt::CLINT_COMPATIBLE, .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
    constexpr driver::FDTDeviceId PLIC_IDS[] = {
        {.compatible = fdt::PLIC_COMPATIBLE, .driver_flag = 0},
        {.compatible = nullptr, .driver_flag = 0},
    };
}  // namespace

namespace riscv::fdt {
    namespace {
        class RiscVIntCIrqFactory final : public device::IIrqChipFactory {
        public:
            explicit RiscVIntCIrqFactory(
                std::vector<::fdt::CpuIntcDescriptor> *candidates) noexcept
                : _candidates(candidates) {}

            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = RISCV_CPU_INTC_IDS,
                    .pci_ids = nullptr,
                };
                return IDS;
            }

            [[nodiscard]]
            bool probe(const device::DeviceNode &node,
                       device::DeviceModel &model,
                       b64 driver_flag) const noexcept override {
                (void)model;
                (void)driver_flag;
                auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
                if (_candidates == nullptr || fdt_node == nullptr) {
                    return false;
                }
                return std::find_if(
                           _candidates->begin(), _candidates->end(),
                           [fdt_node](const ::fdt::CpuIntcDescriptor &desc) {
                               return desc.node != nullptr &&
                                      desc.node == &fdt_node->raw_node();
                           }) != _candidates->end();
            }

            [[nodiscard]]
            Result<driver::DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model,
                b64 driver_flag) const override {
                (void)driver_flag;
                (void)model;
                if (_candidates == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
                if (fdt_node == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                const auto matched = std::find_if(
                    _candidates->begin(), _candidates->end(),
                    [fdt_node](const ::fdt::CpuIntcDescriptor &desc) {
                        return desc.node != nullptr &&
                               desc.node == &fdt_node->raw_node();
                    });
                if (matched == _candidates->end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto virqs = device::DevResManager::get_virq_resource(node);
                auto mmios = device::DevResManager::get_mmio_resource(node);
                auto device_owner_res = ::riscv::IntC::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs), std::move(mmios)),
                    matched->identifier, matched->hart_id);
                propagate(device_owner_res);
                auto &root = *device_owner_res.value();
                auto domain_res = model.interrupt().get_domain(root.identifier());
                propagate(domain_res);
                auto irq_domain_res = _provider->register_irq_domain_view(
                    static_cast<::fdt::phandle_t>(root.identifier()),
                    domain_res.value().get());
                propagate(irq_domain_res);
                return static_cast<driver::DriverBase *>(
                    device_owner_res.value().get());
            }

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            std::vector<::fdt::CpuIntcDescriptor> *_candidates = nullptr;
            const ::fdt::FDTProvider *_provider                = nullptr;
        };

        class ClintIrqFactory final : public device::IIrqChipFactory {
        public:
            explicit ClintIrqFactory(
                ::fdt::LocalInterruptTargetMap *local_intc_map) noexcept
                : _local_intc_map(local_intc_map) {}

            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = CLINT_IDS,
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
                if (_provider == nullptr || _local_intc_map == nullptr ||
                    fdt_node == nullptr)
                {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }

                auto refs_res = _provider->parse_interrupts_extended_view(
                    fdt_node->raw_node());
                propagate(refs_res);
                std::vector<::fdt::ParsedInterruptRef> parsed_refs;
                parsed_refs.reserve(refs_res.value().size());
                for (const auto &ref : refs_res.value()) {
                    parsed_refs.push_back(::fdt::ParsedInterruptRef{
                        .phandle = ref.phandle,
                        .hwirq   = ref.hwirq,
                        .trigger = ref.trigger,
                    });
                }
                auto target_harts = ::fdt::target_harts_from_interrupt_refs(
                    fdt_node->raw_node().name.c_str(), parsed_refs,
                    *_local_intc_map);
                if (target_harts.empty()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                auto &irqman = model.interrupt();
                auto virq_values_res =
                    _provider->parse_interrupt_virqs_view(
                        fdt_node->raw_node(), irqman);
                propagate(virq_values_res);
                std::vector<util::owner<device::VIrqResource *>> virqs;
                virqs.reserve(virq_values_res.value().size());
                for (auto virq : virq_values_res.value()) {
                    virqs.push_back(device::VIrqResource::make(virq));
                }
                auto mmios = device::DevResManager::get_mmio_resource(node);

                auto device_owner_res = ::riscv::Clint::create(
                    driver::DriverBase::DevRes(
                        node, std::move(virqs), std::move(mmios)),
                    fdt_node->raw_node().phandle != 0
                        ? static_cast<driver::intc_t>(
                              fdt_node->raw_node().phandle)
                        : static_cast<driver::intc_t>(1),
                    target_harts.front(), target_harts);
                propagate(device_owner_res);
                auto &clint = *device_owner_res.value();
                const auto &fdt_device_node =
                    static_cast<const ::fdt::FDTDeviceNode &>(clint.node());
                auto *domain = new driver::LinearIrqDomain<CLINT_MAX_HW_IRQ>(
                    static_cast<driver::domain_t>(clint.identifier()),
                    clint.name(), clint);
                if (domain == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                auto register_res = model.interrupt().register_domain(
                    util::owner<driver::IrqDomain *>(domain));
                propagate(register_res);
                if (fdt_device_node.raw_node().phandle != 0) {
                    auto irq_domain_res = _provider->register_irq_domain_view(
                        fdt_device_node.raw_node().phandle, *domain);
                    propagate(irq_domain_res);
                }
                auto attach_res =
                    clint.attach_to_parent_domain(model.interrupt(), *domain);
                propagate(attach_res);
                model.set_clock_virq(clint.clock_virq());
                return static_cast<driver::DriverBase *>(
                    device_owner_res.value().get());
            }

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            const ::fdt::FDTProvider *_provider         = nullptr;
            ::fdt::LocalInterruptTargetMap *_local_intc_map = nullptr;
        };

        class PlicIrqFactory final : public device::IIrqChipFactory {
        public:
            [[nodiscard]]
            const driver::DeviceId &device_id() const noexcept override {
                static constexpr driver::DeviceId IDS{
                    .fdt_ids = PLIC_IDS,
                    .pci_ids = nullptr,
                };
                return IDS;
            }

            [[nodiscard]]
            Result<driver::DriverBase *> create(
                const device::DeviceNode &node,
                device::DeviceModel &model,
                b64 driver_flag) const override;

            void bind_provider(const ::fdt::FDTProvider &provider) noexcept {
                _provider = &provider;
            }

        private:
            [[nodiscard]]
            Result<std::vector<::riscv::Plic::Context>> build_plic_contexts(
                const std::vector<::fdt::ParsedInterruptRef> &refs,
                driver::IrqManager &irqman) const noexcept;

            [[nodiscard]]
            Result<::riscv::Plic::Context> build_plic_context(
                size_t index, ::fdt::phandle_t phandle, driver::hwirq_t hwirq,
                driver::IrqManager &irqman) const noexcept;

            const ::fdt::FDTProvider *_provider = nullptr;
        };

        Result<driver::DriverBase *> PlicIrqFactory::create(
            const device::DeviceNode &node,
            device::DeviceModel &model,
            b64 driver_flag) const {
            (void)driver_flag;
            auto *fdt_node = node.as<::fdt::FDTDeviceNode>();
            if (_provider == nullptr || fdt_node == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            auto refs_res = _provider->parse_interrupts_extended_view(
                fdt_node->raw_node());
            propagate(refs_res);
            std::vector<::fdt::ParsedInterruptRef> parsed_refs;
            parsed_refs.reserve(refs_res.value().size());
            for (const auto &ref : refs_res.value()) {
                parsed_refs.push_back(::fdt::ParsedInterruptRef{
                    .phandle = ref.phandle,
                    .hwirq   = ref.hwirq,
                    .trigger = ref.trigger,
                });
            }

            auto &irqman = model.interrupt();
            auto contexts_res = build_plic_contexts(parsed_refs, irqman);
            propagate(contexts_res);
            auto contexts = std::move(contexts_res.value());
            if (contexts.empty()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            auto ndev_it = fdt_node->raw_node().properties.find(::fdt::RISCV_NDEV_PROP);
            if (ndev_it == fdt_node->raw_node().properties.end()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            std::vector<driver::virq_t> virq_values;
            virq_values.reserve(refs_res.value().size());
            for (const auto &ref : refs_res.value()) {
                auto domain_res =
                    _provider->resolve_irq_domain_view(ref.phandle, irqman);
                if (!domain_res.has_value()) {
                    virq_values.push_back(driver::INVALID_VIRQ);
                    continue;
                }

                auto virq_res = irqman.allocate_virq(
                    domain_res.value().get().id(), ref.hwirq);
                if (!virq_res.has_value()) {
                    virq_values.push_back(driver::INVALID_VIRQ);
                    continue;
                }

                if (ref.trigger.has_value()) {
                    auto trigger_res =
                        irqman.set_trigger(virq_res.value(), *ref.trigger);
                    if (!trigger_res.has_value() &&
                        trigger_res.error() != ErrCode::NOT_SUPPORTED)
                    {
                        virq_values.push_back(driver::INVALID_VIRQ);
                        continue;
                    }
                }

                virq_values.push_back(virq_res.value());
            }
            std::vector<util::owner<device::VIrqResource *>> virqs;
            virqs.reserve(virq_values.size());
            for (auto virq : virq_values) {
                virqs.push_back(device::VIrqResource::make(virq));
            }
            auto mmios = device::DevResManager::get_mmio_resource(node);

            auto device_owner_res = ::riscv::Plic::create(
                driver::DriverBase::DevRes(
                    node, std::move(virqs), std::move(mmios)),
                fdt_node->raw_node().phandle != 0
                    ? static_cast<driver::intc_t>(
                          fdt_node->raw_node().phandle)
                    : static_cast<driver::intc_t>(3),
                static_cast<driver::hwirq_t>(
                    ndev_it->second->as_integral()),
                std::move(contexts));
            propagate(device_owner_res);
            auto &plic = *device_owner_res.value();
            const auto &fdt_device_node =
                static_cast<const ::fdt::FDTDeviceNode &>(plic.node());
            auto domain_res = model.interrupt().get_domain(plic.identifier());
            propagate(domain_res);
            if (fdt_device_node.raw_node().phandle != 0) {
                auto irq_domain_res = _provider->register_irq_domain_view(
                    fdt_device_node.raw_node().phandle,
                    domain_res.value().get());
                propagate(irq_domain_res);
            }
            auto attach_res =
                plic.attach_to_parent_domain(model.interrupt(),
                                             domain_res.value().get());
            propagate(attach_res);
            return static_cast<driver::DriverBase *>(
                device_owner_res.value().get());
        }

        Result<std::vector<::riscv::Plic::Context>>
        PlicIrqFactory::build_plic_contexts(
            const std::vector<::fdt::ParsedInterruptRef> &refs,
            driver::IrqManager &irqman) const noexcept {
            std::vector<::riscv::Plic::Context> contexts;
            contexts.reserve(refs.size());

            for (size_t index = 0; index < refs.size(); ++index) {
                auto context_res = build_plic_context(index, refs[index].phandle,
                                                      refs[index].hwirq, irqman);
                if (!context_res.has_value()) {
                    propagate_return(context_res);
                }
                contexts.push_back(context_res.value());
            }

            return contexts;
        }

        Result<::riscv::Plic::Context> PlicIrqFactory::build_plic_context(
            size_t index, ::fdt::phandle_t phandle, driver::hwirq_t hwirq,
            driver::IrqManager &irqman) const noexcept {
            auto *intc_node = _provider->config().get_node_by_phandle(phandle);
            if (intc_node == nullptr || intc_node->parent == nullptr) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            auto parsed_res = ::fdt::parse_cpu_node(*intc_node->parent);
            if (!parsed_res.has_value()) {
                propagate_return(parsed_res);
            }

            auto hart_id = parsed_res.value().id;
            auto ctx_id = static_cast<::riscv::Plic::ctx_t>(
                hart_id * 2 + index % 2);
            ::riscv::Plic::Context context{
                .hart_id       = hart_id,
                .external_virq = 0,
                .ctx_id        = ctx_id,
                .enabled       = false,
                .completed     = true,
            };

            auto parent_domain_res =
                _provider->resolve_irq_domain_view(phandle, irqman);
            if (!parent_domain_res.has_value()) {
                return context;
            }

            auto virq_res = irqman.allocate_virq(
                parent_domain_res.value().get().id(), hwirq);
            if (!virq_res.has_value()) {
                return context;
            }

            context.external_virq = virq_res.value();
            context.enabled       = true;
            return context;
        }
    }  // namespace

    void register_irq_factories(const ::fdt::FDTProvider &provider) noexcept {
        auto *root_factory  =
            new RiscVIntCIrqFactory(&provider.cpu_intc_candidates());
        auto *clint_factory =
            new ClintIrqFactory(&provider.local_intc_map());
        auto *plic_factory  = new PlicIrqFactory();
        if (root_factory == nullptr || clint_factory == nullptr ||
            plic_factory == nullptr)
        {
            return;
        }

        root_factory->bind_provider(provider);
        clint_factory->bind_provider(provider);
        plic_factory->bind_provider(provider);

        [[maybe_unused]] auto root_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(root_factory));
        [[maybe_unused]] auto clint_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(clint_factory));
        [[maybe_unused]] auto plic_res =
            driver::DriverModel::inst().register_factory(
                util::owner<driver::IIrqChipFactory *>(plic_factory));
    }
}  // namespace riscv::fdt
