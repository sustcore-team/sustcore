/**
 * @file exception.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 trap 入口处理
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/device/cpuic.h>
#include <arch/loongarch64/csr.h>
#include <arch/loongarch64/trait.h>
#include <device/model.h>
#include <env.h>
#include <logger.h>
#include <sus/logger.h>
#include <syscall/syscall.h>
#include <task/scheduler.h>
#include <task/task.h>

using namespace la64;

namespace exception {
    constexpr umb_t INTERRUPT           = 0x0;  // 中断
    constexpr umb_t LOAD_PAGE_INVALID   = 0x1;  // load操作页无效例外
    constexpr umb_t STORE_PAGE_INVALID  = 0x2;  // store操作页无效例外
    constexpr umb_t FETCH_PAGE_INVALID  = 0x3;  // 取指操作页无效例外
    constexpr umb_t PAGE_MODIFICATION   = 0x4;  // 页修改例外
    constexpr umb_t PAGE_NOT_READABLE   = 0x5;  // 页不可读例外
    constexpr umb_t PAGE_NOT_EXECUTABLE = 0x6;  // 页不可执行例外
    constexpr umb_t PAGE_PRIVILEGE_VIOLATION = 0x7;  // 页特权等级不合规则例外
    // 取指地址错例外 (EsubCode = 0)
    constexpr umb_t FETCH_ADDRESS_FAULT         = 0x8;
    // 访存指令地址错例外  (EsubCode = 1)
    constexpr umb_t MEMORY_ACCESS_ADDRESS_FAULT = 0x8;
    constexpr umb_t ADDRESS_MISALIGNED          = 0x9;  // 地址非对齐例外
    constexpr umb_t BOUNDARY_CHECK              = 0xA;  // 边界检查错例外
    constexpr umb_t SYSTEM_CALL                 = 0xB;  // 系统调用例外
    constexpr umb_t BREAKPOINT                  = 0xC;  // 断点例外
    constexpr umb_t INSTRUCTION_NOT_EXIST       = 0XD;  // 指令不存在例外
    constexpr umb_t INSTRUCTION_PRIVILEGE_ERROR = 0xE;  // 指令特权等级错例外
    // 浮点指令未使能例外
    constexpr umb_t FLOAT_DISABLED                     = 0xF;
    // 128位向量扩展指令未使能例外
    constexpr umb_t VECTOR_128_DISABLED                = 0x10;
    // 256位向量扩展指令未使能例外
    constexpr umb_t VECTOR_256_DISABLED                = 0x11;
    // 基础浮点指令例外 (EsubCode = 0)
    constexpr umb_t FLOAT_EXCEPTION                    = 0x12;
    // 向量浮点指令例外 (EsubCode = 1)
    constexpr umb_t VECTOR_FLOAT_EXCEPTION             = 0x12;
    // 取指监测点例外 (EsubCode = 0)
    constexpr umb_t WATCHPOINT_FETCH                   = 0x13;
    // load/store操作监测点例外 (EsubCode = 1)
    constexpr umb_t WATCHPOINT_MEMORY                  = 0x13;
    // 二进制翻译扩展指令未使能例外
    constexpr umb_t BINARY_TRANSLATION_DISABLED        = 0x14;
    // 二进制翻译相关例外
    constexpr umb_t BINARY_TRANSLATION_EXCEPTION       = 0x15;
    // 客户机敏感特权资源例外
    constexpr umb_t GUEST_SENSITIVE_PRIVILEGE_RESOURCE = 0x16;
    // 虚拟机监控调用例外
    constexpr umb_t HYPERVISOR_CALL                    = 0x17;
    // 客户机CSR软件修改例外 (EsubCode = 0)
    constexpr umb_t GUEST_CSR_SOFTWARE_MODIFICATION    = 0x18;
    // 客户机CSR硬件修改例外 (EsubCode = 1)
    constexpr umb_t GUEST_CSR_HARDWARE_MODIFICATION    = 0x18;

    const char *EXCEPTION_NAMES[] = {
        "中断",
        "load操作页无效例外",
        "store操作页无效例外",
        "取指操作页无效例外",
        "页修改例外",
        "页不可读例外",
        "页不可执行例外",
        "页特权等级不合规则例外",
        "取指地址错例外/访存指令地址错例外",
        "地址非对齐例外",
        "边界检查错例外",
        "系统调用例外",
        "断点例外",
        "指令不存在例外",
        "指令特权等级错例外",
        "浮点指令未使能例外",
        "128位向量扩展指令未使能例外",
        "256位向量扩展指令未使能例外",
        "基础浮点指令例外/向量浮点指令例外",
        "取指监测点例外/load/store操作监测点例外",
        "二进制翻译扩展指令未使能例外",
        "二进制翻译相关例外",
        "客户机敏感特权资源例外",
        "虚拟机监控调用例外",
        "客户机CSR软件修改例外/客户机CSR硬件修改例外"};

    /**
     * @brief 获取例外号对应的可读名称。
     *
     * @param cause 例外号（按上述常量定义）。
     * @return const char* 例外名称。
     */
    [[nodiscard]]
    const char *exception_name(umb_t cause) noexcept {
        if (cause < sizeof(EXCEPTION_NAMES) / sizeof(EXCEPTION_NAMES[0])) {
            return EXCEPTION_NAMES[cause];
        }
        return "未知";
    }

    [[nodiscard]]
    const char *page_fault_kind(umb_t cause) noexcept {
        switch (cause) {
            case LOAD_PAGE_INVALID:        return "load-page-invalid";
            case STORE_PAGE_INVALID:       return "store-page-invalid";
            case FETCH_PAGE_INVALID:       return "fetch-page-invalid";
            case PAGE_MODIFICATION:        return "page-modification";
            case PAGE_NOT_READABLE:        return "page-not-readable";
            case PAGE_NOT_EXECUTABLE:      return "page-not-executable";
            case PAGE_PRIVILEGE_VIOLATION: return "page-privilege-violation";
            case FETCH_ADDRESS_FAULT:      return "fetch-address-fault";
            default: return "not-paging-fault";
        }
    }

    [[nodiscard]]
    const char *page_size_name(PageMan::PageSize size) noexcept {
        switch (size) {
            case PageMan::PageSize::_4K:   return "4K";
            case PageMan::PageSize::_2M:   return "2M";
            case PageMan::PageSize::_1G:   return "1G";
            case PageMan::PageSize::_NULL:
            default:                       return "null";
        }
    }

    [[nodiscard]]
    bool from_umode() noexcept {
        return csr_get_prmd().pplv == PLV_USER;
    }

    void log_current_task_error() {
        if (!schd::Scheduler::initialized()) {
            loggers::INTERRUPT::ERROR("task: scheduler not initialized");
            return;
        }

        auto *tcb = schd::Scheduler::inst().current_tcb();
        if (tcb == nullptr) {
            loggers::INTERRUPT::ERROR("task: none");
            return;
        }
        loggers::INTERRUPT::ERROR(
            "task: pid=%lu, tid=%lu, state=%d, kstack_top=%p",
            tcb->task != nullptr ? tcb->task->pid : 0, tcb->tid,
            static_cast<int>(tcb->basic_entity.state), tcb->kstack_bottom);
    }

    void dump_regs(const Context *ctx) {
        if (ctx == nullptr) {
            loggers::INTERRUPT::ERROR("regs: ctx=null");
            return;
        }

        loggers::INTERRUPT::ERROR("regs: ra=0x%lx sp=0x%lx tp=0x%lx fp=0x%lx",
                                  ctx->ra, ctx->sp(), ctx->tp, ctx->fp);
        loggers::INTERRUPT::ERROR("regs: a0=0x%lx a1=0x%lx a2=0x%lx a3=0x%lx",
                                  ctx->a0, ctx->a1, ctx->a2, ctx->a3);
        loggers::INTERRUPT::ERROR("regs: a4=0x%lx a5=0x%lx a6=0x%lx a7=0x%lx",
                                  ctx->a4, ctx->a5, ctx->a6, ctx->a7);
        loggers::INTERRUPT::ERROR("regs: t0=0x%lx t1=0x%lx t2=0x%lx t3=0x%lx",
                                  ctx->t0, ctx->t1, ctx->t2, ctx->t3);
        loggers::INTERRUPT::ERROR("regs: t4=0x%lx t5=0x%lx t6=0x%lx t7=0x%lx",
                                  ctx->t4, ctx->t5, ctx->t6, ctx->t7);
        loggers::INTERRUPT::ERROR("regs: t8=0x%lx u0=0x%lx s0=0x%lx s1=0x%lx",
                                  ctx->t8, ctx->u0, ctx->s0, ctx->s1);
        loggers::INTERRUPT::ERROR("regs: s2=0x%lx s3=0x%lx s4=0x%lx s5=0x%lx",
                                  ctx->s2, ctx->s3, ctx->s4, ctx->s5);
        loggers::INTERRUPT::ERROR("regs: s6=0x%lx s7=0x%lx s8=0x%lx", ctx->s6,
                                  ctx->s7, ctx->s8);
    }

    [[noreturn]]
    void unrecoverable(umb_t cause, csr_estat_t estat, const Context *ctx) {
        auto badv = csr_get_badv();
        auto crmd = csr_get_crmd();

        loggers::INTERRUPT::ERROR("=======UNRECOVERABLE EXCEPTION===========");
        loggers::INTERRUPT::ERROR("cause=%s(%lu), badv=0x%lx",
                                  exception_name(cause), cause, badv.value);
        if (ctx == nullptr) {
            loggers::INTERRUPT::ERROR("ctx: null");
        } else {
            loggers::INTERRUPT::ERROR(
                "ctx: crmd=0x%lx, era=0x%lx, kstack_top=%p, estat=0x%lx",
                crmd.value, ctx->era,
                reinterpret_cast<void *>(ctx->kstack_top()), estat.value);
        }

        if (task::TaskManager::initialized()) {
            if (!schd::Scheduler::initialized()) {
                loggers::INTERRUPT::ERROR("task: scheduler not initialized");
            } else {
                auto *tcb = schd::Scheduler::inst().current_tcb();
                if (tcb == nullptr) {
                    loggers::INTERRUPT::ERROR("task: none");
                } else {
                    loggers::INTERRUPT::ERROR(
                        "task: pid=%lu, tid=%lu",
                        tcb->task != nullptr ? tcb->task->pid : 0, tcb->tid);
                }
            }
        }

        dump_regs(ctx);
        loggers::INTERRUPT::ERROR(
            "=======UNRECOVERABLE EXCEPTION END===========");
        while (true) {
        }
    }

    [[noreturn]]
    void paging_unrecoverable(umb_t cause, csr_estat_t estat,
                              const Context *ctx, const char *fault_cause,
                              PageMan &pman) {
        auto badv       = csr_get_badv();
        auto fault_addr = VirAddr(badv.value);
        auto query_res  = pman.query_page(fault_addr);
        if (!query_res.has_value()) {
            loggers::INTERRUPT::ERROR(
                "pte: addr=%p, query_err=%s(%d)", fault_addr.addr(),
                to_cstring(query_res.error()), query_res.error());
        } else {
            auto qres        = query_res.value();
            PageMan::PTE pte = *qres.pte;
            loggers::INTERRUPT::ERROR(
                "pte: addr=%p, value=0x%lx, pa=%p, size=%s, rwx=0x%lx",
                fault_addr.addr(), pte.value,
                PageMan::get_physical_address(pte).addr(),
                page_size_name(qres.size), rwx_cast(PageMan::rwx(pte)));
            loggers::INTERRUPT::ERROR(
                "pte flags: V=%d U=%d G=%d P=%d D=%d COW=%d",
                PageMan::is_valid(pte), PageMan::is_user_accessible(pte),
                PageMan::is_global(pte), PageMan::is_present(pte),
                PageMan::is_dirty(pte), PageMan::is_cow(pte));
        }
        loggers::INTERRUPT::ERROR(
            "paging fault: kind=%s, fault_cause=%s, badv=%p, page=%p",
            page_fault_kind(cause), fault_cause, fault_addr.addr(),
            fault_addr.page_align_down().addr());
        unrecoverable(cause, estat, ctx);
    }

    namespace paging {
        enum class FaultCause {
            NO_PRESENT,
            WRITE_PROTECT,
            READ_PROTECT,
            EXECUTE_PROTECT,
            PRIVILEGE_PROTECT,
            ACCESS_FAULT,
            UNKNOWN
        };

        [[nodiscard]]
        const char *fault_cause_name(FaultCause cause) noexcept {
            switch (cause) {
                case FaultCause::NO_PRESENT:        return "NO_PRESENT";
                case FaultCause::WRITE_PROTECT:     return "WRITE_PROTECT";
                case FaultCause::READ_PROTECT:      return "READ_PROTECT";
                case FaultCause::EXECUTE_PROTECT:   return "EXECUTE_PROTECT";
                case FaultCause::PRIVILEGE_PROTECT: return "PRIVILEGE_PROTECT";
                case FaultCause::ACCESS_FAULT:      return "ACCESS_FAULT";
                case FaultCause::UNKNOWN:
                default:                            return "UNKNOWN";
            }
        }

        [[nodiscard]]
        bool is_kernel_vaddr(VirAddr vaddr) noexcept {
            return !is_user_vaddr(vaddr);
        }

        void log_pte_debug(VirAddr addr, PageMan &pman) {
            auto query_res = pman.query_page(addr);
            if (!query_res.has_value()) {
                loggers::INTERRUPT::DEBUG(
                    "pte: addr=%p, query_err=%s(%d)", addr.addr(),
                    to_cstring(query_res.error()), query_res.error());
                return;
            }
            auto qres        = query_res.value();
            PageMan::PTE pte = *qres.pte;
            loggers::INTERRUPT::DEBUG(
                "pte: addr=%p, value=0x%lx, pa=%p, size=%s, rwx=0x%lx",
                addr.addr(), pte.value,
                PageMan::get_physical_address(pte).addr(),
                page_size_name(qres.size), rwx_cast(PageMan::rwx(pte)));
            loggers::INTERRUPT::DEBUG(
                "pte flags: V=%d U=%d G=%d P=%d D=%d COW=%d",
                PageMan::is_valid(pte), PageMan::is_user_accessible(pte),
                PageMan::is_global(pte), PageMan::is_present(pte),
                PageMan::is_dirty(pte), PageMan::is_cow(pte));
        }

        [[nodiscard]]
        FaultCause confirm_fault_cause(umb_t cause, VirAddr fault_addr,
                                       PageMan &pman) noexcept {
            if (cause == FETCH_ADDRESS_FAULT ||
                cause == MEMORY_ACCESS_ADDRESS_FAULT)
            {
                return FaultCause::ACCESS_FAULT;
            }

            if (cause == PAGE_NOT_READABLE) {
                return FaultCause::READ_PROTECT;
            }
            if (cause == PAGE_NOT_EXECUTABLE) {
                return FaultCause::EXECUTE_PROTECT;
            }
            if (cause == PAGE_PRIVILEGE_VIOLATION) {
                return FaultCause::PRIVILEGE_PROTECT;
            }

            auto query_res = pman.query_page(fault_addr);
            if (!query_res.has_value()) {
                if (query_res.error() == ErrCode::PAGE_NOT_PRESENT) {
                    return FaultCause::NO_PRESENT;
                }
                return FaultCause::UNKNOWN;
            }

            auto qres        = query_res.value();
            PageMan::PTE pte = *qres.pte;
            if (!PageMan::is_present(pte)) {
                return FaultCause::NO_PRESENT;
            }

            if ((cause == STORE_PAGE_INVALID || cause == PAGE_MODIFICATION) &&
                PageMan::is_cow(pte))
            {
                return FaultCause::WRITE_PROTECT;
            }

            auto rwx = PageMan::rwx(pte);
            switch (cause) {
                case LOAD_PAGE_INVALID:
                    return PageMan::is_readable(rwx) ? FaultCause::UNKNOWN
                                                     : FaultCause::READ_PROTECT;
                case STORE_PAGE_INVALID:
                case PAGE_MODIFICATION:
                    return PageMan::is_writable(rwx)
                               ? FaultCause::UNKNOWN
                               : FaultCause::WRITE_PROTECT;
                case FETCH_PAGE_INVALID:
                    return PageMan::is_executable(rwx)
                               ? FaultCause::UNKNOWN
                               : FaultCause::EXECUTE_PROTECT;
                default: return FaultCause::UNKNOWN;
            }
        }

        void paging_fault(umb_t cause, csr_estat_t estat, Context *ctx) {
            auto &e         = env::inst();
            auto fault_addr = VirAddr(csr_get_badv().value);
            auto fault_page = fault_addr.page_align_down();
            loggers::INTERRUPT::DEBUG(
                "paging fault: kind=%s, badv=%p, page=%p, era=0x%lx",
                page_fault_kind(cause), fault_addr.addr(), fault_page.addr(),
                ctx != nullptr ? ctx->era : 0);
            loggers::INTERRUPT::DEBUG("page fault env: pgd=%p, tm=%p",
                                      e.pgd().addr(), e.tmm());

            if (!e.pgd().nonnull()) {
                loggers::INTERRUPT::ERROR("page fault: 当前页表根为空");
                auto null_pman = PageMan(PhyAddr::null);
                exception::paging_unrecoverable(
                    cause, estat, ctx, fault_cause_name(FaultCause::UNKNOWN),
                    null_pman);
            }

            PageMan pman(e.pgd());
            auto fault_cause = confirm_fault_cause(cause, fault_addr, pman);
            bool processed   = false;

            switch (fault_cause) {
                case FaultCause::NO_PRESENT: {
                    if (is_kernel_vaddr(fault_addr)) {
                        auto kernel_pgd = e.main_kernel_pgd();
                        if (!kernel_pgd.nonnull()) {
                            loggers::INTERRUPT::ERROR(
                                "kernel page fault: 主内核页表不可用 addr=%p",
                                fault_addr.addr());
                            break;
                        }
                        PageMan kernel_pman(kernel_pgd);
                        auto clone_res =
                            pman.clone_mapping_from(kernel_pman, fault_page);
                        if (!clone_res.has_value()) {
                            loggers::INTERRUPT::ERROR(
                                "kernel page fault: 复制主内核页表映射失败 "
                                "addr=%p err=%s",
                                fault_addr.addr(),
                                to_cstring(clone_res.error()));
                            break;
                        }
                        PageMan::flush_tlb();
                        loggers::INTERRUPT::DEBUG(
                            "kernel page fault: 已复制主内核页表映射 addr=%p "
                            "page=%p",
                            fault_addr.addr(), fault_page.addr());
                        processed = true;
                        break;
                    }

                    auto *tm = e.tmm();
                    if (tm != nullptr) {
                        loggers::INTERRUPT::DEBUG(
                            "缺页异常可尝试处理: addr=%p, page=%p, tm_pgd=%p",
                            fault_addr.addr(), fault_page.addr(),
                            tm->pgd().addr());
                        processed |= tm->on_np({fault_addr});
                        if (processed) {
                            auto verify_pman = PageMan(PageMan::read_root());
                            log_pte_debug(fault_addr, verify_pman);
                        }
                    }
                    break;
                }
                case FaultCause::WRITE_PROTECT: {
                    auto *tm = e.tmm();
                    if (tm != nullptr) {
                        processed |= tm->on_wp(fault_addr);
                    }
                    if (processed) {
                        loggers::INTERRUPT::DEBUG(
                            "写保护页异常已处理: addr=%p, page=%p",
                            fault_addr.addr(), fault_page.addr());
                        log_pte_debug(fault_addr, pman);
                    }
                    break;
                }
                case FaultCause::READ_PROTECT:
                case FaultCause::EXECUTE_PROTECT:
                case FaultCause::PRIVILEGE_PROTECT:
                case FaultCause::ACCESS_FAULT:
                case FaultCause::UNKNOWN:
                default:                            break;
            }

            if (!processed) {
                exception::paging_unrecoverable(
                    cause, estat, ctx, fault_cause_name(fault_cause), pman);
            }
        }
    }  // namespace paging

    [[nodiscard]]
    bool system_call(csr_estat_t estat, Context *ctx) noexcept {
        (void)estat;
        if (ctx == nullptr) {
            loggers::SYSCALL::ERROR("系统调用异常缺少 trap 上下文");
            return false;
        }
        if (!from_umode()) {
            loggers::SYSCALL::ERROR("拒绝处理非用户态系统调用: era=0x%lx",
                                    ctx->era);
            return false;
        }
        if (!schd::Scheduler::initialized()) {
            loggers::SYSCALL::ERROR("系统调用异常时调度器未初始化");
            return false;
        }

        env::inst().trap_context(env::key::trap_context()) = ctx;
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        assert(current_tcb != nullptr);

        syscall::ArgPack args = read_args(*ctx);
        loggers::SYSCALL::DEBUG(
            "系统调用触发: name=%s, no=0x%016lx, pid=%lu, tid=%lu",
            syscall::name_of(args.syscall_number), args.syscall_number,
            current_tcb->task != nullptr ? current_tcb->task->pid : 0,
            current_tcb->tid);
        loggers::SYSCALL::DEBUG(
            "系统调用参数: capidx=0x%016lx, arg0=0x%016lx, arg1=0x%016lx, "
            "arg2=0x%016lx",
            args.args[0], args.args[1], args.args[2], args.args[3]);
        loggers::SYSCALL::DEBUG(
            "系统调用参数: arg3=0x%016lx, arg4=0x%016lx, arg5=0x%016lx, "
            "era=0x%016lx",
            args.args[4], args.args[5], args.args[6], ctx->era);

        ctx->era += 4;
        syscall::handle_user_ecall(util::nnullforce(current_tcb),
                                   util::nnullforce(ctx), args);
        env::inst().trap_context(env::key::trap_context()) = nullptr;
        return true;
    }

    void exception(umb_t cause, csr_estat_t estat, Context *ctx) {
        switch (cause) {
            case SYSTEM_CALL:
                if (!system_call(estat, ctx)) {
                    unrecoverable(cause, estat, ctx);
                }
                break;
            case LOAD_PAGE_INVALID:
            case STORE_PAGE_INVALID:
            case FETCH_PAGE_INVALID:
            case PAGE_MODIFICATION:
            case PAGE_NOT_READABLE:
            case PAGE_NOT_EXECUTABLE:
            case PAGE_PRIVILEGE_VIOLATION:
                paging::paging_fault(cause, estat, ctx);
                break;
            default: unrecoverable(cause, estat, ctx); break;
        }
    }
}  // namespace exception

