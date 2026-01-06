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

#include <mem/alloc.h>
#include <mem/kmem.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
#include <task/scheduler.h>

#ifdef DLOG_TASK
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

/**
 * @brief 获得下一个就绪线程
 *
 * @return TCB* 下一个就绪进程的TCB指针, 如果是nullptr, 则表示没有就绪进程
 */
static TCB *lookup_ready_thread(void) {
    TCB *t = nullptr;

    // 从RP0开始逐步查找
    list_front(t, RP_LIST(0));
    if (t != nullptr) {
        return t;
    }

    list_front(t, RP_LIST(1));
    if (t != nullptr) {
        return t;
    }

    list_front(t, RP_LIST(2));
    if (t != nullptr) {
        return t;
    }

    ordered_list_front(t, RP3_LIST);
    return t;
}

static void pop_ready_thread(TCB *t) {
    if (t == nullptr || t->pcb == nullptr) {
        return;
    }

    // 从就绪队列中移除
    if (t->pcb->rp_level == 3) {
        ordered_list_remove(t, RP3_LIST);
    } else {
        list_remove(t, RP_LIST(t->pcb->rp_level));
    }
}

/**
 * @brief 判断线程是否可用
 *
 * @param t 线程TCB指针
 * @return true 线程可被调度
 * @return false 线程不可被调度
 */
static bool thread_available(TCB *t) {
    if (t == nullptr || t->pcb == nullptr) {
        return false;
    }
    if (t->state != TS_READY && t->state != TS_RUNNING) {
        return false;
    }
    if (t->pcb->rp_level == 1 && t->rp1_count <= 0) {
        return false;
    }
    if (t->pcb->rp_level == 2 && t->rp2_count <= 0) {
        return false;
    }
    return true;
}

/**
 * @brief 比较线程优先程度
 *
 * 实际上, 我们比较的是其进程的优先程度XD
 * 其中, 我们总是假定线程B是可调度的
 *
 * @param a 线程A(通常是当前线程)
 * @param b 线程B(通常是就绪线程)
 * @return true A比B更加优先
 * @return false B比A更加优先
 */
static bool compare_priority(TCB *a, TCB *b) {
    return thread_available(a) && (a->pcb->rp_level <= b->pcb->rp_level);
}

// 未启用调度器
bool sheduling_enabled = false;

const char *thread_info_to_string(TCB *t) {
    if (t == nullptr || t->pcb == nullptr) {
        return "NULL";
    }
    static char buffer[128];
    if (t->pcb->rp_level == 1 || t->pcb->rp_level == 2) {
        sprintf(
            buffer,
            "TCB(tid=%d, state=%s, priority=%d, pcb=PCB(pid=%d, proc_rp_level=%d), rp1_count=%d, rp2_count=%d)",
            t->tid, thread_state_to_string(t->state), t->priority,
            t->pcb ? t->pcb->pid : -1, t->pcb ? t->pcb->rp_level : -1);
        return buffer;
    }
    sprintf(
        buffer,
        "TCB(tid=%d, state=%s, priority=%d, pcb=PCB(pid=%d, proc_rp_level=%d))",
        t->tid, thread_state_to_string(t->state), t->priority,
        t->pcb ? t->pcb->pid : -1, t->pcb ? t->pcb->rp_level : -1);
    return buffer;
}

#define RP1_TIME_SLICE 5
#define RP2_TIME_SLICE 3

// 在调度前的预处理
static void pre_schedule(RegCtx **ctx, int time_gap) {
    // 更新当前线程的上下文
    if (cur_thread != nullptr) {
        cur_thread->ctx = *ctx;

        // 更新时间片计数器
        if (time_gap > 0) {
            if (cur_thread->pcb->rp_level == 1) {
                cur_thread->rp1_count--;
            } else if (cur_thread->pcb->rp_level == 2) {
                cur_thread->rp2_count--;
            }
        }

        // 如果是rp3进程, 更新其运行时间统计
        if (cur_thread->pcb->rp_level == 3) {
            cur_thread->run_time += time_gap;
        }
    }
}

// 在调度后对当前线程的处理
static void post_schedule(RegCtx **ctx) {
    if (cur_thread == nullptr || cur_thread->pcb == nullptr) {
        return;
    }

    cur_thread->state = TS_RUNNING;

    // 在时间片耗尽时, 重置时间片计数器
    if (cur_thread->pcb->rp_level == 1 && cur_thread->rp1_count <= 0) {
        cur_thread->rp1_count = RP1_TIME_SLICE;
    } else if (cur_thread->pcb->rp_level == 2 && cur_thread->rp2_count <= 0) {
        cur_thread->rp2_count = RP2_TIME_SLICE;
    }
}

// TODO: 将调度器更改为彻底按照线程为单位进行调度
/**
 * @brief 调度器 - 选择下一个就绪进程进行切换
 *
 */
