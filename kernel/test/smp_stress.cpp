/**
 * @file smp_stress.cpp
 * @brief SMP WorkQueue、timer completion 和内核对象分配压力 selftest。
 */

#include <arch/timer.h>
#include <cpu/topology.h>
#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/slab/heap.h>
#include <obj/cspace.h>
#include <obj/mem_seg.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <sustcore/capability.h>
#include <test/cases.h>

#include <atomic>
#include <cstddef>

namespace kernel::test::cases {
    namespace {
        constexpr size_t OBJECT_ROUNDS = 12;

        void log_smp_diag(const char *reason) noexcept {
            const auto snapshot = cpu::topology().snapshot();
            kernel::log::info("SMP diagnostic ({}): possible={}, started={}, online={}", reason,
                              snapshot.possible.count(), snapshot.started.count(),
                              snapshot.online.count());
            snapshot.online.for_each([](cpu::CpuId id) noexcept {
                const auto ipi = smp::stats(id);
                kernel::log::info("  cpu={} ipi(posted={},dispatch={},resched={},tlb={},timer={})",
                                  id.value, ipi.posted, ipi.dispatches, ipi.reschedules,
                                  ipi.tlb_shootdowns, ipi.timer_deadlines);
            });
        }

        struct ObjectProducerState final {
            std::atomic<bool> failed{false};
            std::atomic<bool> completed{false};
            units::time deadline{};
            size_t rounds = OBJECT_ROUNDS;
        };

        void object_producer(void *opaque) noexcept {
            auto *state = static_cast<ObjectProducerState *>(opaque);
            if (state == nullptr) {
                kernel::log::panic("SMP allocator producer context 无效");
            }

            for (size_t round = 0; round < state->rounds; ++round) {
                if (hal::Clock::instance().now() >= state->deadline) {
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                if (!hal::irq_enabled()) {
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                auto stack = task::KernelStack::create(2 * PAGE_SIZE);
                if (!stack) {
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                auto small = memory::alloc(32, alignof(std::max_align_t));
                if (!small) {
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                memory::dealloc(*small);

                auto segment = memory::MemSeg::create(PAGE_SIZE * 3 + 13);
                auto space   = cap::CSpace::create();
                if (!segment || !space) {
                    if (space)
                        delete *space;
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                std::byte payload[PAGE_SIZE]{};
                if (!(*segment)->write(PAGE_SIZE - 7, payload, sizeof(payload))) {
                    delete *space;
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                constexpr u64_t RIGHTS = cap::RIGHT_READ | cap::RIGHT_WRITE | cap::RIGHT_COPY |
                                         cap::RIGHT_MINT | cap::RIGHT_REVOKE;
                auto token = (*space)->install(**segment, RIGHTS);
                if (!token ||
                    !(*space)->resolve(*token, cap::ObjectType::MEMORY, cap::RIGHT_READ) ||
                    !(*space)->delete_cap(*token))
                {
                    delete *space;
                    state->failed.store(true, std::memory_order_release);
                    return;
                }
                delete *space;
                segment->reset();
            }
            state->completed.store(true, std::memory_order_release);
        }

        [[nodiscard]] bool wait_until(const std::atomic<bool> &flag,
                                      units::time deadline) noexcept {
            while (!flag.load(std::memory_order_acquire)) {
                if (hal::Clock::instance().now() >= deadline)
                    return false;
                scheduler::yield();
            }
            return true;
        }
    }  // namespace

    void run_alloc_stress(Context &) noexcept {
        const auto online           = cpu::topology().snapshot().online;
        const size_t baseline_pages = memory::buddy()->free_pages();
        ObjectProducerState states[cpu::MAX_CPUS]{};
        cap::KObjectRef<task::Thread> workers[cpu::MAX_CPUS]{};
        // LABOOT 的当前 BSP-only 环境在 LoongArch 上执行页分配、跨页拷贝和 CSpace
        // 回收明显慢于 RISC-V；仍保持有界，但给该固件路径足够的完成窗口。
        const auto deadline = hal::Clock::instance().now() + (online.count() == 1 ? 45_s : 20_s);

        online.for_each([&](cpu::CpuId id) noexcept {
            states[id.value].deadline = deadline;
            states[id.value].rounds   = online.count() == 1 ? 4 : OBJECT_ROUNDS;
            auto worker = task::Thread::create_kernel(task::kernel_proc(), object_producer,
                                                      &states[id.value]);
            if (!worker || !scheduler::attach(**worker, scheduler::Placement::Pinned(id)))
                kernel::test::fail("allocator/object producer 发布失败");
            workers[id.value] = std::move(*worker);
        });

        online.for_each([&](cpu::CpuId id) noexcept {
            if (!wait_until(states[id.value].completed, deadline)) {
                log_smp_diag("allocator_and_object_stress timeout");
                kernel::test::fail("allocator/object stress watchdog 超时");
            }
            kernel::test::require(!states[id.value].failed.load(std::memory_order_acquire),
                                  "allocator/object stress worker 失败");
        });

        for (size_t index = 0; index < cpu::MAX_CPUS; ++index) {
            if (!workers[index])
                continue;
            while (!workers[index]->exited()) {
                if (hal::Clock::instance().now() >= deadline) {
                    log_smp_diag("allocator_and_object_stress join timeout");
                    kernel::test::fail("allocator/object worker 退出 watchdog 超时");
                }
                scheduler::yield();
            }
            workers[index].reset();
        }
        const size_t after_pages = memory::buddy()->free_pages();
        kernel::log::info("allocator/object stress pages: baseline={}, after={}", baseline_pages,
                          after_pages);
        kernel::test::require(after_pages + 16 >= baseline_pages,
                              "allocator/object stress 物理页统计超出允许基线范围");
    }
}  // namespace kernel::test::cases
