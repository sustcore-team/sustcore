/**
 * @file isr.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断处理例程
 * @version alpha-1.0.0
 * @date 2025-11-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/description.h>
#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/int/isr.h>
#include <env.h>
#include <kio.h>
#include <mem/kaddr.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sustcore/addr.h>
#include <sustcore/epacks.h>
#include <task/scheduler.h>
#include <task/task_struct.h>

namespace Exceptions {
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
};  // namespace Exceptions

namespace handlers::paging {
    enum class FaultCause {
        NO_PRESENT,       // No present page
        SAU_NO_SUM,       // S-mode Access User page without SUM
        SEU,              // S-mode Execute User page
        UAS,              // User Access Supervisor page
        INVALID_AD,       // Invalid A/D bit
        WRITE_PROTECT,    // Write protection
        EXECUTE_PROTECT,  // Execute protection
        UNKNOWN           // Unknown cause
    };

    static FaultCause confirm_fault_cause(const csr_scause_t &scause,
                                          const VirAddr &fault_addr,
                                          const Riscv64Context *ctx,
                                          PageMan &pman) {
        if (scause.cause != Exceptions::INST_PAGE_FAULT &&
            scause.cause != Exceptions::LOAD_PAGE_FAULT &&
            scause.cause != Exceptions::STORE_PAGE_FAULT)
        {
            return FaultCause::UNKNOWN;
        }

        auto query_res = pman.query_page(fault_addr);
        if (!query_res.has_value()) {
            // 页面不存在
            if (query_res.error() == ErrCode::PAGE_NOT_PRESENT) {
                return FaultCause::NO_PRESENT;
            }
            loggers::INTERRUPT::ERROR("查询页表时发生错误: addr=%p, err=%d",
                                      fault_addr.addr(), query_res.error());
            return FaultCause::UNKNOWN;
        }

        auto qres                  = query_res.value();
        typename PageMan::PTE *pte = qres.pte;

        // 页面不存在
        if (!PageMan::is_present(*pte)) {
            return FaultCause::NO_PRESENT;
        }

        int acc_priv =
            ctx->sstatus.spp ? 1 : 0;  // 访问发生时的特权级, 0=用户态, 1=内核态
        int pte_priv = pte->u ? 0 : 1;  // 页表项的特权级, 0=用户页, 1=内核页
        int priv_gap =
            acc_priv - pte_priv;  // 访问特权级与页表项特权级的差值
                                  // = 0 说明访问特权级与页表项特权级相同
                                  // > 0 说明访问特权级比页表项特权级高
                                  // < 0 说明访问特权级比页表项特权级低

        // 访问特权级比页表项特权级高, 说明是内核态访问用户页
        if (priv_gap > 0) {
            if (scause.cause == Exceptions::INST_PAGE_FAULT) {
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
        if ((scause.cause == Exceptions::STORE_PAGE_FAULT) &&
            !PageMan::is_writable(rwx))
        {
            return FaultCause::WRITE_PROTECT;
        }
        // 取指页异常且页面不可执行
        if ((scause.cause == Exceptions::INST_PAGE_FAULT) &&
            !PageMan::is_executable(rwx))
        {
            return FaultCause::EXECUTE_PROTECT;
        }
        // 取指页异常且是高特权级访问用户页
        if ((scause.cause == Exceptions::INST_PAGE_FAULT) &&
            !PageMan::is_executable(rwx))
        {
            return FaultCause::EXECUTE_PROTECT;
        }

        // AD位错误
        if (!pte->a) {
            return FaultCause::INVALID_AD;
        }
        // 存储页异常且D位未设置但页面可写
        if ((scause.cause == Exceptions::STORE_PAGE_FAULT) && !pte->d &&
            PageMan::is_writable(rwx))
        {
            return FaultCause::INVALID_AD;
        }

        return FaultCause::UNKNOWN;
    }

    bool paging_fault(csr_scause_t scause, umb_t sepc, umb_t stval,
                      Riscv64Context *ctx) {
        loggers::INTERRUPT::DEBUG("进入页异常处理程序");
        loggers::INTERRUPT::INFO("异常页地址: 0x%016lx", stval);

        const VirAddr fault_addr = VirAddr(stval);
        auto &e                  = env::inst();
        loggers::INTERRUPT::DEBUG("paging_fault: env.pgd=%p, tm=%p",
                                  e.pgd().addr(), e.tm());
        PageMan pman(e.pgd());

        if (!e.pgd().nonnull()) {
            return false;
        }

        FaultCause cause = confirm_fault_cause(scause, fault_addr, ctx, pman);

        bool processsed = false;

        switch (cause) {
            case FaultCause::NO_PRESENT: {
                loggers::INTERRUPT::INFO("缺页异常: 0x%016lx", stval);
                // 使用缺页异常处理程序处理缺页异常
                auto *tm = e.tm();
                if (tm != nullptr) {
                    loggers::INTERRUPT::DEBUG(
                        "调用 TM::on_np 处理缺页: addr=%p, tm_pgd=%p",
                        fault_addr.addr(), tm->pgd().addr());
                    processsed |= tm->on_np({fault_addr});

                    // for debug:
                    if (processsed) {
                        // 再次查询，验证该页已被映射
                        PhyAddr hw_root_after = PageMan::read_root();
                        PageMan verify_pman(hw_root_after);
                        auto verify_res = verify_pman.query_page(fault_addr);
                        if (!verify_res.has_value()) {
                            loggers::INTERRUPT::ERROR(
                                "TM::on_np 返回成功但页面仍不存在: addr=%p, "
                                "err=%d, hw_root_after=%p",
                                fault_addr.addr(), verify_res.error(),
                                hw_root_after.addr());
                        } else {
                            loggers::INTERRUPT::DEBUG(
                                "TM::on_np 映射完成: addr=%p, hw_root_after=%p",
                                fault_addr.addr(), hw_root_after.addr());
                        }
                    }
                }
                break;
            }
            case FaultCause::SAU_NO_SUM: {
                loggers::INTERRUPT::ERROR(
                    "内核态访问用户页但未设置 SUM 位! addr=%p",
                    fault_addr.addr());
                break;
            }
            case FaultCause::UAS: {
                loggers::INTERRUPT::ERROR("用户态访问用户页但权限不足! addr=%p",
                                          fault_addr.addr());
                break;
            }
            case FaultCause::SEU: {
                loggers::INTERRUPT::ERROR("内核态执行用户页! addr=%p",
                                          fault_addr.addr());
                break;
            }
            case FaultCause::INVALID_AD: {
                auto query_res = pman.query_page(fault_addr);
                if (!query_res.has_value()) {
                    loggers::INTERRUPT::ERROR(
                        "处理 A/D 位错误时查询页表失败: addr=%p, err=%d",
                        fault_addr.addr(), query_res.error());
                    break;
                }
                PageMan::PTE *pte = query_res.value().pte;
                bool present      = PageMan::is_present(*pte);
                if (!present) {
                    loggers::INTERRUPT::ERROR(
                        "处理 A/D 位错误时页面不存在: addr=%p, pte=%p",
                        fault_addr.addr(), pte);
                    break;
                }

                bool updated = false;

                if (!pte->a) {
                    cause = FaultCause::INVALID_AD;
                }

                PageMan::RWX rwx = PageMan::rwx(*pte);
                if ((scause.cause == Exceptions::STORE_PAGE_FAULT) && !pte->d &&
                    PageMan::is_writable(rwx))
                {
                    cause = FaultCause::INVALID_AD;
                }

                processsed |= updated;
                if (updated) {
                    PageMan::flush_tlb();
                    loggers::INTERRUPT::DEBUG(
                        "修复 A/D 位后重试: addr=%p, A=%d, D=%d",
                        fault_addr.addr(), pte->a, pte->d);
                }
                break;
            }
            case FaultCause::WRITE_PROTECT: {
                loggers::INTERRUPT::ERROR("写保护异常: addr=%p",
                                          fault_addr.addr());
                break;
            }
            case FaultCause::EXECUTE_PROTECT: {
                loggers::INTERRUPT::ERROR("执行保护异常: addr=%p",
                                          fault_addr.addr());
                break;
            }
            default: {
                loggers::INTERRUPT::ERROR("未知页异常! addr=%p",
                                          fault_addr.addr());
                break;
            }
        }

        return processsed;

        // 接下来应该执行页异常相关处理
        // 1. 检查地址是否合法
        // 2. 如果是缺页, 则分配物理页并映射
        // 2.1 如果是缺页, 且该页属于虚拟内存, 则从磁盘中调入
        // 2.2 如果是写保护错误, 则检查是否为写时复制
        // 2.3 更新页表项和TLB
        // 3. 如果是权限错误, 则终止相关进程
    }
}  // namespace handlers::paging

namespace Handlers {

    bool illegal_instruction(csr_scause_t scause, umb_t sepc, umb_t stval,
                             Riscv64Context *ctx) {
        loggers::INTERRUPT::DEBUG("进入非法指令异常处理程序");
        // 我们可以通过该指令自定义kernel服务
        dword ins = *((dword *)sepc);
        loggers::INTERRUPT::INFO("指令内容: 0x%08x", ins);
        // 这是一个任意的非法指令
        // 被我们选中用于模拟真实指令
        if (ins == 0x000000FF) {
            loggers::INTERRUPT::INFO("自定义Kernel服务: Hello, World!");
        } else if (ins == 0x00FF00FF) {
            loggers::INTERRUPT::INFO(
                "自定义Kernel服务: 输出 t0寄存器指向的日志");

            ker_paddr::SumGuard guard;  // 确保可以访问用户空间地址
            char msg[64];
            // t0 = x5 = regs[5 - 1]
            memcpy(msg, (void *)ctx->regs[4], sizeof(msg) - 1);
            msg[sizeof(msg) - 1] = '\0';  // 确保字符串以 null 结尾
            loggers::INTERRUPT::INFO("用户程序传递的消息: %s", msg);
        } else {
            loggers::INTERRUPT::ERROR("非kernel自定义指令: 0x%08x", ins);
            return false;
        }

        ctx->sepc += 4;  // 跳过该指令
        return true;
    }

    void exception(csr_scause_t scause, umb_t sepc, umb_t stval,
                   Riscv64Context *ctx) {
        // 输出异常类型
        if (scause.cause < sizeof(Exceptions::MSG) / sizeof(Exceptions::MSG[0]))
        {
            loggers::INTERRUPT::INFO("发生异常! 类型: %s (%lu)",
                                     Exceptions::MSG[scause.cause],
                                     scause.cause);
        } else {
            loggers::INTERRUPT::INFO("发生异常! 类型: 未知 (%lu)",
                                     scause.cause);
        }
        if (ctx->sstatus.spp) {
            loggers::INTERRUPT::ERROR("异常发生在S-Mode");
        } else {
            loggers::INTERRUPT::ERROR("异常发生在U-Mode");
        }
        // 输出寄存器状态
        loggers::INTERRUPT::ERROR(
            "scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx, sstatus: 0x%lx",
            scause.value, sepc, stval, ctx->sstatus.value);
        loggers::INTERRUPT::ERROR("ctx: 0x%lx", ctx);

        bool processed = false;
        switch (scause.cause) {
            case Exceptions::ECALL_U: {
                break;
            }
            case Exceptions::ILLEGAL_INST:
                processed = illegal_instruction(scause, sepc, stval, ctx);
                break;
            case Exceptions::INST_PAGE_FAULT:
            case Exceptions::LOAD_PAGE_FAULT:
            case Exceptions::STORE_PAGE_FAULT:
                processed =
                    handlers::paging::paging_fault(scause, sepc, stval, ctx);
                break;
            default:
                loggers::INTERRUPT::INFO("无异常处理程序!");
                processed = false;
                break;
        }

        if (!processed) {
            loggers::INTERRUPT::ERROR("无法处理该异常, 终止相关进程");
            while (true);
        }
    }

    void timer(csr_scause_t scause, umb_t sepc, umb_t stval,
               Riscv64Context *ctx) {
        // 计算时间差
        units::tick current_ticks = units::tick::from_ticks(csr_get_time());
        units::tick gap_ticks     = current_ticks - timer_info.last_ticks;

        TimerTickEvent e = {.last_tick = timer_info.last_ticks,
                            .increment = timer_info.increment,
                            .gap_ticks = gap_ticks};

        timer_info.last_ticks = current_ticks;

        // 重新设置下一次时钟中断
        sbi_legacy_set_timer((current_ticks + timer_info.increment).to_ticks());
    }
}  // namespace Handlers