void schedule(RegCtx **ctx, int time_gap) {
    // ========= PART I: 更新当前线程状态 =========

    // 未启用调度器
    if (!sheduling_enabled) {
        return;
    }

    if (cur_thread != nullptr) {
        log_debug("schedule(): 当前线程=%s", thread_info_to_string(cur_thread));
    }

    pre_schedule(ctx, time_gap);

    // ========= PART II: 进行调度判定 =========

    // 获取下一个就绪线程
    TCB *next_thread = lookup_ready_thread();

    // 没有就绪线程
    if (next_thread == nullptr) {
        if (cur_thread == nullptr || cur_thread->pcb == nullptr ||
            (cur_thread->state != TS_RUNNING && cur_thread->state != TS_READY))
        {
            log_error("没有就绪线程, 且当前线程无效, 系统无可运行线程, 死锁!");
            while (true);
        }

        post_schedule(ctx);

        log_debug("没有就绪线程, 继续运行当前线程 (pid=%d, tid=%d)",
                  cur_thread->pcb->pid, cur_thread->tid);

        return;
    }

    if (compare_priority(cur_thread, next_thread)) {
        // 继续运行当前线程, 直接返回
        log_debug("继续运行当前线程 (pid=%d, tid=%d)", cur_thread->pcb->pid,
                    cur_thread->tid);
        return;
    }

    // 弹出并切换至下一个就绪线程
    pop_ready_thread(next_thread);


    // ========= PART III: 进行线程调度 =========

    // 现在我们判断是否切换了进程
    bool switch_proc =
        cur_thread == nullptr || next_thread->pcb != cur_thread->pcb;

    // 切换到下一个就绪线程
    log_debug("调度: 选择下一个线程 (tid=%d)", next_thread->tid);

    // 更新切换前线程状态
    if (cur_thread != nullptr &&
        (cur_thread->state == TS_RUNNING || cur_thread->state == TS_YIELD || cur_thread->state == TS_READY))
    {
        cur_thread->state = TS_READY;
        insert_ready_thread(cur_thread);
    }

    // 更新当前线程指针
    cur_thread        = next_thread;
    cur_thread->state = TS_RUNNING;

    // 如果需要切换进程
    if (switch_proc) {
        // 更新页表
        log_info("切换页表到进程 (pid=%d) 的页表 %p", cur_thread->pcb->pid,
                 cur_thread->pcb->tm->pgd);
        mem_switch_root(KPA2PA(cur_thread->pcb->tm->pgd));
    }

    post_schedule(ctx);
    // 更新上下文指针
    *ctx = cur_thread->ctx;
}

void after_interrupt(RegCtx **ctx) {
    // 未启用调度器
    if (!sheduling_enabled) {
        return;
    }

    // 模拟调度器, 判断是否需要进行调度
    // 获取下一个就绪线程
    TCB *next_thread = lookup_ready_thread();
    if ((next_thread == nullptr || compare_priority(cur_thread, next_thread)) && thread_available(cur_thread)) {
        return;
    }

    // 需要调度, 则调用调度器
    schedule(ctx, 0);
}

/**
 * @brief 将线程加入到就绪队列
 *
 * @param t 线程TCB指针
 */
void insert_ready_thread(TCB *t) {
    // 判断 t 的加入是否合理
    if (t == nullptr) {
        log_error("insert_ready_thread: 传入的TCB指针为空");
        return;
    }

    // 不为 READY 或 RUNNING
    if (t->state != TS_READY && t->state != TS_RUNNING) {
        log_error(
            "insert_ready_thread: 只能加入READY或RUNNING状态的线程 (tid=%d, "
            "state=%d)",
            t->tid, t->state);
        return;
    }

    // 加入就绪队列
    if (t->pcb->rp_level == 3) {
        // rp3为有序链表
        ordered_list_insert(t, RP3_LIST);
    } else {
        // 其它rp队列为普通链表, 加到尾部(先到先得)
        list_push_back(t, RP_LIST(t->pcb->rp_level));
    }

    log_debug("insert_ready_thread: 线程 (tid=%d) 加入就绪队列", t->tid);
}

/**
 * @brief 唤醒线程
 *
 * @param t 被唤醒的线程WaitingTCB指针
 */
void wakeup_thread(WaitingTCB *t) {
    // 判断 t 的唤醒是否合理
    if (t == nullptr || t->tcb == nullptr) {
        log_error("wakeup_thread: 传入的TCB指针为空");
        return;
    }

    if (t->tcb->state != TS_WAITING) {
        log_error("wakeup_thread: 只能唤醒WAITING状态的线程 (tid=%d, state=%d)",
                  t->tcb->tid, t->tcb->state);
        return;
    }

    // 进入 READY 状态
    t->tcb->state = TS_READY;

    // 加入就绪队列
    insert_ready_thread(t->tcb);

    // 从等待队列中移除
    list_remove(t, WAITING_LIST);
    kfree(t);
}