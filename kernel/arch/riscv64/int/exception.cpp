/**
 * @file exception.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/callconv.h>
#include <arch/riscv64/csr.h>
#include <arch/riscv64/intc.h>
#include <arch/riscv64/trait.h>
#include <device/model.h>
#include <env.h>
#include <logger.h>
#include <sus/logger.h>
#include <sus/types.h>
#include <syscall/syscall.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/wait.h>

#include <new>

using namespace rv64;

namespace exception {

    constexpr umb_t INST_MISALIGNED    = 0;   // 指令地址不对齐
    constexpr umb_t INST_ACCESS_FAULT  = 1;   // 指令访问错误
    constexpr umb_t ILLEGAL_INST       = 2;   // 非法指令
    constexpr umb_t BREAKPOINT         = 3;   // 断点
    constexpr umb_t LOAD_MISALIGNED    = 4;   // 加载地址
    constexpr umb_t LOAD_ACCESS_FAULT  = 5;   // 加载访问错误
    constexpr umb_t STORE_MISALIGNED   = 6;   // 存储
    constexpr umb_t STORE_ACCESS_FAULT = 7;   // 存储访问错误
    constexpr umb_t ECALL_U            = 8;   // 用户模式环境调用
    constexpr umb_t ECALL_S            = 9;   // 监管模式环境调用
    constexpr umb_t RESERVED_0         = 10;  // 保留
    constexpr umb_t RESERVED_1         = 11;  // 保留
    constexpr umb_t INST_PAGE_FAULT    = 12;  // 取指页
    constexpr umb_t LOAD_PAGE_FAULT    = 13;  // 加载页错误
    constexpr umb_t RESERVED_2         = 14;  // 保留
    constexpr umb_t STORE_PAGE_FAULT   = 15;  // 存储页
    constexpr umb_t RESERVED_3         = 16;  // 保留
    constexpr umb_t RESERVED_4         = 17;  // 保留
    constexpr umb_t SOFTWARE_CHECK     = 18;  // 软件检查异常
    constexpr umb_t HARDWARE_EEROR     = 19;  // 硬件错误

    const char *MSG[] = {"指令地址不对齐",
                         "指令访问错误",
                         "非法指令",
                         "断点",
                         "加载地址不对齐",
                         "加载访问错误",
                         "存储地址不对齐",
                         "存储访问错误",
                         "用户模式环境调用",
                         "监管模式环境调用",
                         "保留",
                         "保留",
                         "取指页错误",
                         "加载页错误",
                         "保留",
                         "存储页错误",
                         "保留",
                         "保留",
                         "软件检查异常",
                         "硬件错误"};

    /**
     * @brief 获取异常号对应的可读名称.
     *
     * @param cause 异常号.
     * @return const char* 异常名称.
     */
    [[nodiscard]]
    const char *exception_name(umb_t cause) noexcept {
        if (cause < sizeof(MSG) / sizeof(MSG[0])) {
            return MSG[cause];
        }
        return "未知";
    }

    /**
     * @brief 获取 trap 上下文对应的特权级名称.
     *
     * @param ctx trap 上下文.
     * @return const char* 特权级名称.
     */
    [[nodiscard]]
    const char *privilege_name(const Context *ctx) noexcept {
        if (ctx == nullptr) {
            return "未知";
        }
        return ctx->sstatus.spp ? "S-Mode" : "U-Mode";
    }

    /**
     * @brief 判断异常是否属于标准页错误.
     *
     * @param cause 异常号.
     * @return true 异常为 instruction/load/store page fault.
     * @return false 异常不是标准页错误.
     */
    [[nodiscard]]
    bool is_page_fault_exception(umb_t cause) noexcept {
        return cause == INST_PAGE_FAULT || cause == LOAD_PAGE_FAULT ||
               cause == STORE_PAGE_FAULT;
    }

    /**
     * @brief 判断异常是否属于数据访问错误.
     *
     * @param cause 异常号.
     * @return true 异常为 load/store access fault.
     * @return false 异常不是 load/store access fault.
     */
    [[nodiscard]]
    bool is_access_fault_exception(umb_t cause) noexcept {
        return cause == LOAD_ACCESS_FAULT || cause == STORE_ACCESS_FAULT;
    }

    /**
     * @brief 判断异常是否应纳入统一页异常分析链路.
     *
     * @param cause 异常号.
     * @return true 异常属于页相关异常或访问错误.
     * @return false 异常不属于页相关异常分析范围.
     */
    [[nodiscard]]
    bool is_paging_related_exception(umb_t cause) noexcept {
        return is_page_fault_exception(cause) ||
               is_access_fault_exception(cause);
    }

    /**
     * @brief 获取页相关异常的分类字符串.
     *
     * @param cause 异常号.
     * @return const char* 页相关异常分类名称.
     */
    [[nodiscard]]
    const char *page_fault_kind(umb_t cause) noexcept {
        switch (cause) {
            case INST_PAGE_FAULT:    return "instruction-page";
            case LOAD_PAGE_FAULT:    return "load-page";
            case STORE_PAGE_FAULT:   return "store-page";
            case LOAD_ACCESS_FAULT:  return "load-access";
            case STORE_ACCESS_FAULT: return "store-access";
            default:                 return "not-paging-fault";
        }
    }

    /**
     * @brief 获取页大小枚举对应的字符串.
     *
     * @param size 页大小枚举.
     * @return const char* 页大小名称.
     */
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

    void dump_regs(const Context *ctx) {
        if (ctx == nullptr) {
            loggers::EXCEPTION::ERROR("regs: ctx=null");
            return;
        }

        loggers::EXCEPTION::ERROR(
            "regs: ra=0x%016lx  sp=0x%016lx  gp=0x%016lx tp=0x%016lx", ctx->ra,
            ctx->sp(), ctx->gp, ctx->tp);
        loggers::EXCEPTION::ERROR(
            "regs: t0=0x%016lx  t1=0x%016lx  t2=0x%016lx s0=0x%016lx", ctx->t0,
            ctx->t1, ctx->t2, ctx->s0);
        loggers::EXCEPTION::ERROR(
            "regs: s1=0x%016lx  a0=0x%016lx  a1=0x%016lx a2=0x%016lx", ctx->s1,
            ctx->a0, ctx->a1, ctx->a2);
        loggers::EXCEPTION::ERROR(
            "regs: a3=0x%016lx  a4=0x%016lx  a5=0x%016lx a6=0x%016lx", ctx->a3,
            ctx->a4, ctx->a5, ctx->a6);
        loggers::EXCEPTION::ERROR(
            "regs: a7=0x%016lx  s2=0x%016lx  s3=0x%016lx s4=0x%016lx", ctx->a7,
            ctx->s2, ctx->s3, ctx->s4);
        loggers::EXCEPTION::ERROR(
            "regs: s5=0x%016lx  s6=0x%016lx  s7=0x%016lx s8=0x%016lx", ctx->s5,
            ctx->s6, ctx->s7, ctx->s8);
        loggers::EXCEPTION::ERROR(
            "regs: s9=0x%016lx s10=0x%016lx s11=0x%016lx t3=0x%016lx", ctx->s9,
            ctx->s10, ctx->s11, ctx->t3);
        loggers::EXCEPTION::ERROR("regs: t4=0x%016lx  t5=0x%016lx  t6=0x%016lx",
                                  ctx->t4, ctx->t5, ctx->t6);
    }

    [[noreturn]]
    void unrecoverable(csr_scause_t scause, umb_t stval, const Context *ctx) {
        loggers::EXCEPTION::ERROR("=======UNRECOVERABLE EXCEPTION===========");
        loggers::EXCEPTION::ERROR("cause=%s(%lu), stval=0x%016lx",
                                  exception_name(scause.cause), scause.cause,
                                  stval);
        if (ctx == nullptr) {
            loggers::EXCEPTION::ERROR("ctx: null");
        } else {
            loggers::EXCEPTION::ERROR(
                "ctx: sstatus=0x%016lx, sepc=0x%016lx, kstack_top=%p",
                ctx->sstatus.value, ctx->sepc,
                reinterpret_cast<void *>(ctx->kstack_top()));
        }

        if (task::TaskManager::initialized()) {
            if (!schd::Scheduler::initialized()) {
                loggers::EXCEPTION::ERROR("task: scheduler not initialized");
            } else {
                auto *tcb = schd::Scheduler::inst().current_tcb();
                if (tcb == nullptr) {
                    loggers::EXCEPTION::ERROR("task: none");
                } else {
                    loggers::EXCEPTION::ERROR(
                        "task: pid=%lu, tid=%lu",
                        tcb->task != nullptr ? tcb->task->pid : 0, tcb->tid);
                }
            }
        }

        dump_regs(ctx);
        loggers::EXCEPTION::ERROR(
            "=======UNRECOVERABLE EXCEPTION END===========");
        while (true) {
        }
    }

    void log_current_task_error() {
        if (!schd::Scheduler::initialized()) {
            loggers::EXCEPTION::ERROR("task: scheduler not initialized");
            return;
        }

        auto *tcb = schd::Scheduler::inst().current_tcb();
        if (tcb == nullptr) {
            loggers::EXCEPTION::ERROR("task: none");
            return;
        }
        loggers::EXCEPTION::ERROR(
            "task: pid=%lu, tid=%lu, state=%d, kstack_bottom=%p",
            tcb->task != nullptr ? tcb->task->pid : 0, tcb->tid,
            static_cast<int>(tcb->basic_entity.state), tcb->kstack_bottom);
    }

    void log_trap_context_error(csr_scause_t scause, umb_t sepc, umb_t stval,
                                const Context *ctx) {
        loggers::EXCEPTION::ERROR(
            "trap: cause=%s(%lu), scause=0x%016lx, sepc=0x%016lx, "
            "stval=0x%016lx",
            exception_name(scause.cause), scause.cause, scause.value, sepc,
            stval);
        if (ctx == nullptr) {
            loggers::EXCEPTION::ERROR("ctx: null");
            return;
        }
        loggers::EXCEPTION::ERROR(
            "ctx: ptr=%p, mode=%s, sstatus=0x%016lx, sp=0x%016lx, ra=0x%016lx",
            ctx, privilege_name(ctx), ctx->sstatus.value, ctx->sp(), ctx->ra);
        loggers::EXCEPTION::ERROR(
            "args: a0=0x%016lx, a1=0x%016lx, a2=0x%016lx, a3=0x%016lx", ctx->a0,
            ctx->a1, ctx->a2, ctx->a3);
        loggers::EXCEPTION::ERROR(
            "args: a4=0x%016lx, a5=0x%016lx, a6=0x%016lx, a7=0x%016lx", ctx->a4,
            ctx->a5, ctx->a6, ctx->a7);
    }

    namespace paging {
        [[nodiscard]]
        bool is_kernel_vaddr(VirAddr vaddr) noexcept {
            return !is_user_vaddr(vaddr);
        }

        void log_pte_debug(VirAddr addr, PageMan &pman) {
            auto query_res = pman.query_page(addr);
            if (!query_res.has_value()) {
                loggers::EXCEPTION::DEBUG(
                    "pte: addr=%p, query_err=%s(%d)", addr.addr(),
                    to_cstring(query_res.error()), query_res.error());
                return;
            }
            auto qres        = query_res.value();
            PageMan::PTE pte = *qres.pte;
            loggers::EXCEPTION::DEBUG(
                "pte: addr=%p, value=0x%016lx, pa=%p, size=%s, rwx=0x%016lx",
                addr.addr(), pte.value,
                PageMan::get_physical_address(pte).addr(),
                page_size_name(qres.size), pte.rwx);
            loggers::EXCEPTION::DEBUG(
                "pte flags: V=%d U=%d G=%d A=%d D=%d NP=%d COW=%d", pte.v,
                pte.u, pte.g, pte.a, pte.d, pte.np, PageMan::is_cow(pte));
        }

        void log_pte_error(VirAddr addr, PageMan &pman) {
            auto query_res = pman.query_page(addr);
            if (!query_res.has_value()) {
                loggers::EXCEPTION::ERROR(
                    "pte: addr=%p, query_err=%s(%d)", addr.addr(),
                    to_cstring(query_res.error()), query_res.error());
                return;
            }
            auto qres        = query_res.value();
            PageMan::PTE pte = *qres.pte;
            loggers::EXCEPTION::ERROR(
                "pte: addr=%p, value=0x%016lx, pa=%p, size=%s, rwx=0x%016lx",
                addr.addr(), pte.value,
                PageMan::get_physical_address(pte).addr(),
                page_size_name(qres.size), pte.rwx);
            loggers::EXCEPTION::ERROR(
                "pte flags: V=%d U=%d G=%d A=%d D=%d NP=%d COW=%d", pte.v,
                pte.u, pte.g, pte.a, pte.d, pte.np, PageMan::is_cow(pte));
        }

        enum class FaultCause {
            NO_PRESENT,       // No present page
            SAU_NO_SUM,       // S-mode Access User page without SUM
            SEU,              // S-mode Execute User page
            UAS,              // User Access Supervisor page
            ACCESS_FAULT,     // Access fault after address translation
            INVALID_AD,       // Invalid A/D bit
            WRITE_PROTECT,    // Write protection
            EXECUTE_PROTECT,  // Execute protection
            UNKNOWN           // Unknown cause
        };

        /**
         * @brief 获取页异常分析结果的可读名称.
         *
         * @param cause 页异常分析结果.
         * @return const char* 分析结果名称.
         */
        [[nodiscard]]
        static const char *fault_cause_name(FaultCause cause) noexcept {
            switch (cause) {
                case FaultCause::NO_PRESENT:      return "NO_PRESENT";
                case FaultCause::SAU_NO_SUM:      return "SAU_NO_SUM";
                case FaultCause::SEU:             return "SEU";
                case FaultCause::UAS:             return "UAS";
                case FaultCause::ACCESS_FAULT:    return "ACCESS_FAULT";
                case FaultCause::INVALID_AD:      return "INVALID_AD";
                case FaultCause::WRITE_PROTECT:   return "WRITE_PROTECT";
                case FaultCause::EXECUTE_PROTECT: return "EXECUTE_PROTECT";
                case FaultCause::UNKNOWN:
                default:                          return "UNKNOWN";
            }
        }

        /**
         * @brief 记录页相关异常的完整错误信息.
         *
         * @param scause trap cause.
         * @param sepc 异常发生时的 PC.
         * @param stval 异常关联地址.
         * @param ctx trap 上下文.
         * @param cause 页异常分析结果.
         * @param pman 当前页表管理器.
         */
        static void log_paging_fault_error(csr_scause_t scause, umb_t sepc,
                                           umb_t stval, const Context *ctx,
                                           FaultCause cause, PageMan &pman) {
            VirAddr fault_addr = VirAddr(stval);
            loggers::EXCEPTION::ERROR(
                "paging fault: kind=%s, fault_cause=%s, addr=%p, page=%p",
                page_fault_kind(scause.cause), fault_cause_name(cause),
                fault_addr.addr(), fault_addr.page_align_down().addr());
            log_trap_context_error(scause, sepc, stval, ctx);
            log_current_task_error();
            log_pte_error(fault_addr, pman);
        }

        [[noreturn]]
        static void paging_unrecoverable(csr_scause_t scause, umb_t stval,
                                         const Context *ctx, FaultCause cause,
                                         PageMan &pman) {
            VirAddr fault_addr = VirAddr(stval);
            loggers::EXCEPTION::ERROR(
                "paging fault: kind=%s, fault_cause=%s, addr=%p, page=%p",
                page_fault_kind(scause.cause), fault_cause_name(cause),
                fault_addr.addr(), fault_addr.page_align_down().addr());
            log_pte_error(fault_addr, pman);
            exception::unrecoverable(scause, stval, ctx);
        }

        /**
         * @brief 分析页相关异常的可能原因.
         *
         * @param scause trap cause.
         * @param fault_addr 异常虚拟地址.
         * @param ctx trap 上下文.
         * @param pman 当前页表管理器.
         * @return FaultCause 页异常原因分类.
         */
        [[nodiscard]]
        static FaultCause confirm_fault_cause(const csr_scause_t &scause,
                                              const VirAddr &fault_addr,
                                              const Context *ctx,
                                              PageMan &pman) {
            if (!is_paging_related_exception(scause.cause)) {
                return FaultCause::UNKNOWN;
            }

            auto query_res = pman.query_page(fault_addr);
            if (!query_res.has_value()) {
                if (is_access_fault_exception(scause.cause)) {
                    loggers::EXCEPTION::DEBUG(
                        "access fault analysis: addr=%p, page lookup failed "
                        "err=%s(%d)",
                        fault_addr.addr(), to_cstring(query_res.error()),
                        query_res.error());
                    return FaultCause::ACCESS_FAULT;
                }
                // 页面不存在
                if (query_res.error() == ErrCode::PAGE_NOT_PRESENT) {
                    return FaultCause::NO_PRESENT;
                }
                loggers::EXCEPTION::ERROR("查询页表时发生错误: addr=%p, err=%d",
                                          fault_addr.addr(), query_res.error());
                return FaultCause::UNKNOWN;
            }

            auto qres                  = query_res.value();
            typename PageMan::PTE *pte = qres.pte;

            // 页面不存在
            if (!PageMan::is_present(*pte)) {
                if (is_access_fault_exception(scause.cause)) {
                    loggers::EXCEPTION::DEBUG(
                        "access fault analysis: addr=%p, mapped but not "
                        "present",
                        fault_addr.addr());
                    return FaultCause::ACCESS_FAULT;
                }
                return FaultCause::NO_PRESENT;
            }

            if (is_access_fault_exception(scause.cause)) {
                loggers::EXCEPTION::DEBUG(
                    "access fault analysis: addr=%p, mapped page present, "
                    "treat as unrecoverable access fault",
                    fault_addr.addr());
                return FaultCause::ACCESS_FAULT;
            }

            int acc_priv = ctx->sstatus.spp
                               ? 1
                               : 0;  // 访问发生时的特权级, 0=用户态, 1=内核态
            int pte_priv =
                pte->u ? 0 : 1;  // 页表项的特权级, 0=用户页, 1=内核页
            int priv_gap =
                acc_priv - pte_priv;  // 访问特权级与页表项特权级的差值
                                      // = 0 说明访问特权级与页表项特权级相同
                                      // > 0 说明访问特权级比页表项特权级高
                                      // < 0 说明访问特权级比页表项特权级低

            // 访问特权级比页表项特权级高, 说明是内核态访问用户页
            if (priv_gap > 0) {
                if (scause.cause == INST_PAGE_FAULT) {
                    return FaultCause::SEU;
                } else if (!ctx->sstatus.sum) {
                    return FaultCause::SAU_NO_SUM;
                }
            }

            // PTE为S页, 但访问发生在U态
            if (priv_gap < 0) {
                return FaultCause::UAS;
            }

            PageMan::RWX rwx = PageMan::rwx(*pte);
            // 存储页异常且页面不可写
            if ((scause.cause == STORE_PAGE_FAULT) &&
                !PageMan::is_writable(rwx))
            {
                return FaultCause::WRITE_PROTECT;
            }
            // 取指页异常且页面不可执行
            if ((scause.cause == INST_PAGE_FAULT) &&
                !PageMan::is_executable(rwx))
            {
                return FaultCause::EXECUTE_PROTECT;
            }
            // 取指页异常且是高特权级访问用户页
            if ((scause.cause == INST_PAGE_FAULT) &&
                !PageMan::is_executable(rwx))
            {
                return FaultCause::EXECUTE_PROTECT;
            }

            // AD位错误
            if (!pte->a) {
                return FaultCause::INVALID_AD;
            }
            // 存储页异常且D位未设置但页面可写
            if ((scause.cause == STORE_PAGE_FAULT) && !pte->d &&
                PageMan::is_writable(rwx))
            {
                return FaultCause::INVALID_AD;
            }

            return FaultCause::UNKNOWN;
        }

        /**
         * @brief 处理页相关异常并在可恢复时完成修复.
         *
         * @param scause trap cause.
         * @param sepc 异常发生时的 PC.
         * @param stval 异常关联地址.
         * @param ctx trap 上下文.
         * @return true 异常已恢复, 可以继续执行.
         * @return false 异常不可恢复, 需由上层终止执行流.
         */
        void paging_fault(csr_scause_t scause, umb_t sepc, umb_t stval,
                          Context *ctx) {
            const VirAddr fault_addr = VirAddr(stval);
            const VirAddr fault_page = fault_addr.page_align_down();
            auto &e                  = env::inst();
            loggers::EXCEPTION::DEBUG(
                "paging fault: kind=%s, addr=%p, page=%p, sepc=0x%016lx",
                page_fault_kind(scause.cause), fault_addr.addr(),
                fault_page.addr(), sepc);
            loggers::EXCEPTION::DEBUG("page fault env: pgd=%p, tm=%p",
                                      e.pgd().addr(), e.tmm());
            PageMan pman(e.pgd());

            if (!e.pgd().nonnull()) {
                loggers::EXCEPTION::ERROR("page fault: 当前页表根为空");
                log_trap_context_error(scause, sepc, stval, ctx);
                log_current_task_error();
                paging_unrecoverable(scause, stval, ctx, FaultCause::UNKNOWN,
                                     pman);
            }

            FaultCause cause =
                confirm_fault_cause(scause, fault_addr, ctx, pman);

            bool processed = false;

            // 在此处可以允许中断发生

            switch (cause) {
                case FaultCause::NO_PRESENT: {
                    // 如果是内核地址, 则尝试复制主内核页表的映射
                    if (is_kernel_vaddr(fault_addr)) {
                        auto kernel_pgd = e.main_kernel_pgd();
                        if (!kernel_pgd.nonnull()) {
                            loggers::EXCEPTION::ERROR(
                                "kernel page fault: 主内核页表不可用 addr=%p",
                                fault_addr.addr());
                            break;
                        }

                        PageMan kernel_pman(kernel_pgd);
                        auto clone_res =
                            pman.clone_mapping_from(kernel_pman, fault_page);
                        if (!clone_res.has_value()) {
                            loggers::EXCEPTION::ERROR(
                                "kernel page fault: 复制主内核页表映射失败 "
                                "addr=%p err=%s",
                                fault_addr.addr(),
                                to_cstring(clone_res.error()));
                            break;
                        }
                        PageMan::flush_tlb();
                        loggers::EXCEPTION::DEBUG(
                            "kernel page fault: 已复制主内核页表映射 addr=%p "
                            "page=%p",
                            fault_addr.addr(), fault_page.addr());
                        processed = true;
                        break;
                    }

                    // 使用缺页异常处理程序处理缺页异常
                    auto *tm = e.tmm();
                    if (tm != nullptr) {
                        loggers::EXCEPTION::DEBUG(
                            "缺页异常可尝试处理: addr=%p, page=%p, tm_pgd=%p",
                            fault_addr.addr(), fault_page.addr(),
                            tm->pgd().addr());
                        processed |= tm->on_np({fault_addr});

                        // for debug:
                        if (processed) {
                            // 再次查询, 验证该页已被映射
                            PhyAddr hw_root_after = PageMan::read_root();
                            PageMan verify_pman(hw_root_after);
                            auto verify_res =
                                verify_pman.query_page(fault_addr);
                            if (!verify_res.has_value()) {
                                loggers::EXCEPTION::ERROR(
                                    "TM::on_np 返回成功但页面仍不存在: "
                                    "addr=%p, "
                                    "err=%d, hw_root_after=%p",
                                    fault_addr.addr(), verify_res.error(),
                                    hw_root_after.addr());
                            } else {
                                loggers::EXCEPTION::DEBUG(
                                    "缺页异常已处理: addr=%p, page=%p, "
                                    "hw_root_after=%p",
                                    fault_addr.addr(), fault_page.addr(),
                                    hw_root_after.addr());
                                log_pte_debug(fault_addr, verify_pman);
                            }
                        }
                    }
                    break;
                }
                case FaultCause::SAU_NO_SUM: {
                    break;
                }
                case FaultCause::UAS: {
                    break;
                }
                case FaultCause::SEU: {
                    break;
                }
                case FaultCause::ACCESS_FAULT: {
                    loggers::EXCEPTION::ERROR(
                        "access fault is not recoverable: type=%s, addr=%p, "
                        "sepc=0x%016lx",
                        exception_name(scause.cause), fault_addr.addr(), sepc);
                    break;
                }
                case FaultCause::INVALID_AD: {
                    auto query_res = pman.query_page(fault_addr);
                    if (!query_res.has_value()) {
                        loggers::EXCEPTION::ERROR(
                            "处理 A/D 位错误时查询页表失败: addr=%p, err=%d",
                            fault_addr.addr(), query_res.error());
                        break;
                    }
                    PageMan::PTE *pte = query_res.value().pte;
                    bool present      = PageMan::is_present(*pte);
                    if (!present) {
                        loggers::EXCEPTION::ERROR(
                            "处理 A/D 位错误时页面不存在: addr=%p, pte=%p",
                            fault_addr.addr(), pte);
                        break;
                    }

                    bool updated = false;

                    if (!pte->a) {
                        cause = FaultCause::INVALID_AD;
                    }

                    PageMan::RWX rwx = PageMan::rwx(*pte);
                    if ((scause.cause == STORE_PAGE_FAULT) && !pte->d &&
                        PageMan::is_writable(rwx))
                    {
                        cause = FaultCause::INVALID_AD;
                    }

                    processed |= updated;
                    if (updated) {
                        PageMan::flush_tlb();
                        loggers::EXCEPTION::DEBUG(
                            "修复 A/D 位后重试: addr=%p, A=%d, D=%d",
                            fault_addr.addr(), pte->a, pte->d);
                    }
                    break;
                }
                case FaultCause::WRITE_PROTECT: {
                    auto *tm = e.tmm();
                    if (tm != nullptr) {
                        processed |= tm->on_wp(fault_addr);
                    }
                    if (processed) {
                        loggers::EXCEPTION::DEBUG(
                            "写保护页异常已处理: addr=%p, page=%p",
                            fault_addr.addr(), fault_page.addr());
                        log_pte_debug(fault_addr, pman);
                    }
                    break;
                }
                case FaultCause::EXECUTE_PROTECT: {
                    break;
                }
                default: {
                    break;
                }
            }

            if (!processed) {
                paging_unrecoverable(scause, stval, ctx, cause, pman);
            }

            // 接下来应该执行页异常相关处理
            // 1. 检查地址是否合法
            // 2. 如果是缺页, 则分配物理页并映射
            // 2.1 如果是缺页, 且该页属于虚拟内存, 则从磁盘中调入
            // 2.2 如果是写保护错误, 则检查是否为写时复制
            // 2.3 更新页表项和TLB
            // 3. 如果是权限错误, 则终止相关进程
        }
    }  // namespace paging

    /**
     * @brief 处理非法指令异常.
     *
     * @param scause trap cause.
     * @param sepc 异常发生时的 PC.
     * @param stval 异常关联值.
     * @param ctx trap 上下文.
     * @return true 异常已恢复.
     * @return false 异常不可恢复.
     */
    [[nodiscard]]
    bool illegal_instruction(csr_scause_t scause, umb_t sepc, umb_t stval,
                             Context *ctx) {
        loggers::EXCEPTION::DEBUG(
            "进入非法指令异常处理程序: sepc=0x%016lx, stval=0x%016lx", sepc,
            stval);
        (void)scause;
        (void)ctx;
        return false;  // 无法处理该异常
    }

    [[nodiscard]]
    bool on_ecall_u(umb_t sepc, Context *ctx) noexcept {
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
            "sepc=0x%016lx",
            args.args[4], args.args[5], args.args[6], sepc);

        ctx->sepc += 4;
        syscall::handle_user_ecall(util::nnullforce(current_tcb),
                                   util::nnullforce(ctx), args);
        env::inst().trap_context(env::key::trap_context()) = nullptr;
        return true;
    }

    /**
     * @brief 分发并处理同步异常.
     *
     * @param scause trap cause.
     * @param sepc 异常发生时的 PC.
     * @param stval 异常关联值.
     * @param ctx trap 上下文.
     */
    void dispatch(csr_scause_t scause, umb_t sepc, umb_t stval, Context *ctx) {
        bool processed = false;
        switch (scause.cause) {
            case ECALL_U: processed = on_ecall_u(sepc, ctx); break;
            case ILLEGAL_INST:
                processed = illegal_instruction(scause, sepc, stval, ctx);
                break;
            case INST_PAGE_FAULT:
            case LOAD_PAGE_FAULT:
            case STORE_PAGE_FAULT:
            case LOAD_ACCESS_FAULT:
            case STORE_ACCESS_FAULT:
                paging::paging_fault(scause, sepc, stval, ctx);
                processed = true;
                break;
            default:
                loggers::EXCEPTION::ERROR("无异常处理程序!");
                processed = false;
                break;
        }

        if (!processed) {
            unrecoverable(scause, stval, ctx);
        }
    }
}  // namespace exception

