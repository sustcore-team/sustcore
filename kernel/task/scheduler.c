/**
 * @file scheduler.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 调度器
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <sus/list_helper.h>
#include <sus/paging.h>
#include <mem/kmem.h>
#include <task/scheduler.h>

#ifdef DLOG_TASK
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

/**
 * @brief 获得下一个就绪进程, 同时将其弹出就绪队列
 *
 * @return PCB* 下一个就绪进程的PCB指针, 如果是nullptr, 则表示发生错误;
 * 如果是cur_proc, 则表示继续运行当前进程
 */
static PCB *fetch_ready_process(void) {
    if (cur_proc == nullptr) {
        // 这是不可能发生的情况, 报错
        log_error("fetch_ready_process: 当前没有运行的进程");
        return nullptr;
    }

    // 如果当前进程已是rp0, 且仍然处于RUNNING状态, 则继续运行该进程
    if (cur_proc->rp_level == 0 && cur_proc->state == PS_RUNNING) {
        return cur_proc;
    }

    // 否则当前进程要么不是rp0, 要么不是RUNNING状态
    // 那么我们就可以从rp0开始寻找下一个就绪进程
    PCB *next;
    list_pop_front(next, RP_LIST(0));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp1
    // 如果当前进程已是rp1, 且仍然处于RUNNING状态, 且时间片未用尽,
    // 则继续运行该进程
    if (cur_proc->rp_level == 1 && cur_proc->state == PS_RUNNING &&
        cur_proc->rp1_count > 0)
    {
        return cur_proc;
    }

    // 否则从rp1队列中弹出下一个就绪进程
    list_pop_front(next, RP_LIST(1));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp2
    // 如果当前进程已是rp2, 且仍然处于RUNNING状态, 且时间片未用尽,
    // 则继续运行该进程
    if (cur_proc->rp_level == 2 && cur_proc->state == PS_RUNNING &&
        cur_proc->rp2_count > 0)
    {
        return cur_proc;
    }
    list_pop_front(next, RP_LIST(2));
    if (next != nullptr) {
        return next;
    }

    // 寻找rp3
    // 如果当前进程已是rp3, 且仍然处于RUNNING状态, 则继续运行该进程
    if (cur_proc->rp_level == 3 && cur_proc->state == PS_RUNNING) {
        return cur_proc;
    }
    // 取出有序链表中run_time最小的进程(位于头部)
    ordered_list_pop_front(next, RP3_LIST);
    if (next != nullptr) {
        return next;
    }

    // 均不符合要求
    return nullptr;
}

/**
 * @brief 调度器 - 选择下一个就绪进程进行切换
 *
 */
