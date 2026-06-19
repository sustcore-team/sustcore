/**
 * @file cpu.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CPU 设备模型
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/cpu.h>

namespace device {
    class RiscV64Cpu : public Cpu {
    public:
        [[nodiscard]]
        CpuType type_id() const override {
            return CpuType::RISCV64;
        }

        ~RiscV64Cpu() override;
        [[nodiscard]]
        cpuid_t id() const noexcept override;
        [[nodiscard]]
        std::string model() const override;
        [[nodiscard]]
        units::frequency frequency() const noexcept override;
        [[nodiscard]]
        CpuPowerState state() const noexcept override {
            return CpuPowerState::ONLINE;
        }
        [[nodiscard]]
        std::vector<CacheInfo> caches() const override;
        [[nodiscard]]
        std::string mmu_type() const override;
        [[nodiscard]]
        Result<void> start() override {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        [[nodiscard]]
        Result<void> stop() override {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        [[nodiscard]]
        Result<void> send_ipi() override;
        [[nodiscard]]
        std::string isa_string() const;
        [[nodiscard]]
        driver::intc_t local_intc() const noexcept override;

    private:
        cpuid_t _id;
        std::string _model;
        units::frequency _frequency;
        std::string _isa_string;
        std::string _mmu_type;
        std::vector<CacheInfo> _caches;
        driver::intc_t _local_intc;

        RiscV64Cpu(cpuid_t id, std::string model, units::frequency freq,
                   std::string isa_string, std::string mmu_type,
                   std::vector<CacheInfo> caches,
                   driver::intc_t local_intc) noexcept;

    public:
        class Builder {
        private:
            std::optional<cpuid_t> _id;
            std::optional<std::string> _model;
            std::optional<units::frequency> _frequency;
            std::optional<std::string> _isa_string;
            std::optional<std::string> _mmu_type;
            std::vector<CacheInfo> _caches;
            std::optional<driver::intc_t> _local_intc;

        public:
            Builder &id(cpuid_t cpu_id) noexcept;
            Builder &model(std::string model);
            Builder &frequency(units::frequency frequency) noexcept;
            Builder &isa_string(std::string isa_string);
            Builder &mmu_type(std::string mmu_type);
            Builder &caches(std::vector<CacheInfo> caches);
            Builder &local_intc(driver::intc_t local_intc) noexcept;
            [[nodiscard]]
            Result<util::owner<RiscV64Cpu *>> build() const;
        };
    };
}  // namespace device
