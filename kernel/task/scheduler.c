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

#include <mem/kmem.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
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
    if (cur_proc->rp_level == 0 && cur_proc->state == TS_RUNNING) {
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
    if (cur_proc->rp_level == 1 && cur_proc->state == TS_RUNNING &&
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
    if (cur_proc->rp_level == 2 && cur_proc->state == TS_RUNNING &&
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
    if (cur_proc->rp_level == 3 && cur_proc->state == TS_RUNNING) {
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
 * @brief 获得进程中的下一个就绪线程, 同时将其弹出就绪队列
 *
 * @param p 进程PCB指针
 * @return TCB* 下一个就绪线程的TCB指针, 如果是nullptr, 则表示没有就绪线程
 */
static TCB *fetch_ready_thread(PCB *p) {
    if (p == nullptr) {
        log_error("fetch_ready_thread: 传入的PCB指针为空");
        return nullptr;
    }

    TCB *t;
    // 获得优先级最高的就绪线程
    ordered_list_front(t, READY_THREAD_LIST(p));

    bool p_ready = (p->current_thread != nullptr &&
                    p->current_thread->state == TS_READY);

    // 与当前线程比较
    if (p_ready && (t == nullptr || t->priority >= p->current_thread->priority))
    {
        // 继续运行当前线程
        return p->current_thread;
    }
    // 否则弹出就绪线程
    ordered_list_pop_front(t, READY_THREAD_LIST(p));
    return t;
}

// TODO: 将调度器更改为彻底按照线程为单位进行调度
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

    // 更新当前线程的上下文
    if (cur_proc->current_thread != nullptr) {
        cur_proc->current_thread->ctx = *ctx;
    }
    // 如果是rp3进程, 更新其运行时间统计
    if (cur_proc->rp_level == 3) {
        cur_proc->run_time += time_gap;  // TODO: 根据已经过去的周期数进行更新
    }

    // 获取下一个就绪进程
    PCB *next_proc;
    TCB *next_thread;
    do {
        next_proc = fetch_ready_process();

        // 没找到任何可运行的进程
        if (next_proc == nullptr) {
            // 如果当前进程仍然是RUNNING状态, 则继续运行当前进程
            if (cur_proc->state == TS_RUNNING) {
                next_proc = cur_proc;
                // 重置时间片计数器
                if (cur_proc->rp_level == 1) {
                    cur_proc->rp1_count = 4;  // TODO: 时间片长度待定
                } else if (cur_proc->rp_level == 2) {
                    cur_proc->rp2_count = 2;  // TODO: 时间片长度待定
                }
            } else {
                // 目前没有任何进程可以运行.
                // 此时应当调度一个备用的空进程
                // 但是我们还没做XD
                log_error("schedule: 没有可运行的进程, 系统停滞");
                while (true);
            }
        }
        // 获得下一个线程
        next_thread = fetch_ready_thread(next_proc);
        if (next_thread == nullptr) {
            log_debug("进程 (pid=%d) 没有就绪线程, 继续寻找下一个就绪进程",
                      next_proc->pid);
            // 将当前进程设为SUSPENDED状态
            next_proc->state = TS_SUSPENDED;
        }
    }
    // 如果没有就绪线程, 则继续寻找下一个就绪进程
    while (next_thread == nullptr);

    bool switch_proc = (next_proc != cur_proc);
    bool switch_thread = (next_thread != next_proc->current_thread);

    // 继续运行当前进程
    if (!switch_proc) {
        log_debug("继续运行当前进程 (pid=%d), rp_level = %d", cur_proc->pid,
                cur_proc->rp_level);
        if (cur_proc->rp_level == 1) {
            log_debug("RP1: 剩余时间片为%d", cur_proc->rp1_count);
        } else if (cur_proc->rp_level == 2) {
            log_debug("RP2: 剩余时间片为%d", cur_proc->rp2_count);
        } else if (cur_proc->rp_level == 2) {
            log_debug("RP3: 已运行%d ms", cur_proc->run_time);
        }
    }


    // 如果没发生进程切换, 更新时间片计数器
    if (!switch_proc) {
        // 如果是rp1/rp2进程, 则更新其时间片计数器
        if (time_gap > 0) {
            if (cur_proc->rp_level == 1) {
                cur_proc->rp1_count--;
            } else if (cur_proc->rp_level == 2) {
                cur_proc->rp2_count--;
            }
        }
    }

    // 如果不需要切换进程和线程, 则直接返回
    if (!switch_proc && !switch_thread) {
        log_debug("继续运行当前线程 (tid=%d)",
                  cur_proc->current_thread->tid);
        cur_proc->current_thread->state = TS_RUNNING;
        return;
    }

    // 如果需要切换进程
    if (switch_proc) {
        log_debug("调度: 选择下一个进程 (pid=%d)", next_proc->pid);
        // 更新切换前进程状态
        // 如果当前进程仍然是RUNNING状态, 或者是YIELD状态, 则将其设为READY
        if (cur_proc->state == TS_RUNNING || cur_proc->state == TS_YIELD) {
            cur_proc->state = TS_READY;
            if (cur_proc->rp_level == 3) {
                // rp3为有序链表
                ordered_list_insert(cur_proc, RP3_LIST);
            } else {
                // 其它rp队列为普通链表, 加到尾部(先到先得)
                list_push_back(cur_proc, RP_LIST(cur_proc->rp_level));
            }
        }
        // 如果进程状态为ZOMBIE, 则进行资源清理
        if (cur_proc->state == TS_ZOMBIE) {
            log_info("进程 (pid=%d) 进入僵尸状态, 进行资源清理", cur_proc->pid);
            // 释放相关资源
            terminate_pcb(cur_proc);
            cur_proc = nullptr;
        }
        cur_proc        = next_proc;
        cur_proc->state = TS_RUNNING;

        // 重置时间片计数器
        if (cur_proc->rp_level == 1) {
            cur_proc->rp1_count = 4;  // TODO: 时间片长度待定
        } else if (cur_proc->rp_level == 2) {
            cur_proc->rp2_count = 2;  // TODO: 时间片长度待定
        }

        // 更新页表
        log_info("切换页表到进程 (pid=%d) 的页表 %p", cur_proc->pid,
                 cur_proc->tm->pgd);
        mem_switch_root(KPA2PA(cur_proc->tm->pgd));
    }

    // 如果需要切换线程
    if (switch_thread) {
        log_debug("调度: 进程 (pid=%d) 切换线程 (tid=%d -> tid=%d)",
                  cur_proc->pid,
                  cur_proc->current_thread ? cur_proc->current_thread->tid : -1,
                  next_thread->tid);
        // 更新切换前线程状态
        if (cur_proc->current_thread != nullptr) {
            // 如果当前线程仍然是RUNNING或YIELD状态, 则将其设为READY
            if (cur_proc->current_thread->state == TS_RUNNING ||
                cur_proc->current_thread->state == TS_YIELD)
            {
                cur_proc->current_thread->state = TS_READY;
                // 加入到就绪队列
                ordered_list_insert(cur_proc->current_thread,
                                    READY_THREAD_LIST(cur_proc));
            }
            // 如果线程状态为ZOMBIE, 则进行资源清理
            if (cur_proc->current_thread->state == TS_ZOMBIE) {
                log_info("线程 (tid=%d) 进入僵尸状态, 进行资源清理",
                         cur_proc->current_thread->tid);
                // 释放相关资源
                terminate_tcb(cur_proc->current_thread);
            }
        }
        // 切换到下一个线程
        cur_proc->current_thread        = next_thread;
    }

    // 设置为RUNNING状态
    cur_proc->current_thread->state = TS_RUNNING;
    // 更新上下文指针
    *ctx = cur_proc->current_thread->ctx;
}

void after_interrupt(RegCtx **ctx) {
    if (cur_proc == nullptr) {
        log_error("after_interrupt: 当前没有运行的进程");
        return;
    }
    // 继续运行当前进程/线程
    if (cur_proc->state == TS_RUNNING && cur_proc->current_thread != nullptr &&
        cur_proc->current_thread->state == TS_RUNNING) {
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

/**
 * @brief 将进程加入到就绪队列
 * 
 * @param p 进程PCB指针
 */
void insert_ready_process(PCB *p)
{
    // 判断 p 的加入是否合理
    if (p == nullptr) {
        log_error("insert_ready_process: 传入的PCB指针为空");
        return;
    }

    // 不为 READY 或 RUNNING
    if (p->state != TS_READY && p->state != TS_RUNNING) {
        log_error("insert_ready_process: 只能加入READY或RUNNING状态的进程 (pid=%d, state=%d)",
                  p->pid, p->state);
        return;
    }

    // 没有主线程
    if (p->main_thread == nullptr) {
        log_error("insert_ready_process: 进程没有主线程 (pid=%d)", p->pid);
        return;
    }

    // 如果 p 没有当前在运行的线程
    if (p->current_thread == nullptr) {
        // 以主线程作为当前运行线程
        p->current_thread = p->main_thread;
        // 设置为 RUNNING 状态
        p->current_thread->state = TS_RUNNING;
    }

    if (p->ready_threads_head == nullptr) {
        log_error("insert_ready_process: 进程没有就绪线程 (pid=%d)", p->pid);
        return;
    }

    // 统一改为 READY 状态
    p->state = TS_READY;

    if (p->rp_level == 3) {
        // rp3为有序链表
        ordered_list_insert(p, RP3_LIST);
    } else {
        // 其它rp队列为普通链表, 加到尾部(先到先得)
        list_push_back(p, RP_LIST(p->rp_level));
    }
}

void wakeup_process(PCB *p)
{
    // 判断 p 的唤醒是否合理
    if (p == nullptr) {
        log_error("wakeup_process: 传入的PCB指针为空");
        return;
    }

    if (p->state != TS_WAITING) {
        log_error("wakeup_process: 只能唤醒WAITING状态的进程 (pid=%d, state=%d)",
                  p->pid, p->state);
        return;
    }

    // 唤醒进程
    insert_ready_process(p);
}

/**
 * @brief 唤醒线程
 * 
 * @param t 被唤醒的线程TCB指针
 */
void wakeup_thread(TCB *t)
{
    // 判断 t 的唤醒是否合理
    if (t == nullptr) {
        log_error("wakeup_thread: 传入的TCB指针为空");
        return;
    }

    if (t->state != TS_WAITING) {
        log_error("wakeup_thread: 只能唤醒WAITING状态的线程 (tid=%d, state=%d)",
                  t->tid, t->state);
        return;
    }

    // 进入 READY 状态
    t->state = TS_READY;

    // 加入就绪队列
    ordered_list_insert(t, READY_THREAD_LIST(t->pcb));
}