namespace interrupt {
    void dispatch(csr_scause_t scause, umb_t sepc, umb_t stval, Context *ctx) {
        auto &irq_manager = device::DeviceModel::inst().interrupt();
        auto *cpu = env::hart_ctx != nullptr ? env::hart_ctx->cpu() : nullptr;
        if (cpu == nullptr) {
            loggers::INTERRUPT::ERROR("当前 hart 缺少 CPU, 无法分发中断");
            return;
        }

        // scause.cause = hwirq
        driver::hwirq_t hwirq = scause.cause;

        auto root_domain_res = irq_manager.get_domain(cpu->local_intc());
        if (!root_domain_res.has_value()) {
            loggers::INTERRUPT::ERROR("获取根中断域失败: %s",
                                      to_cstring(root_domain_res.error()));
            return;
        }
        auto &root_domain = root_domain_res.value().get();
        auto &chip        = root_domain.chip();
        if (chip.compatible() != riscv::IntC::COMPATIBLE_STRING) {
            loggers::INTERRUPT::ERROR(
                "根中断域的中断控制器不兼容: 期望兼容 %s, 实际兼容 %s",
                riscv::IntC::COMPATIBLE_STRING, chip.compatible().data());
            return;
        }
        auto &riscv_intc = static_cast<riscv::IntC &>(chip);
        switch (hwirq) {
            case riscv::IntC::CLOCK_LOCAL_IRQ_S: {
                auto post_res = riscv_intc.post_timer();
                if (!post_res.has_value()) {
                    loggers::INTERRUPT::ERROR("中断分发失败: %s",
                                              to_cstring(post_res.error()));
                }
                return;
            }
            case riscv::IntC::EXTERNAL_LOCAL_IRQ: {
                auto post_res = riscv_intc.post_external();
                if (!post_res.has_value()) {
                    loggers::INTERRUPT::ERROR("中断分发失败: %s",
                                              to_cstring(post_res.error()));
                }
                return;
            }
            default:
                loggers::INTERRUPT::WARN("忽略未支持的本地中断 hwirq=%llu",
                                         static_cast<unsigned long long>(hwirq));
                return;
        }
    }
}  // namespace interrupt