namespace interrupt {
    void dispatch(csr_estat_t estat, Context *ctx) {
        (void)ctx;
        auto &irq_manager = device::DeviceModel::inst().interrupt();
        auto *cpu = env::hart_ctx != nullptr ? env::hart_ctx->cpu() : nullptr;
        if (cpu == nullptr) {
            loggers::INTERRUPT::ERROR("当前 hart 缺少 CPU, 无法分发中断");
            return;
        }

        auto root_domain_res = irq_manager.get_domain(cpu->local_intc());
        if (!root_domain_res.has_value()) {
            loggers::INTERRUPT::ERROR("获取根中断域失败: %s",
                                      to_cstring(root_domain_res.error()));
            return;
        }
        auto &root_domain = root_domain_res.value().get();
        auto &chip        = root_domain.chip();
        if (chip.compatible() != la64::CpuICChip::COMPATIBLE_STRING) {
            loggers::INTERRUPT::ERROR(
                "根中断域的中断控制器不兼容: 期望兼容 %s, 实际兼容 %s",
                la64::CpuICChip::COMPATIBLE_STRING, chip.compatible().data());
            return;
        }
        auto &cpuic = static_cast<la64::CpuICChip &>(chip);

        for (umb_t bit = 0; bit <= la64::CpuICChip::PMC_IRQ; ++bit) {
            if ((estat.is & (1ULL << bit)) == 0) {
                continue;
            }

            Result<void> post_res = Result<void>{};
            if (bit == la64::CpuICChip::TIMER_IRQ) {
                post_res = cpuic.post_timer();
            } else if (bit == la64::CpuICChip::PMC_IRQ) {
                post_res = cpuic.post_pmc();
            } else if (bit >= la64::CpuICChip::HWI_BEGIN &&
                       bit <= la64::CpuICChip::HWI_END)
            {
                post_res = cpuic.post_hw(
                    static_cast<int>(bit - la64::CpuICChip::HWI_BEGIN));
            } else {
                loggers::INTERRUPT::WARN("忽略未支持的本地中断 hwirq=%llu",
                                         static_cast<unsigned long long>(bit));
                continue;
            }

            if (!post_res.has_value()) {
                loggers::INTERRUPT::ERROR("中断分发失败: hwirq=%llu err=%s",
                                          static_cast<unsigned long long>(bit),
                                          to_cstring(post_res.error()));
            }
        }
    }
}  // namespace interrupt

