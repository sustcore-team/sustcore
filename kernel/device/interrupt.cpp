/**
 * @file interrupt.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 运行期中断处理器注册与分发。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/cpu.h>
#include <device/catalog.h>
#include <device/interrupt.h>
#include <synchronized.h>
#include <tay/lock.h>
#include <tay/rcu.h>
#include <tay/static_vector.h>
#include <tay/unique_ptr.h>

#include <atomic>
#include <new>
#include <utility>

namespace device::interrupt {
    namespace detail {
        constexpr size_t MAX_TRAP_HANDLERS      = 32;
        constexpr size_t MAX_IRQ_DOMAINS        = 8;
        constexpr size_t MAX_IRQ_HANDLERS       = 128;
        constexpr size_t MAX_CLAIMS_PER_TRAP    = 256;
        constexpr size_t MAX_CASCADE_DEPTH      = 8;
        constexpr size_t MAX_CASCADE_BINDINGS   = 32;
        constexpr size_t MAX_RETIRED_REGISTRIES = 8;

        struct Slot {
            TrapLine line{};
            Handler handler   = nullptr;
            void *handler_ctx = nullptr;
            u32_t gen         = 0;
        };

        struct Registry : tay::rcu_retired_node {
            struct IrqSlot {
                IrqLine line{};
                IrqHandler handler = nullptr;
                void *handler_ctx  = nullptr;
                u32_t gen          = 0;
            };
            struct DomainSlot {
                IrqDomain *domain = nullptr;
                bool root         = false;
            };
            struct CascadeSlot {
                IrqDomain *parent = nullptr;
                u32_t parent_irq  = 0;
                IrqDomain *child  = nullptr;
            };

            constexpr Registry() noexcept = default;

            constexpr Registry(const Registry &other) noexcept
                : tay::rcu_retired_node{},
                  slots(other.slots),
                  irq_slots(other.irq_slots),
                  domains(other.domains),
                  cascades(other.cascades),
                  next_gen(other.next_gen) {}

            tay::static_vector<Slot, MAX_TRAP_HANDLERS> slots;
            tay::static_vector<IrqSlot, MAX_IRQ_HANDLERS> irq_slots;
            tay::static_vector<DomainSlot, MAX_IRQ_DOMAINS> domains;
            tay::static_vector<CascadeSlot, MAX_CASCADE_BINDINGS> cascades;
            u32_t next_gen = 1;
        };

        struct FlowState {
            IrqCascadeFrame frames[MAX_CASCADE_DEPTH]{};
            size_t claims       = 0;
            bool claimed_any    = false;
            bool all_handled    = true;
            u32_t next_claim_id = 1;
        };

        class IrqRcuPolicy final {
        public:
            void enter_read() noexcept {
                readers_.fetch_add(1, std::memory_order_seq_cst);
            }

            void exit_read() noexcept {
                readers_.fetch_sub(1, std::memory_order_seq_cst);
            }

            void synchronize() noexcept {
                while (readers_.load(std::memory_order_seq_cst) != 0) hal::cpu_relax();
            }

        private:
            static_assert(std::atomic<u32_t>::is_always_lock_free,
                          "IRQ RCU requires lock-free reader counters");

            // TODO: SMP 高并发中断启用后迁移为 per-CPU epoch/QSBR，避免共享 cache line。
            std::atomic<u32_t> readers_{0};
        };

        constinit Registry initial_registry{};
        constinit std::atomic<Registry *> published_registry{&initial_registry};
        constinit tay::spinlock registry_writer_lock;
        constinit tay::rcu_domain<IrqRcuPolicy, MAX_RETIRED_REGISTRIES> registry_rcu;

        [[nodiscard]] Registry *current_registry() noexcept {
            // reader counter 与快照指针共享 seq_cst 全序；能观察旧快照的 reader
            // 必定先计入 counter，writer 只能在该 reader 离开后观察到零值。
            return published_registry.load(std::memory_order_seq_cst);
        }

        void reclaim_registry(tay::rcu_retired_node &node) noexcept {
            delete static_cast<Registry *>(&node);
        }

        [[nodiscard]] tay::expected<void, tay::error_code> publish_registry(
            Registry &current, tay::unique_ptr<Registry> next) noexcept {
            if (&current != &initial_registry)
                TAY_TRYV(registry_rcu.retire(current, reclaim_registry));

            published_registry.store(next.get(), std::memory_order_seq_cst);
            static_cast<void>(next.release());
            registry_rcu.synchronize();
            return {};
        }

        template <class Mutator>
        [[nodiscard]] tay::expected<void, tay::error_code> mutate_registry(
            Mutator &&mutator) noexcept {
            kernel::lock_guard<tay::spinlock> held{registry_writer_lock};
            auto *current = current_registry();
            auto *copy    = new (std::nothrow) Registry(*current);
            if (copy == nullptr)
                return tay::Err(tay::error_code::OUT_OF_MEMORY);
            tay::unique_ptr<Registry> next(copy);

            TAY_TRYV(std::forward<Mutator>(mutator)(*next));
            return publish_registry(*current, std::move(next));
        }

        [[nodiscard]] bool domain_registered(const Registry &state, IrqDomain &domain) noexcept {
            for (const auto &slot : state.domains)
                if (slot.domain == &domain)
                    return true;
            return false;
        }

        [[nodiscard]] tay::expected<u32_t, tay::error_code> alloc_gen(Registry &registry) noexcept {
            if (registry.next_gen == 0 || registry.next_gen == UINT32_MAX)
                return tay::Err(tay::error_code::OVERFLOW_ERROR);
            return registry.next_gen++;
        }

        [[nodiscard]] bool binding_for(const Registry &registry, IrqDomain &parent,
                                       u32_t parent_irq, IrqBinding &binding) noexcept {
            for (const auto &cascade : registry.cascades) {
                if (cascade.parent != &parent || cascade.parent_irq != parent_irq)
                    continue;
                binding.kind         = IrqBindingKind::CASCADE;
                binding.child_domain = cascade.child;
                binding.line         = IrqLine{.ctrl = parent.ctrl(), .hw_irq = parent_irq};
                return true;
            }
            return false;
        }

        [[nodiscard]] IrqDomain *root_domain(const Registry &registry) noexcept {
            IrqDomain *root = nullptr;
            for (const auto &slot : registry.domains) {
                if (!slot.root)
                    continue;
                if (root != nullptr)
                    return nullptr;
                root = slot.domain;
            }
            return root;
        }

        [[nodiscard]] bool ack_claim(const IrqClaim &claim) noexcept {
            if (claim.domain == nullptr || claim.hw_irq == 0 || claim.gen == 0)
                return false;
            const auto acknowledged = claim.domain->ack(claim);
            return acknowledged.has_value();
        }

        [[nodiscard]] bool finish_claim(const IrqClaim &claim) noexcept {
            if (claim.domain == nullptr || claim.hw_irq == 0 || claim.gen == 0)
                return false;

            // 即使 EOI 失败也尝试 complete，避免把控制器永久留在 in-service 状态。
            const auto eoi       = claim.domain->eoi(claim);
            const auto completed = claim.domain->complete(claim);
            return eoi.has_value() && completed.has_value();
        }

        [[nodiscard]] bool dispatch_domain(const Registry &registry, IrqDomain &domain,
                                           size_t depth, FlowState &state) noexcept {
            if (depth >= MAX_CASCADE_DEPTH) {
                state.all_handled = false;
                return false;
            }

            while (true) {
                auto claimed = domain.claim();
                if (!claimed) {
                    state.all_handled = false;
                    return false;
                }
                if (*claimed == 0)
                    return true;

                state.claimed_any = true;
                const IrqClaim claim{
                    .domain = &domain, .hw_irq = *claimed, .gen = state.next_claim_id++};
                if (++state.claims > MAX_CLAIMS_PER_TRAP) {
                    (void)ack_claim(claim);
                    (void)finish_claim(claim);
                    state.all_handled = false;
                    return false;
                }

                if (!ack_claim(claim)) {
                    (void)finish_claim(claim);
                    state.all_handled = false;
                    return false;
                }

                IrqBinding binding{};
                if (binding_for(registry, domain, *claimed, binding)) {
                    const size_t child_claims = state.claims;
                    state.frames[depth] =
                        IrqCascadeFrame{.parent = claim, .child_domain = binding.child_domain};
                    const bool child_ok =
                        dispatch_domain(registry, *binding.child_domain, depth + 1, state);
                    state.frames[depth] = IrqCascadeFrame{};
                    if (state.claims == child_claims)
                        state.all_handled = false;
                    if (!finish_claim(claim)) {
                        state.all_handled = false;
                        return false;
                    }
                    if (!child_ok)
                        return false;
                    continue;
                }

                const IrqLine line{.ctrl = domain.ctrl(), .hw_irq = *claimed};
                IrqHandler handler = nullptr;
                void *handler_ctx  = nullptr;
                for (const auto &slot : registry.irq_slots) {
                    if (slot.line == line) {
                        handler     = slot.handler;
                        handler_ctx = slot.handler_ctx;
                        break;
                    }
                }
                if (handler != nullptr)
                    handler(handler_ctx, line);
                else
                    state.all_handled = false;

                if (!finish_claim(claim)) {
                    state.all_handled = false;
                    return false;
                }
            }
        }

        [[nodiscard]] bool source_for(const hal::TrapInfo &info, IrqSource &source) noexcept {
            switch (info.kind) {
                case hal::TrapKind::TIMER:       source = IrqSource::TIMER; return true;
                case hal::TrapKind::SOFTWARE:    source = IrqSource::SOFTWARE; return true;
                case hal::TrapKind::EXTERNAL:    source = IrqSource::EXTERNAL; return true;
                case hal::TrapKind::SYNCHRONOUS: return false;
            }
            return false;
        }

        [[nodiscard]] DispatchResult dispatch_external(const Registry &registry) noexcept {
            auto *root = root_domain(registry);
            if (root == nullptr)
                return DispatchResult::UNHANDLED;

            FlowState state{};
            (void)dispatch_domain(registry, *root, 0, state);
            return state.claimed_any && state.all_handled ? DispatchResult::HANDLED
                                                          : DispatchResult::UNHANDLED;
        }
    }  // namespace detail

    tay::expected<IrqLine, tay::error_code> resolve(FwId ctrl, u32_t hw_irq) noexcept {
        if (!ctrl.valid() || catalog().find_irq_ctrl(ctrl) == nullptr)
            return tay::Err(tay::error_code::OUT_OF_RANGE);

        auto read_guard      = detail::registry_rcu.read_lock();
        const auto &registry = *detail::current_registry();
        static_cast<void>(read_guard);
        for (const auto &slot : registry.domains) {
            if (slot.domain->ctrl() == ctrl) {
                if (hw_irq == 0 || hw_irq > slot.domain->line_count())
                    return tay::Err(tay::error_code::OUT_OF_RANGE);
                return IrqLine{.ctrl = ctrl, .hw_irq = hw_irq};
            }
        }
        return tay::Err(tay::error_code::OUT_OF_RANGE);
    }

    tay::expected<TrapSub, tay::error_code> subscribe(TrapLine line, Handler handler,
                                                      void *handler_ctx) noexcept {
        if (handler == nullptr)
            return tay::Err(tay::error_code::NULLPTR);

        u32_t gen = 0;
        TAY_TRYV(detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                for (const auto &slot : registry.slots)
                    if (slot.line == line)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);

                gen = TAY_TRY(detail::alloc_gen(registry));
                return registry.slots.push_back(detail::Slot{
                    .line = line, .handler = handler, .handler_ctx = handler_ctx, .gen = gen});
            }));
        return TrapSub{.line = line, .gen = gen};
    }

    tay::expected<void, tay::error_code> unsubscribe(TrapSub subscription) noexcept {
        return detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                for (auto it = registry.slots.begin(); it != registry.slots.end(); ++it) {
                    if (it->line == subscription.line && it->gen == subscription.gen)
                        return registry.slots.erase(it).transform([](auto) {});
                }
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            });
    }

    tay::expected<void, tay::error_code> register_domain(IrqDomain &domain) noexcept {
        return detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                if (!domain.ctrl().valid() || domain.line_count() == 0)
                    return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (const auto &existing : registry.domains)
                    if (existing.domain->ctrl() == domain.ctrl())
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (const auto &existing : registry.domains)
                    if (existing.root)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                return registry.domains.push_back(
                    detail::Registry::DomainSlot{.domain = &domain, .root = true});
            });
    }

    tay::expected<void, tay::error_code> register_cascade(IrqDomain &parent, u32_t parent_irq,
                                                          IrqDomain &child) noexcept {
        if (&parent == &child || !child.ctrl().valid() || child.line_count() == 0 ||
            parent_irq == 0 || parent_irq > parent.line_count())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        return detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                if (!detail::domain_registered(registry, parent) ||
                    detail::domain_registered(registry, child))
                    return tay::Err(tay::error_code::OUT_OF_RANGE);
                for (const auto &existing : registry.domains)
                    if (existing.domain->ctrl() == child.ctrl())
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (const auto &slot : registry.irq_slots)
                    if (slot.line.ctrl == parent.ctrl() && slot.line.hw_irq == parent_irq)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (const auto &cascade : registry.cascades)
                    if (cascade.parent == &parent && cascade.parent_irq == parent_irq)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);

                TAY_TRYV(registry.domains.push_back(
                    detail::Registry::DomainSlot{.domain = &child, .root = false}));
                return registry.cascades.push_back(detail::Registry::CascadeSlot{
                    .parent     = &parent,
                    .parent_irq = parent_irq,
                    .child      = &child,
                });
            });
    }

    tay::expected<void, tay::error_code> unregister_domain(IrqDomain &domain) noexcept {
        return detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                for (const auto &slot : registry.irq_slots)
                    if (slot.line.ctrl == domain.ctrl())
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (const auto &cascade : registry.cascades)
                    if (cascade.parent == &domain || cascade.child == &domain)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                for (auto it = registry.domains.begin(); it != registry.domains.end(); ++it) {
                    if (it->domain == &domain)
                        return registry.domains.erase(it).transform([](auto) {});
                }
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            });
    }

    tay::expected<IrqSub, tay::error_code> subscribe_irq(IrqLine line, IrqHandler handler,
                                                         void *handler_ctx) noexcept {
        if (handler == nullptr)
            return tay::Err(tay::error_code::NULLPTR);

        u32_t gen = 0;
        TAY_TRYV(detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                for (const auto &slot : registry.irq_slots)
                    if (slot.line == line)
                        return tay::Err(tay::error_code::INVALID_ARGUMENT);
                gen = TAY_TRY(detail::alloc_gen(registry));
                return registry.irq_slots.push_back(detail::Registry::IrqSlot{
                    .line        = line,
                    .handler     = handler,
                    .handler_ctx = handler_ctx,
                    .gen         = gen,
                });
            }));
        return IrqSub{.line = line, .gen = gen};
    }

    tay::expected<void, tay::error_code> unsubscribe_irq(IrqSub subscription) noexcept {
        return detail::mutate_registry(
            [&](detail::Registry &registry) noexcept -> tay::expected<void, tay::error_code> {
                for (auto it = registry.irq_slots.begin(); it != registry.irq_slots.end(); ++it) {
                    if (it->line == subscription.line && it->gen == subscription.gen)
                        return registry.irq_slots.erase(it).transform([](auto) {});
                }
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            });
    }

    DispatchResult dispatch(const hal::TrapInfo &info) noexcept {
        cpu::irq_context_guard interrupt_context;

        IrqSource source{};
        if (!detail::source_for(info, source))
            return DispatchResult::UNHANDLED;

        auto read_guard      = detail::registry_rcu.read_lock();
        const auto &registry = *detail::current_registry();
        static_cast<void>(read_guard);
        if (source == IrqSource::EXTERNAL)
            return detail::dispatch_external(registry);

        Handler handler   = nullptr;
        void *handler_ctx = nullptr;
        const TrapLine line =
            source == IrqSource::TIMER ? TIMER_LINE : TrapLine{.source = source, .code = info.code};
        for (const auto &slot : registry.slots) {
            if (slot.line == line) {
                handler     = slot.handler;
                handler_ctx = slot.handler_ctx;
                break;
            }
        }
        if (handler == nullptr)
            return DispatchResult::UNHANDLED;

        handler(handler_ctx, TrapEvent{.line = line, .trap = info});
        return DispatchResult::HANDLED;
    }
}  // namespace device::interrupt
