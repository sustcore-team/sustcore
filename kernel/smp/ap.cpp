/**
 * @file ap.cpp
 * @brief 次级 CPU 启动状态机的无锁发布实现。
 */

#include <arch/cpu.h>
#include <arch/interrupt.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <cpu/local.h>
#include <log.h>
#include <memory/virtual/kernel/vm.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <smp/ap.h>
#include <synchronized.h>
#include <tay/array.h>
#include <tay/spinlock.h>
#include <timer/deadline.h>

namespace smp {
    namespace detail::ap_manager {
        constinit tay::static_array<boot::smp::ApBootRes, cpu::MAX_CPUS> entries{};
        constinit std::atomic<u64_t> ready_bits{0};
        constinit std::atomic<u64_t> committed_bits{0};
        constinit std::atomic<bool> committed_flag{false};
        constinit tay::spinlock transition_lock{};
        constinit ApManager manager_instance{};

        [[nodiscard]] boot::smp::ApBootRes *entry(cpu::CpuId id) noexcept {
            return id.valid() ? &entries[id.value] : nullptr;
        }

        [[nodiscard]] const boot::smp::ApBootRes *entry_const(cpu::CpuId id) noexcept {
            return id.valid() ? &entries[id.value] : nullptr;
        }

        [[nodiscard]] boot::smp::StartState state_of(const boot::smp::ApBootRes &value) noexcept {
            return static_cast<boot::smp::StartState>(value.state.load(std::memory_order_acquire));
        }

        [[nodiscard]] bool transition(boot::smp::ApBootRes &value, boot::smp::StartState expected,
                                      boot::smp::StartState next) noexcept {
            u32_t observed = static_cast<u32_t>(expected);
            return value.state.compare_exchange_strong(observed, static_cast<u32_t>(next),
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire);
        }

        [[nodiscard]] bool try_set_cpu_state(cpu::CpuId id, cpu::CpuState expected,
                                             cpu::CpuState next) noexcept {
            while (cpu::topology().state(id) == expected) {
                if (cpu::topology().transition(id, expected, next))
                    return true;
                hal::cpu_relax();
            }
            return cpu::topology().state(id) == next;
        }

        [[nodiscard]] cpu::CpuSet make_set(u64_t bits) noexcept {
            cpu::CpuSet result;
            for (size_t index = 0; index < cpu::MAX_CPUS; ++index)
                if ((bits & (u64_t{1} << index)) != 0)
                    static_cast<void>(result.set(cpu::CpuId{static_cast<u32_t>(index)}));
            return result;
        }
    }  // namespace detail::ap_manager

