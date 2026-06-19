/**
 * @file cpu.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CPU 设备模型实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/device/cpu.h>
#include <logger.h>

namespace device {
    RiscV64Cpu::RiscV64Cpu(cpuid_t id, std::string model, units::frequency freq,
                           std::string isa_string, std::string mmu_type,
                           std::vector<CacheInfo> caches,
                           driver::intc_t local_intc) noexcept
        : _id(id),
          _model(std::move(model)),
          _frequency(freq),
          _isa_string(std::move(isa_string)),
          _mmu_type(std::move(mmu_type)),
          _caches(std::move(caches)),
          _local_intc(local_intc) {}

    RiscV64Cpu::~RiscV64Cpu() = default;

    cpuid_t RiscV64Cpu::id() const noexcept {
        return _id;
    }

    std::string RiscV64Cpu::model() const {
        return _model;
    }

    units::frequency RiscV64Cpu::frequency() const noexcept {
        return _frequency;
    }

    std::vector<CacheInfo> RiscV64Cpu::caches() const {
        return _caches;
    }

    std::string RiscV64Cpu::mmu_type() const {
        return _mmu_type;
    }

    Result<void> RiscV64Cpu::send_ipi() {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    std::string RiscV64Cpu::isa_string() const {
        return _isa_string;
    }

    driver::intc_t RiscV64Cpu::local_intc() const noexcept {
        return _local_intc;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::id(cpuid_t cpu_id) noexcept {
        _id = cpu_id;
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::model(std::string model) {
        _model = std::move(model);
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::frequency(
        units::frequency frequency) noexcept {
        _frequency = frequency;
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::isa_string(
        std::string isa_string) {
        _isa_string = std::move(isa_string);
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::mmu_type(std::string mmu_type) {
        _mmu_type = std::move(mmu_type);
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::caches(
        std::vector<CacheInfo> caches) {
        _caches = std::move(caches);
        return *this;
    }

    RiscV64Cpu::Builder &RiscV64Cpu::Builder::local_intc(
        driver::intc_t local_intc) noexcept {
        _local_intc = local_intc;
        return *this;
    }

    Result<util::owner<RiscV64Cpu *>> RiscV64Cpu::Builder::build() const {
        if (!_id.has_value() || !_model.has_value() ||
            !_frequency.has_value() || !_isa_string.has_value() ||
            !_mmu_type.has_value() || !_local_intc.has_value())
        {
            loggers::DEVICE::ERROR("构建 RiscV64Cpu 失败: 关键字段缺失");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (_model->empty() || _isa_string->empty()) {
            loggers::DEVICE::ERROR(
                "构建 RiscV64Cpu 失败: model/isa_string 为空");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return util::owner(new RiscV64Cpu(*_id, *_model, *_frequency,
                                          *_isa_string, *_mmu_type, _caches,
                                          *_local_intc));
    }
}  // namespace device