extern "C" void isr_entry();

extern "C" void handle_trap(umb_t era, csr_estat_t estat, Context *ctx) {
    loggers::EXCEPTION::DEBUG("trap: ecode=%llu era=%p ctx=%p from_%s",
                              static_cast<unsigned long long>(estat.ecode),
                              (void *)era, ctx,
                              exception::from_umode() ? "umode" : "smode");
    if (task::TaskManager::initialized()) {
        task::TaskManager::inst().reap_recycled();
    }
    if (estat.ecode == ECODE_INT) {
        interrupt::dispatch(estat, ctx);
    } else {
        exception::exception(estat.ecode, estat, ctx);
    }
    Interrupt::sti();
    if (schd::Scheduler::initialized()) {
        schd::Scheduler::inst().schedule();
    }
    Interrupt::cli();
}

void Interrupt::init() {
    auto isr_addr = reinterpret_cast<umb_t>(&isr_entry);
    LA64_CSR_WRITE(CSR_EENTRY, isr_addr);
    loggers::INTERRUPT::INFO("LoongArch64 isr_entry 已设置: 0x%lx", isr_addr);
}

void Interrupt::sti() {
    LA64_CSR_SET(CSR_CRMD, CRMD_IE);
}

void Interrupt::cli() {
    LA64_CSR_CLEAR(CSR_CRMD, CRMD_IE);
}

bool Interrupt::enabled() {
    return (LA64_CSR_READ(CSR_CRMD) & CRMD_IE) != 0;
}