    tay::expected<void, tay::error_code> ApManager::prepare(cpu::CpuId id, cpu::CpuHwId hw_id,
                                                            PhyAddr root_pt, addr_t stack_top,
                                                            addr_t cpu_local,
                                                            addr_t entry_pc) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || id.value == 0 || root_pt.arith() == 0 || stack_top == 0 ||
            cpu_local == 0 || entry_pc == 0 || committed())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        if (!detail::ap_manager::transition(*slot, boot::smp::StartState::OFFLINE,
                                            boot::smp::StartState::PREPARED))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto &args      = slot->arguments;
        const u32_t gen = cpu::topology().generation(id);
        args            = boot::smp::ApBootArgs{
                       .magic       = boot::smp::AP_BOOT_MAGIC,
                       .abi_version = boot::smp::AP_BOOT_ABI_VERSION,
                       .cpu_id      = id,
                       .hw_id       = hw_id,
                       .root_pt     = root_pt,
                       .stack_top   = stack_top,
                       .cpu_local   = cpu_local,
                       .entry_pc    = entry_pc,
                       .gen         = gen,
        };
        slot->gen.store(gen, std::memory_order_relaxed);
        slot->release_gate.store(false, std::memory_order_relaxed);
        slot->online_ack.store(false, std::memory_order_relaxed);
        return {};
    }

    tay::expected<void, tay::error_code> ApManager::start(cpu::CpuId id,
                                                          boot::smp::StartAp starter) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || starter == nullptr || state(id) != boot::smp::StartState::PREPARED)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (!detail::ap_manager::try_set_cpu_state(id, cpu::CpuState::POSSIBLE,
                                                   cpu::CpuState::STARTED))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (!detail::ap_manager::transition(*slot, boot::smp::StartState::PREPARED,
                                            boot::smp::StartState::STARTING))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        const auto physical = PhyAddr(reinterpret_cast<addr_t>(&slot->arguments) - KVA_START);
        auto result         = starter(slot->arguments.hw_id, physical);
        if (!result) {
            static_cast<void>(detail::ap_manager::transition(*slot, boot::smp::StartState::STARTING,
                                                             boot::smp::StartState::FAILED));
            static_cast<void>(detail::ap_manager::try_set_cpu_state(id, cpu::CpuState::STARTED,
                                                                    cpu::CpuState::FAILED));
            return result;
        }
        return {};
    }

    bool ApManager::publish_started(const boot::smp::ApBootArgs &arguments) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(arguments.cpu_id);
        if (slot == nullptr || arguments.magic != boot::smp::AP_BOOT_MAGIC ||
            arguments.abi_version != boot::smp::AP_BOOT_ABI_VERSION ||
            slot->gen.load(std::memory_order_acquire) != arguments.gen ||
            slot->arguments.hw_id != arguments.hw_id)
            return false;
        return detail::ap_manager::transition(*slot, boot::smp::StartState::STARTING,
                                              boot::smp::StartState::EARLY_ONLINE);
    }

    bool ApManager::publish_ready(cpu::CpuId id, u32_t gen) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || committed() || slot->gen.load(std::memory_order_acquire) != gen)
            return false;
        if (!detail::ap_manager::try_set_cpu_state(id, cpu::CpuState::STARTED,
                                                   cpu::CpuState::READY))
            return false;
        if (!detail::ap_manager::transition(*slot, boot::smp::StartState::EARLY_ONLINE,
                                            boot::smp::StartState::READY))
            return false;
        detail::ap_manager::ready_bits.fetch_or(u64_t{1} << id.value, std::memory_order_release);
        return true;
    }

    bool ApManager::fail(cpu::CpuId id, u32_t gen) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || generation(id) != gen || committed())
            return false;
        const auto current = state(id);
        if (current != boot::smp::StartState::STARTING &&
            current != boot::smp::StartState::EARLY_ONLINE &&
            current != boot::smp::StartState::READY)
            return false;
        if (!detail::ap_manager::transition(*slot, current, boot::smp::StartState::FAILED))
            return false;
        const auto topology_state =
            current == boot::smp::StartState::READY ? cpu::CpuState::READY : cpu::CpuState::STARTED;
        static_cast<void>(
            detail::ap_manager::try_set_cpu_state(id, topology_state, cpu::CpuState::FAILED));
        detail::ap_manager::ready_bits.fetch_and(~(u64_t{1} << id.value),
                                                 std::memory_order_release);
        return true;
    }

    bool ApManager::abandon(cpu::CpuId id, u32_t gen) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || generation(id) != gen || committed())
            return false;
        const auto current = state(id);
        if (current != boot::smp::StartState::STARTING &&
            current != boot::smp::StartState::EARLY_ONLINE)
            return false;
        if (!detail::ap_manager::transition(*slot, current, boot::smp::StartState::ABANDONED))
            return false;
        static_cast<void>(detail::ap_manager::try_set_cpu_state(id, cpu::CpuState::STARTED,
                                                                cpu::CpuState::FAILED));
        return true;
    }

    bool ApManager::commit_ready_set() noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        if (detail::ap_manager::committed_flag.load(std::memory_order_acquire))
            return false;
        const u64_t bits =
            detail::ap_manager::ready_bits.load(std::memory_order_acquire) | u64_t{1};
        detail::ap_manager::committed_bits.store(bits, std::memory_order_release);
        for (size_t index = 1; index < cpu::MAX_CPUS; ++index)
            if ((bits & (u64_t{1} << index)) != 0)
                detail::ap_manager::entries[index].release_gate.store(true,
                                                                      std::memory_order_release);
        // committed 的 release 发布排在 immutable snapshot 与全部 gate 之后，AP acquire
        // 观察到它时必然可以使用完整在线集合。本函数仅由 BSP smp-init Thread 调用一次。
        detail::ap_manager::committed_flag.store(true, std::memory_order_release);
        return true;
    }

    bool ApManager::released(cpu::CpuId id, u32_t gen) const noexcept {
        const auto *slot = detail::ap_manager::entry_const(id);
        return slot != nullptr && generation(id) == gen && committed() &&
               slot->release_gate.load(std::memory_order_acquire);
    }

    bool ApManager::committed() const noexcept {
        return detail::ap_manager::committed_flag.load(std::memory_order_acquire);
    }

    bool ApManager::publish_online(cpu::CpuId id, u32_t gen) noexcept {
        kernel::lock_guard<tay::spinlock> guard(detail::ap_manager::transition_lock);
        auto *slot = detail::ap_manager::entry(id);
        if (slot == nullptr || generation(id) != gen || !committed() ||
            !slot->release_gate.load(std::memory_order_acquire))
            return false;
        if (!detail::ap_manager::try_set_cpu_state(id, cpu::CpuState::READY,
                                                   cpu::CpuState::ONLINE))
            return false;
        if (!detail::ap_manager::transition(*slot, boot::smp::StartState::READY,
                                            boot::smp::StartState::ONLINE))
            return false;
        slot->online_ack.store(true, std::memory_order_release);
        return true;
    }

    boot::smp::StartState ApManager::state(cpu::CpuId id) const noexcept {
        const auto *slot = detail::ap_manager::entry_const(id);
        return slot == nullptr ? boot::smp::StartState::OFFLINE
                               : detail::ap_manager::state_of(*slot);
    }

    u32_t ApManager::generation(cpu::CpuId id) const noexcept {
        const auto *slot = detail::ap_manager::entry_const(id);
        return slot == nullptr ? 0 : slot->gen.load(std::memory_order_acquire);
    }

    const boot::smp::ApBootRes *ApManager::boot_res(cpu::CpuId id) const noexcept {
        const auto *slot = detail::ap_manager::entry_const(id);
        return slot;
    }

    boot::smp::ApBootRes *ApManager::boot_res(cpu::CpuId id) noexcept {
        auto *slot = detail::ap_manager::entry(id);
        return slot;
    }

    cpu::CpuSet ApManager::ready_set() const noexcept {
        return detail::ap_manager::make_set(
            detail::ap_manager::ready_bits.load(std::memory_order_acquire));
    }

    cpu::CpuSet ApManager::committed_set() const noexcept {
        return detail::ap_manager::make_set(
            detail::ap_manager::committed_bits.load(std::memory_order_acquire));
    }

    ApManager &ap_manager() noexcept {
        return detail::ap_manager::manager_instance;
    }

    extern "C" [[noreturn]] void ap_main(const boot::smp::ApBootArgs *arguments) noexcept {
        if (arguments == nullptr || arguments->magic != boot::smp::AP_BOOT_MAGIC ||
            arguments->abi_version != boot::smp::AP_BOOT_ABI_VERSION ||
            arguments->cpu_id.value == 0 || !arguments->cpu_id.valid())
            __builtin_trap();

        auto *storage  = cpu::try_slot(arguments->cpu_id);
        auto *boot_res = ap_manager().boot_res(arguments->cpu_id);
        if (storage == nullptr || boot_res == nullptr ||
            boot_res->state.load(std::memory_order_acquire) !=
                static_cast<u32_t>(boot::smp::StartState::STARTING) ||
            storage->local.hw_id != arguments->hw_id)
            __builtin_trap();

        // trampoline 只建立可执行 C++ 的最小环境；以下本地硬件与 CpuLocal 只能由 AP 自己
        // 初始化，BSP 不写远端 CPU 寄存器。
        cpu::bind_storage(*storage, arguments->cpu_id, arguments->hw_id, arguments->stack_top);
        if (!ap_manager().publish_started(*arguments))
            __builtin_trap();

        hal::set_trap_vectors();
        memory::kernel_vm().activate();
        hal::init_ipi();
        // panic 可能在 AP 完成本地 IPI 初始化前发布 STOP mailbox；架构初始化会清掉
        // 硬件 pending，因此此处必须主动消费一次软件 mailbox。STARTED 阶段只有 STOP
        // 能被发送到该 CPU，STOP 分支也不会依赖尚未初始化的 scheduler。
        dispatch_ipi();
        auto &clock = hal::Clock::instance();
        clock.initialize_local();
        auto &deadlines = kernel::timer::init_deadline_mux(arguments->cpu_id, clock);

        auto bootstrap  = task::Thread::adopt_current(task::kernel_proc());
        auto &scheduler = scheduler::prepare_cpu(arguments->cpu_id);
        if (auto initialized = scheduler.initialize(bootstrap); !initialized)
            kernel::log::panic("AP scheduler 初始化失败: cpu={}, error={}", arguments->cpu_id.value,
                               initialized.error());
        if (auto installed = scheduler.set_preempt_sink(deadlines.preemption_sink()); !installed)
            kernel::log::panic("AP scheduler deadline sink 安装失败: cpu={}, error={}",
                               arguments->cpu_id.value, installed.error());
        scheduler.become_idle();
        if (!ap_manager().publish_ready(arguments->cpu_id, arguments->gen))
            __builtin_trap();

        while (!ap_manager().released(arguments->cpu_id, arguments->gen)) hal::cpu_relax();
        if (!ap_manager().publish_online(arguments->cpu_id, arguments->gen))
            __builtin_trap();

        hal::sti();
        while (true) hal::wfi();
    }
}  // namespace smp