void schedule(RegCtx **ctx, int time_gap) {
    // cur_proc 为空, 还未到进程调度阶段
    if (cur_proc == nullptr) {
        log_error("schedule: 当前没有运行的进程");
        return;
    }

    log_debug("schedule: 当前进程 pid=%d, state=%d",
              cur_proc ? cur_proc->pid : -1, cur_proc ? cur_proc->state : -1);

    // 更新当前进程的上下文
    cur_proc->ctx = *ctx;
    // 如果是rp1/rp2进程, 则更新其时间片计数器
    if (time_gap > 0) {
        if (cur_proc->rp_level == 1) {
            cur_proc->rp1_count--;
        } else if (cur_proc->rp_level == 2) {
            cur_proc->rp2_count--;
        }
    }
    // 如果是rp3进程, 则更新其运行时间统计
    else if (cur_proc->rp_level == 3)
    {
        cur_proc->run_time += time_gap;  // TODO: 根据已经过去的周期数进行更新
    }

    // 获取下一个就绪进程
    PCB *next = fetch_ready_process();

    // 继续运行当前进程
    if (next == cur_proc) {
        log_debug("继续运行当前进程 (pid=%d), rp_level = %d", cur_proc->pid,
                  cur_proc->rp_level);
        if (cur_proc->rp_level == 1) {
            log_debug("RP1: 剩余时间片为%d", cur_proc->rp1_count);
        } else if (cur_proc->rp_level == 2) {
            log_debug("RP2: 剩余时间片为%d", cur_proc->rp2_count);
        } else if (cur_proc->rp_level == 2) {
            log_debug("RP3: 已运行%d ms", cur_proc->run_time);
        }
        return;
    }

    // 没找到任何可运行的进程
    if (next == nullptr) {
        // 如果当前进程仍然是RUNNING状态, 则继续运行当前进程
        if (cur_proc->state == PS_RUNNING) {
            next = cur_proc;
        } else {
            log_debug("所有进程均运行");
            // 目前没有任何进程可以运行.
            // 此时应当调度一个备用的空进程
            // 但是我们还没做XD
            log_error("schedule: 没有可运行的进程, 系统停滞");
            while (true);
        }
    }

    // 更新进程状态
    // 如果当前进程仍然是RUNNING状态, 或者是YIELD状态, 则将其设为READY
    int prev_pid = cur_proc->pid;
    if (cur_proc->state == PS_RUNNING || cur_proc->state == PS_YIELD) {
        cur_proc->state = PS_READY;
    }
    // 加入到相应的就绪队列
    if (cur_proc->state == PS_READY) {
        if (cur_proc->rp_level == 3) {
            // rp3为有序链表
            ordered_list_insert(cur_proc, RP3_LIST);
        } else {
            // 其它rp队列为普通链表, 加到尾部(先到先得)
            list_push_back(cur_proc, RP_LIST(cur_proc->rp_level));
        }
    }

    // 特殊判断: 如果进程已经处于DEAD状态, 则不将其放回就绪队列,
    // 并进行相关资源的清理
    if (cur_proc->state == PS_ZOMBIE) {
        log_info("进程 (pid=%d) 进入僵尸状态, 进行资源清理", cur_proc->pid);
        // 释放相关资源
        terminate_pcb(cur_proc);
        cur_proc = nullptr;
    }

    // 将当前进程更新到下一个进程
    cur_proc        = next;
    cur_proc->state = PS_RUNNING;

    // 如果是rp1/rp2进程, 则重置其时间片计数器
    if (cur_proc->rp_level == 1) {
        cur_proc->rp1_count = 5;  // TODO: 时间片长度待定
    } else if (cur_proc->rp_level == 2) {
        cur_proc->rp2_count = 3;  // TODO: 时间片长度待定
    }

    log_debug("调度: 从进程 (pid=%d) 切换到进程 (pid=%d)", prev_pid,
              cur_proc->pid);
    // 根据rp_level打印调度信息
    if (cur_proc->rp_level == 0) {
        log_debug("调度到 rp0 实时进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 1) {
        log_debug("调度到 rp1 服务进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 2) {
        log_debug("调度到 rp2 普通用户进程 (pid=%d)", cur_proc->pid);
    } else if (cur_proc->rp_level == 3) {
        log_debug("调度到 rp3 Daemon进程 (pid=%d)", cur_proc->pid);
    }

    // 切换到下一个进程
    *ctx = cur_proc->ctx;

    // 更新页表
    log_info("切换页表到进程 (pid=%d) 的页表 %p", cur_proc->pid,
             cur_proc->tm->pgd);
    mem_switch_root(KPA2PA(cur_proc->tm->pgd));

    // TODO: 更新页表与其它控制寄存器
}

void after_interrupt(RegCtx **ctx) {
    if (cur_proc == nullptr) {
        log_error("after_interrupt: 当前没有运行的进程");
        return;
    }
    // 继续运行当前进程
    if (cur_proc->state == PS_RUNNING) {
        // 判断一下高级别是否有就绪进程需要抢占
        bool yield = false;
        for (int i = cur_proc->rp_level - 1; i >= 0; i--) {
            // 高级别有就绪进程
            if (rp_list_heads[i] != nullptr) {
                yield = true;
                break;
            }
        }
        // 无需抢占
        if (!yield) {
            return;
        }
    }
    // 当前进程已阻塞或终止, 抑或是有高级别就绪进程, 需要调度下一个进程
    schedule(ctx, 0);
}
