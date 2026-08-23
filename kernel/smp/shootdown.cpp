/**
 * @file shootdown.cpp
 * @brief TLB shootdown 请求发布、IPI fan-out 和 acknowledgement 等待。
 */

#include <arch/cpu.h>
#include <arch/paging_traits.h>
#include <arch/timer.h>
#include <cpu/local.h>
#include <log.h>
#include <smp/ipi.h>
#include <smp/shootdown.h>
#include <synchronized.h>

#include <atomic>
#include <limits>

namespace smp {
    namespace detail::shootdown {
        struct CoordinatorState final {
            TlbShootdownSnapshot request{};
        };

        constinit kernel::synchronized<CoordinatorState> coordinator{};
        constinit std::atomic<u64_t> published_generation{0};
        constinit std::atomic<u64_t> acknowledgements[cpu::MAX_CPUS]{};
        constinit std::atomic<bool> initialized{false};

        void acknowledge(cpu::CpuId id, u64_t generation) noexcept {
            if (!id.valid())
                __builtin_trap();

            auto &acknowledged = acknowledgements[id.value];
            u64_t observed     = acknowledged.load(std::memory_order_relaxed);
            while (observed < generation &&
                   !acknowledged.compare_exchange_weak(
                       observed, generation, std::memory_order_release, std::memory_order_relaxed))
            {}
        }

        void wait_acks(const cpu::CpuSet &targets, u64_t generation) noexcept {
            // Shootdown 是回收屏障；缺失 acknowledgement 后继续会释放仍存在于远端 TLB 的
            // 页面。以 deadline 把 firmware/AP 故障转为包含目标 CPU 的致命诊断，避免 BSP
            // 无限等待。
            constexpr auto ACK_TIMEOUT = 1_s;
            const auto deadline        = hal::Clock::instance().now() + ACK_TIMEOUT;
            while (hal::Clock::instance().now() < deadline) {
                bool complete = true;
                targets.for_each([&complete, generation](cpu::CpuId target) noexcept {
                    complete &= acknowledgements[target.value].load(std::memory_order_acquire) >=
                                generation;
                });
                if (complete)
                    return;
                hal::cpu_relax();
            }

            targets.for_each([generation](cpu::CpuId target) noexcept {
                const auto acknowledged =
                    acknowledgements[target.value].load(std::memory_order_acquire);
                if (acknowledged < generation)
                    kernel::log::panic(
                        "TLB shootdown acknowledgement timeout: cpu={}, generation={}, ack={}",
                        target.value, generation, acknowledged);
            });
            kernel::log::panic("TLB shootdown acknowledgement timeout: generation={}", generation);
        }

        [[noreturn]] void ipi_failure(cpu::CpuId target) noexcept {
            kernel::log::panic("TLB shootdown IPI 发送失败: cpu={}", target.value);
        }
    }  // namespace detail::shootdown

    void init_shootdown() noexcept {
        if (detail::shootdown::initialized.load(std::memory_order_acquire))
            return;
        smp::set_tlb_handler(handle_shootdown);
        detail::shootdown::initialized.store(true, std::memory_order_release);
    }

    void shootdown(memory::RootBinding binding, addr_t address, size_t pages,
                   TlbInvalidationKind kind) noexcept {
        if (!detail::shootdown::initialized.load(std::memory_order_acquire) ||
            !cpu::topology().ready())
        {
            hal::PtOps::flush_tlb();
            return;
        }

        const cpu::CpuId current = cpu::current_id();
        const auto targets       = cpu::topology().snapshot().online;
        if (!targets.test(current))
            __builtin_trap();

        auto state      = detail::shootdown::coordinator.lock();
        const u64_t old = state->request.generation;
        if (old == std::numeric_limits<u64_t>::max())
            kernel::log::panic("TLB shootdown generation 溢出");

        const u64_t generation = old + 1;
        state->request         = TlbShootdownSnapshot{
                    .generation = generation,
                    .targets    = targets,
                    .kind       = kind,
                    .binding    = binding,
                    .address    = address,
                    .pages      = pages,
        };

        // PTE 的 release 写入发生在调用本函数前。generation 的 release 发布让 handler 在
        // flush 前观察到本次完整请求；coordinator 在所有 ack 前不会覆写该存储。
        hal::PtOps::flush_tlb();
        detail::shootdown::acknowledge(current, generation);
        detail::shootdown::published_generation.store(generation, std::memory_order_release);

        const auto remote_targets = targets.without(current);
        remote_targets.for_each([](cpu::CpuId target) noexcept {
            auto requested = smp::request(target, IpiReason::TLB_SHOOTDOWN);
            if (!requested)
                detail::shootdown::ipi_failure(target);
        });
        detail::shootdown::wait_acks(remote_targets, generation);
    }

    void handle_shootdown() noexcept {
        const cpu::CpuId current = cpu::current_id();
        const u64_t generation =
            detail::shootdown::published_generation.load(std::memory_order_acquire);
        if (generation == 0 || !current.valid())
            __builtin_trap();

        // generation 的 acquire 保证 PTE 修改已在本地 flush 前可见。handler 不取得 coordinator
        // 或 PageTable 锁；发起者正持有前者等待 acknowledgement，任何锁依赖都会形成死锁。
        hal::PtOps::flush_tlb();
        detail::shootdown::acknowledge(current, generation);
    }

    TlbShootdownSnapshot shootdown_snapshot() noexcept {
        auto state = detail::shootdown::coordinator.lock();
        return state->request;
    }

    u64_t acked_gen(cpu::CpuId id) noexcept {
        return id.valid()
                   ? detail::shootdown::acknowledgements[id.value].load(std::memory_order_acquire)
                   : 0;
    }
}  // namespace smp