extern "C" void handle_trap(csr_scause_t scause, umb_t sepc, umb_t stval,
                            Context *ctx) {
    if (!scause.interrupt) {
        loggers::EXCEPTION::DEBUG(
            "trap: cause=%llu sepc=%p stval=%p ctx=%p sp(before fault)=%p "
            "kstack_sp=%p from_%s",
            static_cast<unsigned long long>(scause.cause), (void *)sepc,
            (void *)stval, ctx, (void *)ctx->sp(), (void *)ctx->kstack_sp,
            ctx->sstatus.spp ? "smode" : "umode");
    } else {
        loggers::INTERRUPT::DEBUG(
            "interrupt: cause=%llu sepc=%p stval=%p ctx=%p sp(before irq)=%p "
            "kstack_sp=%p from_%s",
            static_cast<unsigned long long>(scause.cause), (void *)sepc,
            (void *)stval, ctx, (void *)ctx->sp(), (void *)ctx->kstack_sp,
            ctx->sstatus.spp ? "smode" : "umode");
    }
    bool from_umode = !ctx->sstatus.spp;
    if (task::TaskManager::initialized()) {
        task::TaskManager::inst().reap_recycled();
    }
    if (scause.interrupt) {
        interrupt::dispatch(scause, sepc, stval, ctx);
    } else {
        // 异常
        exception::dispatch(scause, sepc, stval, ctx);
    }

    Interrupt::sti();
    schd::Scheduler::inst().schedule();

    Interrupt::cli();
    if (from_umode) {
        auto *tcb = schd::Scheduler::inst().current_tcb();
        if (tcb != nullptr) {
            auto new_sscratch =
                reinterpret_cast<csr_sscratch_t>(tcb->kstack_bottom);
            csr_set_sscratch(new_sscratch);
        }
    }
}

extern "C" void isr_entry(void);

void Interrupt::init(void) {
    // 重置 sscratch 寄存器
    csr_set_sscratch(0);

    auto isr_addr = (umb_t)isr_entry;
    if (isr_addr & 0x3) {
        loggers::INTERRUPT::ERROR("错误: isr_entry地址未对齐!");
        return;
    }

    csr_stvec_t stvec = {};
    stvec.ivt_addr    = isr_addr;
    // 采用direct模式
    stvec.mode        = 0b00;
    if (stvec.value & 0x3) {
        loggers::INTERRUPT::ERROR("错误: stvec地址未对齐!");
        return;
    }
    loggers::INTERRUPT::DEBUG("isr_entry 地址: 0x%016lx", isr_addr);

    // 写入stvec寄存器
    csr_set_stvec(stvec);
}

void Interrupt::sti() {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sie           = 1;
    csr_set_sstatus(sstatus);
}

void Interrupt::cli() {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sie           = 0;
    csr_set_sstatus(sstatus);
}
