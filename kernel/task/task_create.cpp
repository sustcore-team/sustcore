/**
 * @file task_create.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief task 创建、装配与运行期切换
 * @version alpha-1.0.0
 * @date 2026-06-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/permission.h>
#include <device/model.h>
#include <elf.h>
#include <env.h>
#include <guard.h>
#include <kinit.h>
#include <logger.h>
#include <object/memory.h>
#include <object/task.h>
#include <sus/raii.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <vfs/vfs.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace task {
    namespace {
        extern "C" [[noreturn]] void new_utask_trampoline();

        void init_user_context(Context *ctx, void *entrypoint, void *stack_top,
                               void *kstack_top,
                               umb_t linuxproc_entrypoint = 0) noexcept {
            assert(ctx != nullptr);
            *ctx = {};
            ctx->setup_regs<SetupCase::USER_THREAD>();
            ctx->pc()         = reinterpret_cast<umb_t>(entrypoint);
            ctx->sp()         = reinterpret_cast<umb_t>(stack_top);
            ctx->linux_ra()   = linuxproc_entrypoint;
            ctx->kstack_top() = reinterpret_cast<umb_t>(kstack_top);
        }

        template <SetupCase setcase>
        void init_kernel_context(Context *ctx, void *entrypoint, void *arg0,
                                 void *stack_top) noexcept {
            assert(ctx != nullptr);
            *ctx = {};
            ctx->setup_regs<setcase>();
            ctx->sp()           = reinterpret_cast<umb_t>(stack_top);
            ctx->ra()           = reinterpret_cast<umb_t>(entrypoint);
            ctx->kthread_arg0() = reinterpret_cast<umb_t>(arg0);
        }

        void reset_thread_runtime(util::nonnull<TCB *> tcb) noexcept {
            tcb->basic_entity.state   = ThreadState::EMPTY;
            tcb->basic_entity.rq_head = {};
            tcb->basic_entity.flags   = 0;
            tcb->rr_entity            = {};
            tcb->syscall_info.reset();
        }

        void build_user_contexts(util::nonnull<TCB *> tcb, void *entrypoint,
                                 void *user_stack_top,
                                 umb_t linuxproc_entrypoint = 0) noexcept {
            tcb->reset_kstack();
            auto *user_ctx = tcb->push<Context>();
            init_user_context(user_ctx, entrypoint, user_stack_top,
                              tcb->kstack_top(), linuxproc_entrypoint);
            init_kernel_context<SetupCase::UTHREAD_TRAMPOLINE>(
                tcb->kernel_context_ptr(),
                reinterpret_cast<void *>(&new_utask_trampoline), user_ctx,
                tcb->kstack_top());
        }

        void build_kernel_context(util::nonnull<TCB *> tcb, void *entrypoint,
                                  void *arg0) noexcept {
            tcb->reset_kstack();
            init_kernel_context<SetupCase::KTHREAD>(
                tcb->kernel_context_ptr(), entrypoint, arg0, tcb->kstack_top());
        }

        void prepare_user_thread(util::nonnull<TCB *> tcb, void *entrypoint,
                                 void *user_stack_top,
                                 schd::ClassType schd_class,
                                 umb_t linuxproc_entrypoint = 0) noexcept {
            tcb->is_kernel  = false;
            tcb->boot_role  = BootThreadRole::NONE;
            tcb->schd_class = schd_class;
            reset_thread_runtime(tcb);
            build_user_contexts(tcb, entrypoint, user_stack_top,
                                linuxproc_entrypoint);
        }

        void prepare_kernel_thread(util::nonnull<TCB *> tcb, void *entrypoint,
                                   void *arg0,
                                   schd::ClassType schd_class) noexcept {
            tcb->is_kernel  = true;
            tcb->boot_role  = BootThreadRole::NONE;
            tcb->schd_class = schd_class;
            reset_thread_runtime(tcb);
            build_kernel_context(tcb, entrypoint, arg0);
        }

        void prepare_forked_thread(util::nonnull<TCB *> tcb,
                                   const Context &parent_ctx,
                                   schd::ClassType parent_schd_class,
                                   BootThreadRole parent_boot_role) noexcept {
            tcb->is_kernel = false;
            reset_thread_runtime(tcb);
            tcb->reset_kstack();
            auto *child_user_ctx = tcb->push<Context>();
            *child_user_ctx      = parent_ctx;
            child_user_ctx->kstack_top() =
                reinterpret_cast<umb_t>(tcb->kstack_top());
            write_ret(*child_user_ctx,
                      syscall::RetPack{
                          .processed = true,
                          .ret0      = 0,
                          .ret1      = static_cast<b64>(ErrCode::SUCCESS),
                      });
            init_kernel_context<SetupCase::UTHREAD_TRAMPOLINE>(
                tcb->kernel_context_ptr(),
                reinterpret_cast<void *>(&new_utask_trampoline), child_user_ctx,
                tcb->kstack_top());
            tcb->boot_role  = parent_schd_class == schd::ClassType::INIT
                                  ? BootThreadRole::NONE
                                  : parent_boot_role;
            tcb->schd_class = parent_schd_class == schd::ClassType::INIT
                                  ? schd::ClassType::FCFS
                                  : parent_schd_class;
        }

        schd::ClassType exec_sched_class(PCB *pcb, TCB *current_tcb,
                                         bool target_current) {
            if (target_current && current_tcb != nullptr) {
                return current_tcb->schd_class;
            }
            if (pcb != nullptr && !pcb->threads.empty()) {
                schd::ClassType cls = pcb->threads.front().schd_class;
                if (cls != schd::ClassType::IDLE && cls != schd::ClassType::BOT)
                {
                    return cls;
                }
            }
            return schd::ClassType::FCFS;
        }

        void kthread_idle() {
            while (true) {
                Idle::idle();
            }
        }

        [[noreturn]]
        void kthread_exit() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || !tcb->is_kernel) {
                panic("非法内核线程退出");
            }
            tcb->basic_entity.state = ThreadState::WAITING;
            tcb->basic_entity
                .template flags_set<schd::SchedMeta::FLAGS_NEED_RESCHED>();
            schd::Scheduler::inst().schedule();
            panic("内核线程退出后仍被调度");
            while (true);
        }

        [[noreturn]]
        void kthread_trampoline() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || !tcb->is_kernel || tcb->kentry == nullptr) {
                panic("内核线程入口无效");
            }
            tcb->kentry(tcb->karg);
            kthread_exit();
        }

        void kthread_noarg_entry(void *arg) {
            auto entry = reinterpret_cast<void (*)()>(arg);
            entry();
        }

        Result<CapIdx> insert_pcb_cap(PCB *pcb) {
            if (pcb == nullptr || pcb->cholder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto *payload = new cap::PCBPayload(pcb);
            if (payload == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            auto insert_res = pcb->cholder->insert_to_free(payload);
            if (!insert_res.has_value()) {
                delete payload;
                propagate_return(insert_res);
            }
            return insert_res.value();
        }

        struct ReservedCapSet {
            std::unordered_set<CapIdx> caps{};

            [[nodiscard]]
            bool contains(CapIdx idx) const {
                return caps.contains(idx);
            }
        };

        Result<ReservedCapSet> collect_reserved_caps(
            cap::CHolder *holder, CapIdx pcb_cap, const CapIdx *reserved_caps,
            size_t reserved_count) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            auto pcb_cap_res = holder->lookup(pcb_cap);
            propagate(pcb_cap_res);

            ReservedCapSet set{};
            set.caps.insert(pcb_cap);
            for (size_t i = 0; i < reserved_count; ++i) {
                CapIdx idx   = reserved_caps[i];
                auto cap_res = holder->lookup(idx);
                propagate(cap_res);
                set.caps.insert(idx);
            }
            return set;
        }

        Result<void> remove_unreserved_caps(cap::CHolder *holder,
                                            const ReservedCapSet &reserved) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            ErrCode err = ErrCode::SUCCESS;
            holder->space().foreach ([&](CapIdx idx, cap::Capability *) {
                if (err != ErrCode::SUCCESS) {
                    return;
                }
                if (reserved.contains(idx)) {
                    return;
                }
                auto remove_res = holder->remove(idx);
                if (!remove_res.has_value()) {
                    err = remove_res.error();
                }
            });
            if (err != ErrCode::SUCCESS) {
                unexpect_return(err);
            }
            void_return();
        }

        struct StartupStackBuilder {
            cap::MemoryPayload &stack_mem;
            VirAddr sp;
            std::vector<VirAddr> argv_ptrs{};
            std::vector<VirAddr> envp_ptrs{};
            std::vector<uint64_t> auxv_entries{};
            std::vector<VirAddr> bsargv_ptrs{};
        };

        [[nodiscard]]
        Result<VirAddr> push_bytes(StartupStackBuilder &builder,
                                   const void *src, size_t len,
                                   size_t align = 1) {
            if (len == 0) {
                return builder.sp;
            }
            if (src == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            addr_t sp = builder.sp.arith();
            if (sp < len) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            sp -= len;
            if (align > 1) {
                sp &= ~(static_cast<addr_t>(align - 1));
            }
            if (sp < USER_STACK_BOTTOM.arith()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            VirAddr dst(sp);
            auto write_res =
                builder.stack_mem.write(dst - USER_STACK_BOTTOM, src, len);
            propagate(write_res);
            builder.sp = dst;
            return dst;
        }

        [[nodiscard]]
        Result<void> build_argv(StartupStackBuilder &builder,
                                const std::vector<std::string> &argv) {
            builder.argv_ptrs.clear();
            for (size_t i = argv.size(); i > 0; --i) {
                const auto &arg    = argv[i - 1];
                auto string_sp_res = push_bytes(builder, arg.c_str(),
                                                arg.size() + 1, alignof(char));
                propagate(string_sp_res);
                builder.argv_ptrs.push_back(string_sp_res.value());
            }
            for (size_t i = 0, j = builder.argv_ptrs.size(); i < j / 2; ++i) {
                auto tmp                     = builder.argv_ptrs[i];
                builder.argv_ptrs[i]         = builder.argv_ptrs[j - 1 - i];
                builder.argv_ptrs[j - 1 - i] = tmp;
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> build_envp(StartupStackBuilder &builder,
                                const std::vector<std::string> &envp) {
            builder.envp_ptrs.clear();
            for (size_t i = envp.size(); i > 0; --i) {
                const auto &env    = envp[i - 1];
                auto string_sp_res = push_bytes(builder, env.c_str(),
                                                env.size() + 1, alignof(char));
                propagate(string_sp_res);
                builder.envp_ptrs.push_back(string_sp_res.value());
            }
            for (size_t i = 0, j = builder.envp_ptrs.size(); i < j / 2; ++i) {
                auto tmp                     = builder.envp_ptrs[i];
                builder.envp_ptrs[i]         = builder.envp_ptrs[j - 1 - i];
                builder.envp_ptrs[j - 1 - i] = tmp;
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> build_auxv(StartupStackBuilder &builder,
                                const std::vector<uint64_t> &auxv) {
            builder.auxv_entries = auxv;
            if (builder.auxv_entries.empty() ||
                builder.auxv_entries.size() < 2 ||
                builder.auxv_entries[builder.auxv_entries.size() - 2] !=
                    AT_NULL)
            {
                builder.auxv_entries.push_back(AT_NULL);
                builder.auxv_entries.push_back(0);
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> build_bsargv(
            StartupStackBuilder &builder,
            const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
            builder.bsargv_ptrs.clear();
            for (size_t i = bsargv.size(); i > 0; --i) {
                const auto &record = bsargv[i - 1];
                if (record.bytes.size() < sizeof(bsheader)) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                auto *header =
                    reinterpret_cast<const bsheader *>(record.bytes.data());
                if (header->size != record.bytes.size() || header->type == 0) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                auto record_sp_res = push_bytes(builder, record.bytes.data(),
                                                record.bytes.size(), 8);
                propagate(record_sp_res);
                builder.bsargv_ptrs.push_back(record_sp_res.value());
            }
            for (size_t i = 0, j = builder.bsargv_ptrs.size(); i < j / 2; ++i) {
                auto tmp                       = builder.bsargv_ptrs[i];
                builder.bsargv_ptrs[i]         = builder.bsargv_ptrs[j - 1 - i];
                builder.bsargv_ptrs[j - 1 - i] = tmp;
            }
            void_return();
        }

        TaskSpec make_empty_task_spec() {
            return TaskSpec{
                .tmm        = util::owner<TaskMemoryManager *>(nullptr),
                .holder     = nullptr,
                .entrypoint = VirAddr::null,
                .linuxproc_entrypoint = VirAddr::null,
                .dyn                  = false,
                .has_interp           = false,
                .load_base            = VirAddr::null,
                .interp_base          = VirAddr::null,
                .interp_entrypoint    = VirAddr::null,
                .program_entrypoint   = VirAddr::null,
                .phdr_vaddr           = VirAddr::null,
                .phdr_num             = 0,
                .phdr_entsize         = 0,
                .linuxss_heap_vaddr   = VirAddr::null,
                .linuxss_heap_mem_cap = cap::null,
            };
        }

        void cleanup_task_spec(TaskSpec &spec) {
            if (spec.tmm.get() != nullptr) {
                PhyAddr pgd = spec.tmm->pgd();
                delete spec.tmm;
                GFP::put_page(pgd, 1);
                spec.tmm = util::owner<TaskMemoryManager *>(nullptr);
            }
        }

        class TaskSpecGuard {
        public:
            explicit TaskSpecGuard(TaskSpec &spec) noexcept : _spec(&spec) {}

            TaskSpecGuard(const TaskSpecGuard &)            = delete;
            TaskSpecGuard &operator=(const TaskSpecGuard &) = delete;

            ~TaskSpecGuard() {
                if (_spec != nullptr) {
                    cleanup_task_spec(*_spec);
                }
            }

            void release() noexcept {
                _spec = nullptr;
            }

        private:
            TaskSpec *_spec;
        };

        template <typename LoadSpecFn, typename... Args>
        Result<TaskSpec> load_task_spec_impl(TaskManager &manager,
                                             LoadSpecFn load_spec_fn,
                                             Args &&...args) {
            TaskSpec spec = make_empty_task_spec();
            LoadPrm load_prm{};
            auto load_spec_res = (manager.*load_spec_fn)(
                std::forward<Args>(args)..., spec, load_prm);
            propagate(load_spec_res);
            return spec;
        }
    }  // namespace

    Result<UserInitLayout> TaskManager::build_user_stack(
        cap::MemoryPayload &stack_mem, VirAddr stack_top, TaskSpec &spec,
        CapIdx pcb_cap, CapIdx main_tcb_cap, CapIdx stack_mem_cap) {
        if (!stack_top.nonnull()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        constexpr size_t STACK_ALIGN = 16;
        StartupStackBuilder builder{
            .stack_mem = stack_mem,
            .sp        = stack_top,
        };

        constexpr char RANDOM_STRING[] = "ITSARANDOMSTRING";
        static_assert(sizeof(RANDOM_STRING) == 16 + 1);  // 包含末尾的'\0'
        auto random_sp_res             = push_bytes(
            builder, RANDOM_STRING, sizeof(RANDOM_STRING), alignof(char));
        propagate(random_sp_res);

        if (!spec.linux_execfn.empty() && spec.linux_execfn != "<cap>") {
            auto execfn_sp_res =
                push_bytes(builder, spec.linux_execfn.c_str(),
                           spec.linux_execfn.size() + 1, alignof(char));
            propagate(execfn_sp_res);
            auto null_pos = std::find(spec.auxv.begin(), spec.auxv.end(), AT_NULL);
            if (null_pos != spec.auxv.end()) {
                spec.auxv.erase(null_pos, spec.auxv.end());
            }
            spec.auxv.push_back(AT_EXECFN);
            spec.auxv.push_back(execfn_sp_res.value().arith());
        }

#if defined(__ARCH_loongarch64__)
        constexpr char PLATFORM_STRING[] = "loongarch64";
        auto platform_sp_res             = push_bytes(
            builder, PLATFORM_STRING, sizeof(PLATFORM_STRING), alignof(char));
        propagate(platform_sp_res);
        spec.auxv.push_back(AT_PLATFORM);
        spec.auxv.push_back(platform_sp_res.value().arith());
#endif
        spec.auxv.push_back(AT_RANDOM);
        spec.auxv.push_back(random_sp_res.value().arith());
        spec.auxv.push_back(AT_NULL);
        spec.auxv.push_back(0);
        auto pcb_explain_res = append_bootstrap_cap_explain_record(
            spec, pcb_cap, PayloadType::PCB, perm::allperm(), "#self:0");
        propagate(pcb_explain_res);
        auto tcb_explain_res = append_bootstrap_cap_explain_record(
            spec, main_tcb_cap, PayloadType::TCB, perm::allperm(), "#main:0");
        propagate(tcb_explain_res);

        char heap_desc[128];
        snprintf(heap_desc, sizeof(heap_desc), "#heap:[%p,%p)",
                 spec.heap_vaddr.addr(), spec.heap_vaddr.addr());
        auto heap_cap_res = append_bootstrap_cap_explain_record(
            spec, spec.heap_mem_cap, PayloadType::MEMORY, perm::allperm(),
            heap_desc);
        propagate(heap_cap_res);
        if (spec.linuxproc_entrypoint.nonnull() &&
            spec.linuxss_heap_vaddr.nonnull() &&
            spec.linuxss_heap_mem_cap != cap::null)
        {
            char ss_heap_desc[128];
            snprintf(ss_heap_desc, sizeof(ss_heap_desc), "#ss-heap:[%p,%p)",
                     spec.linuxss_heap_vaddr.addr(),
                     spec.linuxss_heap_vaddr.addr());
            auto ss_heap_cap_res = append_bootstrap_cap_explain_record(
                spec, spec.linuxss_heap_mem_cap, PayloadType::MEMORY,
                perm::allperm(), ss_heap_desc);
            propagate(ss_heap_cap_res);
        }

        char stack_desc[128];
        snprintf(stack_desc, sizeof(stack_desc), "#stack:[%p,%p)",
                 USER_STACK_BOTTOM.addr(), USER_STACK_TOP.addr());
        auto stack_cap_explain_res = append_bootstrap_cap_explain_record(
            spec, stack_mem_cap, PayloadType::MEMORY, perm::allperm(),
            stack_desc);
        propagate(stack_cap_explain_res);

        auto heap_vaddr_res = append_bootstrap_vaddr_explain_record(
            spec, spec.heap_vaddr, "#heap");
        propagate(heap_vaddr_res);
        if (spec.linuxproc_entrypoint.nonnull() &&
            spec.linuxss_heap_vaddr.nonnull())
        {
            auto ss_heap_vaddr_res = append_bootstrap_vaddr_explain_record(
                spec, spec.linuxss_heap_vaddr, "#ss-heap");
            propagate(ss_heap_vaddr_res);
        }
        auto stack_vaddr_res = append_bootstrap_vaddr_explain_record(
            spec, USER_STACK_BOTTOM, "#stack");
        propagate(stack_vaddr_res);
        auto entry_vaddr_res = append_bootstrap_vaddr_explain_record(
            spec,
            spec.linuxproc_entrypoint.nonnull() ? spec.linuxproc_entrypoint
                                                : spec.entrypoint,
            "#entrypoint");
        propagate(entry_vaddr_res);
        if (spec.linuxproc_entrypoint.nonnull()) {
            auto ss_vaddr_res = append_bootstrap_vaddr_explain_record(
                spec, spec.entrypoint, "#ss-entrypoint");
            propagate(ss_vaddr_res);
        }
        auto argv_res   = build_argv(builder, spec.argv);
        auto envp_res   = build_envp(builder, spec.envp);
        auto auxv_res   = build_auxv(builder, spec.auxv);
        auto bsargv_res = build_bsargv(builder, spec.bsargv);
        propagate(argv_res);
        propagate(envp_res);
        propagate(auxv_res);
        propagate(bsargv_res);

        std::vector<uint64_t> layout{};
        layout.reserve(1 + builder.argv_ptrs.size() + 1 +
                       builder.envp_ptrs.size() + 1 +
                       builder.auxv_entries.size() * 2 + 1 +
                       builder.bsargv_ptrs.size() + 1);
        const size_t argc        = builder.argv_ptrs.size();
        const size_t argv_offset = layout.size();
        layout.push_back(argc);
        for (auto ptr : builder.argv_ptrs) {
            layout.push_back(ptr.arith());
        }
        layout.push_back(0);
        const size_t envp_offset = layout.size();
        for (auto ptr : builder.envp_ptrs) {
            layout.push_back(ptr.arith());
        }
        layout.push_back(0);
        const size_t auxv_offset = layout.size();
        for (auto value : builder.auxv_entries) {
            layout.push_back(value);
        }
        layout.push_back(builder.bsargv_ptrs.size());
        for (auto ptr : builder.bsargv_ptrs) {
            layout.push_back(ptr.arith());
        }
        layout.push_back(0);

        auto table_sp_res =
            push_bytes(builder, layout.data(), layout.size() * sizeof(uint64_t),
                       STACK_ALIGN);
        propagate(table_sp_res);
        VirAddr base = table_sp_res.value();
        UserInitLayout init_layout{
            .sp   = base,
            .argc = argc,
            .argv = base + argv_offset * sizeof(uint64_t),
            .envp = base + envp_offset * sizeof(uint64_t),
            .auxv = base + auxv_offset * sizeof(uint64_t),
        };
        loggers::TASK::DEBUG(
            "build_user_stack: stack_top=%p final_sp=%p argc=%lu "
            "envc=%lu bsargc=%lu",
            stack_top.addr(), builder.sp.addr(), builder.argv_ptrs.size(),
            builder.envp_ptrs.size(), builder.bsargv_ptrs.size());
        return init_layout;
    }

    Result<util::nonnull<TCB *>> TaskManager::setup_user_main_thread(
        util::nonnull<PCB *> pcb, util::nonnull<TCB *> tcb, TaskSpec &spec,
        bool link_into_pcb, const char *log_tag) {
        if (pcb->cholder == nullptr || pcb->tmm.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *stack_mem = new cap::MemoryPayload(MAX_INITIAL_STACK_SIZE, false,
                                                 false, VMA::Growth::GROW_DOWN);
        if (stack_mem == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto stack_cap_res = pcb->cholder->insert_to_free(stack_mem);
        if (!stack_cap_res.has_value()) {
            delete stack_mem;
            propagate_return(stack_cap_res);
        }
        auto vma_res =
            pcb->tmm->add_vma(VMA::Type::STACK, VMA::Growth::GROW_DOWN,
                              VirArea(USER_STACK_BOTTOM, USER_STACK_TOP),
                              stack_mem, VMA::PROT_R | VMA::PROT_W);
        propagate(vma_res);
        loggers::TASK::INFO(
            "创建STACK VMA: pid=%lu area=[%p,%p) mem=%p memsz=%lu mem_off=%lu",
            pcb->pid, USER_STACK_BOTTOM.addr(), USER_STACK_TOP.addr(),
            stack_mem, static_cast<unsigned long>(MAX_INITIAL_STACK_SIZE), 0UL);

        auto tcb_cap_res = pcb->cholder->create<cap::TCBPayload>(tcb.get());
        propagate(tcb_cap_res);

        pcb->main_tcb_cap = tcb_cap_res.value();
        auto init_layout_res =
            build_user_stack(*stack_mem, USER_STACK_TOP, spec, pcb->pcb_cap,
                             pcb->main_tcb_cap, stack_cap_res.value());
        propagate(init_layout_res);
        const auto init_layout = init_layout_res.value();
        loggers::TASK::DEBUG("栈内存分配: pid=%lu 栈内存地址=%p 已分配=%lu",
                             pcb->pid, stack_mem, stack_mem->allocated_size());
        prepare_user_thread(tcb, pcb->entrypoint.addr(), init_layout.sp.addr(),
                            tcb->schd_class, spec.linuxproc_entrypoint.arith());

        tcb->context()->set_init_regs(static_cast<umb_t>(init_layout.argc),
                                      init_layout.argv.arith(),
                                      init_layout.envp.arith());
        if (link_into_pcb) {
            pcb->threads.push_back(*tcb);
        }
        loggers::TASK::INFO("%s: pid=%lu entry=%p sp=%p", log_tag, pcb->pid,
                            pcb->entrypoint.addr(), init_layout.sp.addr());

        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::construct_thread(
        util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top,
        schd::ClassType schd_class, bool kernel_thread) {
        auto tcb_res = create_bound_tcb(pcb);
        propagate(tcb_res);
        util::nonnull<TCB *> tcb = tcb_res.value();

        if (kernel_thread) {
            prepare_kernel_thread(tcb, entrypoint, nullptr, schd_class);
        } else {
            prepare_user_thread(tcb, entrypoint, stack_top, schd_class);
        }

        pcb->threads.push_back(*tcb);
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::populate_task(
        util::nonnull<PCB *> pcb, TaskSpec spec, schd::ClassType schd_class,
        TCB *reuse_main_tcb) {
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (spec.tmm.get() == nullptr || spec.holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        pcb->tmm                   = spec.tmm;
        pcb->cholder               = spec.holder;
        pcb->entrypoint            = spec.entrypoint;
        pcb->linuxproc_entrypoint  = spec.linuxproc_entrypoint;
        pcb->linux_subsystem_entry = spec.entrypoint;
        pcb->is_linux_process      = spec.linuxproc_entrypoint.nonnull();

        if (pcb->pcb_cap == cap::null) {
            auto pcb_cap_res = insert_pcb_cap(pcb);
            propagate(pcb_cap_res);
            pcb->pcb_cap = pcb_cap_res.value();
        } else {
            auto pcb_cap_res = pcb->cholder->lookup(pcb->pcb_cap);
            propagate(pcb_cap_res);
        }

        if (reuse_main_tcb == nullptr) {
            auto tcb_res = create_bound_tcb(pcb);
            propagate(tcb_res);
            auto tcb = tcb_res.value();
            auto tcb_guard =
                util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });
            tcb->is_kernel  = false;
            tcb->boot_role  = BootThreadRole::NONE;
            tcb->schd_class = schd_class;
            auto setup_res =
                setup_user_main_thread(pcb, tcb, spec, true, "构造用户主线程");
            propagate(setup_res);
            tcb_guard.release();
            return setup_res.value();
        }

        auto tcb        = util::nnullforce(reuse_main_tcb);
        tcb->task       = pcb;
        tcb->is_kernel  = false;
        tcb->boot_role  = BootThreadRole::NONE;
        tcb->schd_class = schd_class;
        return setup_user_main_thread(pcb, tcb, spec, true, "复用主线程上下文");
    }

    Result<util::nonnull<PCB *>> TaskManager::commit_loaded_task(
        TaskSpec spec, schd::ClassType schd_class, bool wakeup) {
        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard           = delete_guard(util::owner(pcb.get()));

        auto init_res = init_pcb(pcb);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        auto main_thread_res = populate_task(pcb, spec, schd_class);
        if (!main_thread_res.has_value()) {
            loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                     to_cstring(main_thread_res.error()));
            propagate_return(main_thread_res);
        }
        if (schd_class == schd::ClassType::INIT) {
            main_thread_res.value()->boot_role = BootThreadRole::INIT_USER;
        }

        if (wakeup) {
            TCB *main_tcb = main_thread_res.value();
            if (!schd::Scheduler::inst().wakeup_new(main_tcb)) {
                loggers::SUSTCORE::ERROR("唤醒新进程失败");
                unexpect_return(ErrCode::CREATION_FAILED);
            }
        }

        _pid_map[pcb->pid] = pcb.get();
        pcb_guard.release();
        return pcb;
    }

    Result<util::nonnull<PCB *>> TaskManager::create_loaded_user_task(
        TaskSpec spec, schd::ClassType schd_class, bool wakeup) {
        return commit_loaded_task(spec, schd_class, wakeup);
    }

    Result<util::nonnull<PCB *>> TaskManager::create_kernel_task() {
        if (_kernel_pcb != nullptr) {
            return util::nnullforce(_kernel_pcb);
        }

        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard           = delete_guard(util::owner(pcb.get()));
        auto init_res            = init_pcb(pcb);
        propagate(init_res);

        pcb->is_kernel = true;
        auto tmm_res =
            TaskMemoryManager::from_existing_pgd(env::inst().main_kernel_pgd());
        if (!tmm_res.has_value()) {
            loggers::SUSTCORE::FATAL("无法绑定主内核页表: %s",
                                     to_cstring(tmm_res.error()));
            propagate_return(tmm_res);
        }
        auto tmm_guard  = delete_guard(tmm_res.value());
        pcb->tmm        = tmm_res.value();
        auto create_res = cap::CHolderManager::inst().create_holder();
        propagate(create_res);
        pcb->cholder      = create_res.value();
        pcb->entrypoint   = VirAddr(nullptr);
        pcb->pcb_cap      = cap::null;
        pcb->main_tcb_cap = cap::null;

        _kernel_pcb        = pcb.get();
        _pid_map[pcb->pid] = pcb.get();
        pcb_guard.release();
        tmm_guard.release();
        return pcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kernel_thread(
        void (*entry)(), schd::ClassType schd_class) {
        return create_kernel_thread(
            &kthread_noarg_entry, reinterpret_cast<void *>(entry), schd_class);
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kernel_thread(
        KThreadEntry entry, void *arg, schd::ClassType schd_class) {
        if (entry == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (schd_class == schd::ClassType::BOT) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto pcb_res = kernel_pcb();
        propagate(pcb_res);
        auto tcb_res = create_bound_tcb(pcb_res.value());
        propagate(tcb_res);
        auto tcb       = tcb_res.value();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });

        tcb->kentry = entry;
        tcb->karg   = arg;
        prepare_kernel_thread(tcb,
                              reinterpret_cast<void *>(&kthread_trampoline),
                              nullptr, schd_class);
        pcb_res.value()->threads.push_back(*tcb);

        tcb_guard.release();
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::create_idle_thread() {
        return create_kernel_thread(&kthread_idle, schd::ClassType::IDLE);
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kinit_thread() {
        auto tcb_res =
            create_kernel_thread(&kinit_runtime_entry, schd::ClassType::INIT);
        propagate(tcb_res);
        tcb_res.value()->boot_role = BootThreadRole::KINIT;
        return tcb_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::create_init_task(
        TaskSpec spec /* ... args*/) {
        constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::INIT;
        return create_loaded_user_task(spec, INIT_SCHED_CLASS, false);
    }

    Result<util::nonnull<PCB *>> TaskManager::create_task(
        TaskSpec spec, schd::ClassType schd_class /* ... args*/) {
        if (schd_class == schd::ClassType::INIT ||
            schd_class == schd::ClassType::IDLE ||
            schd_class == schd::ClassType::BOT)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return create_loaded_user_task(spec, schd_class, true);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_task_image(
        CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
        bool wakeup, const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        auto spec_res =
            load_task_spec_impl(*this, &TaskManager::load_task_spec, image_cap,
                                holder, argv, envp, bsargv);
        propagate(spec_res);
        TaskSpec spec = std::move(spec_res.value());
        TaskSpecGuard spec_guard(spec);
        auto task_res = commit_loaded_task(spec, schd_class, wakeup);
        propagate(task_res);
        spec_guard.release();
        return task_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_linux_task_image(
        CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
        schd::ClassType schd_class, bool wakeup,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        auto spec_res = load_task_spec_impl(
            *this, &TaskManager::load_linux_task_spec, image_cap, holder,
            subsystem_image_cap, argv, envp, bsargv);
        propagate(spec_res);
        TaskSpec spec = std::move(spec_res.value());
        TaskSpecGuard spec_guard(spec);
        auto task_res = commit_loaded_task(spec, schd_class, wakeup);
        propagate(task_res);
        spec_guard.release();
        return task_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf(
        CapIdx image_cap, schd::ClassType schd_class) {
        return load_task_image(image_cap, nullptr, schd_class, true);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf_into(
        CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        return load_task_image(image_cap, holder, schd_class, true, argv, envp,
                               bsargv);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_linux_elf_into(
        CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
        schd::ClassType schd_class, const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        return load_linux_task_image(image_cap, holder, subsystem_image_cap,
                                     schd_class, true, argv, envp, bsargv);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_init(const char *path) {
        constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::INIT;
        if (path == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto *holder      = create_res.value();
        auto holder_guard = util::Guard([holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
        });
        auto image_res    = VFS::inst().open(path, *holder);
        propagate(image_res);
        auto root_res =
            VFS::inst().open_dir("/", *holder,
                                 perm::vdir::READ | perm::vdir::WRITE |
                                     perm::vdir::EXEC | perm::basic::CLONE);
        propagate(root_res);
        std::vector<TaskSpec::BootstrapRecordData> bsargv{};
        {
            BootstrapCapExplainPayloadHead head{
                .cap_idx  = root_res.value(),
                .cap_type = PayloadType::VDIR,
                .cap_perm = perm::vdir::READ | perm::vdir::WRITE |
                            perm::vdir::EXEC | perm::basic::CLONE,
            };
            std::vector<char> payload(sizeof(head) + 3);
            memcpy(payload.data(), &head, sizeof(head));
            memcpy(payload.data() + sizeof(head), "#/", 3);
            bsargv.push_back(TaskSpec::BootstrapRecordData{
                .type  = boot::TYPE_CAPEXP,
                .bytes = std::vector<char>(sizeof(bsheader) + payload.size()),
            });
            auto *header =
                reinterpret_cast<bsheader *>(bsargv.back().bytes.data());
            header->size = bsargv.back().bytes.size();
            header->type = boot::TYPE_CAPEXP;
            memcpy(bsargv.back().bytes.data() + sizeof(bsheader),
                   payload.data(), payload.size());
        }
        {
            constexpr char CWD_DESC[] = "#cwd:/";
            bsargv.push_back(TaskSpec::BootstrapRecordData{
                .type  = boot::TYPE_PATHEXP,
                .bytes = std::vector<char>(sizeof(bsheader) + sizeof(CWD_DESC)),
            });
            auto *header =
                reinterpret_cast<bsheader *>(bsargv.back().bytes.data());
            header->size = bsargv.back().bytes.size();
            header->type = boot::TYPE_PATHEXP;
            memcpy(bsargv.back().bytes.data() + sizeof(bsheader), CWD_DESC,
                   sizeof(CWD_DESC));
        }
        auto *platform = device::DeviceModel::inst().platform();
        if (platform != nullptr && !platform->stdout_device_dir().empty()) {
            char stdout_desc[256]{};
            snprintf(stdout_desc, sizeof(stdout_desc), "#stdout:%s",
                     platform->stdout_device_dir().c_str());
            bsargv.push_back(TaskSpec::BootstrapRecordData{
                .type  = boot::TYPE_PATHEXP,
                .bytes = std::vector<char>(sizeof(bsheader) +
                                           strlen(stdout_desc) + 1),
            });
            auto *header =
                reinterpret_cast<bsheader *>(bsargv.back().bytes.data());
            header->size = bsargv.back().bytes.size();
            header->type = boot::TYPE_PATHEXP;
            memcpy(bsargv.back().bytes.data() + sizeof(bsheader), stdout_desc,
                   strlen(stdout_desc) + 1);
        }
        auto loaded_spec_res = load_task_spec_impl(
            *this, &TaskManager::load_task_spec, image_res.value(), holder,
            std::vector<std::string>{}, std::vector<std::string>{}, bsargv);
        propagate(loaded_spec_res);

        TaskSpec loaded_spec = std::move(loaded_spec_res.value());
        TaskSpecGuard loaded_spec_guard(loaded_spec);
        auto load_res =
            commit_loaded_task(loaded_spec, INIT_SCHED_CLASS, false);
        propagate(load_res);
        loaded_spec_guard.release();
        holder_guard.release();
        return load_res.value();
    }

    Result<util::nonnull<TCB *>> TaskManager::populate_forked_task(
        util::nonnull<PCB *> child_pcb, PCB *parent_pcb, TCB *parent_tcb,
        Context *parent_ctx, CapIdx ret_slot) {
        if (parent_pcb == nullptr || parent_tcb == nullptr ||
            parent_ctx == nullptr || parent_pcb->tmm == nullptr ||
            parent_pcb->cholder == nullptr)
        {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto holder_res = cap::CHolderManager::inst().create_holder();
        propagate(holder_res);
        cap::CHolder *child_holder = holder_res.value();
        auto holder_guard          = util::Guard([child_holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(child_holder->id());
            assert(rm_res.has_value());
        });

        auto pgd_res = GFP::get_free_page(1);
        propagate(pgd_res);
        auto child_tmm = util::owner(new TaskMemoryManager(pgd_res.value()));
        auto tmm_guard = util::Guard([&child_tmm]() {
            if (child_tmm.get() == nullptr) {
                return;
            }
            PhyAddr pgd = child_tmm->pgd();
            delete child_tmm;
            GFP::put_page(pgd, 1);
        });

        auto clone_mem_res = parent_pcb->tmm->clone_to_cow(*child_tmm);
        propagate(clone_mem_res);

        ErrCode clone_caps_err = ErrCode::SUCCESS;
        parent_pcb->cholder->space().foreach (
            [&](CapIdx idx, cap::Capability *parent_cap) {
                if (clone_caps_err != ErrCode::SUCCESS) {
                    return;
                }
                cap::Payload *payload = parent_cap->payload();
                auto *memory = parent_cap->payload_as<cap::MemoryPayload>();
                if (memory != nullptr) {
                    auto child_memory_res =
                        parent_pcb->tmm->cloned_memory_for(memory, *child_tmm);
                    if (child_memory_res.has_value()) {
                        payload = child_memory_res.value();
                    } else {
                        payload = memory->clone_payload();
                    }
                }
                auto insert_res =
                    child_holder->insert(idx, payload, parent_cap->perm());
                if (!insert_res.has_value()) {
                    clone_caps_err = insert_res.error();
                }
            });
        if (clone_caps_err != ErrCode::SUCCESS) {
            unexpect_return(clone_caps_err);
        }

        child_pcb->tmm          = child_tmm;
        child_pcb->cholder      = child_holder;
        child_pcb->entrypoint   = parent_pcb->entrypoint;
        child_pcb->linuxproc_entrypoint  = parent_pcb->linuxproc_entrypoint;
        child_pcb->linux_subsystem_entry = parent_pcb->linux_subsystem_entry;
        child_pcb->is_linux_process      = parent_pcb->is_linux_process;
        child_pcb->pcb_cap      = ret_slot;
        child_pcb->main_tcb_cap = parent_pcb->main_tcb_cap;

        auto child_tcb_res = create_bound_tcb(child_pcb);
        propagate(child_tcb_res);
        auto child_tcb = child_tcb_res.value();
        auto tcb_guard =
            util::Guard([this, child_tcb]() { (void)recycle_tcb(child_tcb); });

        prepare_forked_thread(child_tcb, *parent_ctx, parent_tcb->schd_class,
                              parent_tcb->boot_role);
        child_pcb->threads.push_back(*child_tcb);
        tcb_guard.release();

        auto *pcb_payload = new cap::PCBPayload(child_pcb.get());
        if (pcb_payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto insert_parent_res =
            parent_pcb->cholder->insert(ret_slot, pcb_payload, perm::allperm());
        if (!insert_parent_res.has_value()) {
            delete pcb_payload;
            propagate_return(insert_parent_res);
        }
        auto insert_child_res =
            child_holder->insert(ret_slot, pcb_payload, perm::allperm());
        if (!insert_child_res.has_value()) {
            auto remove_res = parent_pcb->cholder->remove(ret_slot);
            assert(remove_res.has_value());
            propagate_return(insert_child_res);
        }

        tmm_guard.release();
        holder_guard.release();
        return child_tcb;
    }

    Result<ForkResult> TaskManager::fork_current(CapIdx ret_slot) {
        auto *parent_tcb = schd::Scheduler::inst().current_tcb();
        auto *parent_ctx = env::inst().trap_context();
        if (parent_tcb == nullptr || parent_tcb->task == nullptr ||
            parent_ctx == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *parent_pcb = parent_tcb->task;
        if (parent_pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (parent_pcb->tmm == nullptr || parent_pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        util::nonnull<PCB *> child_pcb = alloc_pcb();
        auto pcb_guard = delete_guard(util::owner(child_pcb.get()));
        auto init_res  = init_pcb(child_pcb);
        propagate(init_res);
        auto fill_res = populate_forked_task(child_pcb, parent_pcb, parent_tcb,
                                             parent_ctx, ret_slot);
        propagate(fill_res);
        TCB *child_tcb = fill_res.value();

        _pid_map[child_pcb->pid] = child_pcb.get();
        if (!schd::Scheduler::inst().wakeup_new(child_tcb)) {
            _pid_map.erase(child_pcb->pid);
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        ForkResult result{ret_slot, child_pcb->pid};
        pcb_guard.release();
        return result;
    }

    Result<CapIdx> TaskManager::create_thread_current(VirAddr entry,
                                                      VirAddr stack_addr,
                                                      size_t stack_size) {
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *pcb = current_tcb->task;
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (pcb->tmm == nullptr || pcb->cholder == nullptr ||
            !entry.nonnull() || !stack_addr.nonnull() || stack_size == 0)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (current_tcb->schd_class == schd::ClassType::IDLE) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        addr_t stack_base = stack_addr.arith();
        if (stack_base > MAX_ADDR - stack_size) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        VirAddr stack_top(stack_base + stack_size);
        VirArea stack_area(stack_addr, stack_top);
        auto range_res = pcb->tmm->locate_range(stack_area);
        propagate(range_res);

        auto con_res =
            construct_thread(util::nnullforce(pcb), entry.addr(),
                             stack_top.addr(), current_tcb->schd_class);
        propagate(con_res);
        auto tcb       = con_res.value();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });

        auto cap_res = pcb->cholder->create<cap::TCBPayload>(tcb.get());
        propagate(cap_res);
        auto cap_guard = remove_guard(pcb->cholder, cap_res.value());

        if (!schd::Scheduler::inst().wakeup_new(tcb.get())) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        cap_guard.release();
        tcb_guard.release();
        return cap_res.value();
    }

    Result<void> TaskManager::exec_current(CapIdx image_cap,
                                           const CapIdx *reserved_caps,
                                           size_t reserved_count) {
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return exec_pcb(util::nnullforce(current_tcb->task), image_cap,
                        reserved_caps, reserved_count);
    }

    Result<void> TaskManager::exec_pcb(
        util::nonnull<PCB *> target, CapIdx image_cap,
        const CapIdx *reserved_caps, size_t reserved_count,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        PCB *pcb = target.get();
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (pcb->tmm == nullptr || pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        bool target_current =
            current_tcb != nullptr && current_tcb->task == pcb;
        schd::ClassType schd_class =
            exec_sched_class(pcb, current_tcb, target_current);
        TCB *reuse_tcb = target_current ? current_tcb : nullptr;

        auto reserved_res = collect_reserved_caps(
            pcb->cholder, pcb->pcb_cap, reserved_caps, reserved_count);
        propagate(reserved_res);
        const auto &reserved = reserved_res.value();

        TaskSpec spec = make_empty_task_spec();
        TaskSpecGuard spec_guard(spec);
        LoadPrm load_prm{};
        auto load_spec_res = load_task_spec(image_cap, pcb->cholder, argv, envp,
                                            bsargv, spec, load_prm);
        propagate(load_spec_res);

        auto prune_res = remove_unreserved_caps(pcb->cholder, reserved);
        propagate(prune_res);

        TaskMemoryManager *old_tmm = pcb->tmm;
        while (!pcb->threads.empty()) {
            TCB *tcb = &pcb->threads.front();
            pcb->threads.pop_front();
            if (tcb == reuse_tcb) {
                continue;
            }
            if (tcb->basic_entity.state == ThreadState::READY) {
                auto dequeue_res =
                    schd::Scheduler::inst().dequeue(util::nnullforce(tcb));
                propagate(dequeue_res);
            }
            auto recycle_res = recycle_tcb(util::nnullforce(tcb));
            propagate(recycle_res);
        }
        pcb->tmm          = util::owner<TaskMemoryManager *>(nullptr);
        pcb->entrypoint   = VirAddr::null;
        pcb->main_tcb_cap = cap::null;

        auto populate_res =
            populate_task(util::nnullforce(pcb), spec, schd_class, reuse_tcb);
        propagate(populate_res);

        if (target_current) {
            current_tcb->basic_entity.state = ThreadState::RUNNING;
            current_tcb->basic_entity.flags = 0;
            current_tcb->rr_entity          = {};
        } else if (!schd::Scheduler::inst().wakeup_new(populate_res.value())) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        spec_guard.release();

        if (target_current) {
            schd::switch_pgd(pcb->tmm);
        }

        TaskSpec old_spec{
            .tmm        = util::owner(old_tmm),
            .holder     = nullptr,
            .entrypoint = VirAddr::null,
        };
        cleanup_task_spec(old_spec);

        loggers::SUSTCORE::DEBUG("execve成功: image_cap=%p pid=%d", image_cap,
                                 pcb->pid);
        void_return();
    }
}  // namespace task
