/**
 * @file cpu.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 CPU 设备模型实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/cpu.h>
#include <arch/loongarch64/device/platform.h>
#include <device/model.h>
#include <logger.h>

namespace device {
    LoongArch64Cpu::LoongArch64Cpu(cpuid_t id, std::string model,
                                   units::frequency freq,
                                   std::string isa_string,
                                   std::string mmu_type,
                                   std::vector<CacheInfo> caches,
                                   driver::intc_t local_intc) noexcept
        : _id(id),
          _model(std::move(model)),
          _frequency(freq),
          _isa_string(std::move(isa_string)),
          _mmu_type(std::move(mmu_type)),
          _caches(std::move(caches)),
          _local_intc(local_intc) {}

    LoongArch64Cpu::~LoongArch64Cpu() = default;

    cpuid_t LoongArch64Cpu::id() const noexcept {
        return _id;
    }

    std::string LoongArch64Cpu::model() const {
        return _model;
    }

    units::frequency LoongArch64Cpu::frequency() const noexcept {
        return _frequency;
    }

    std::vector<CacheInfo> LoongArch64Cpu::caches() const {
        return _caches;
    }

    std::string LoongArch64Cpu::mmu_type() const {
        return _mmu_type;
    }

    std::string LoongArch64Cpu::isa_string() const {
        return _isa_string;
    }

    driver::intc_t LoongArch64Cpu::local_intc() const noexcept {
        if (!device::DeviceModel::initialized()) {
            return _local_intc;
        }
        auto *platform = device::DeviceModel::inst().platform();
        if (platform == nullptr || !platform->is<la64::LoongArch64Platform>()) {
            return _local_intc;
        }
        return platform->as<la64::LoongArch64Platform>()->global_intc();
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::id(
        cpuid_t cpu_id) noexcept {
        _id = cpu_id;
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::model(
        std::string model) {
        _model = std::move(model);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::frequency(
        units::frequency frequency) noexcept {
        _frequency = frequency;
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::isa_string(
        std::string isa_string) {
        _isa_string = std::move(isa_string);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::mmu_type(
        std::string mmu_type) {
        _mmu_type = std::move(mmu_type);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::caches(
        std::vector<CacheInfo> caches) {
        _caches = std::move(caches);
        return *this;
    }

    LoongArch64Cpu::Builder &LoongArch64Cpu::Builder::local_intc(
        driver::intc_t local_intc) noexcept {
        _local_intc = local_intc;
        return *this;
    }

    Result<util::owner<LoongArch64Cpu *>> LoongArch64Cpu::Builder::build() const {
        if (!_id.has_value() || !_model.has_value() || !_frequency.has_value() ||
            !_isa_string.has_value() || !_mmu_type.has_value() ||
            !_local_intc.has_value())
        {
            loggers::DEVICE::ERROR("构建 LoongArch64Cpu 失败: 关键字段缺失");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (_model->empty()) {
            loggers::DEVICE::ERROR("构建 LoongArch64Cpu 失败: model 为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return util::owner(new LoongArch64Cpu(*_id, *_model, *_frequency,
                                              *_isa_string, *_mmu_type,
                                              _caches, *_local_intc));
    }
}  // namespace device
