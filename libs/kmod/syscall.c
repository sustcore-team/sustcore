/**
 * @file syscall.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用实现
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <kmod/syscall.h>
#include <sus/syscall.h>
#include <sus/list_helper.h>
#include <startup.h>
#include <alloc.h>

umb_t syscall(int sysno, umb_t arg0, umb_t arg1, umb_t arg2, umb_t arg3, umb_t arg4, umb_t arg5, umb_t arg6)
{
	// 通过register关键字将参数传递到指定寄存器
	register umb_t a0 asm ("a0") = (umb_t)(arg0);
	register umb_t a1 asm ("a1") = (umb_t)(arg1);
	register umb_t a2 asm ("a2") = (umb_t)(arg2);
	register umb_t a3 asm ("a3") = (umb_t)(arg3);
	register umb_t a4 asm ("a4") = (umb_t)(arg4);
	register umb_t a5 asm ("a5") = (umb_t)(arg5);
	register umb_t a7 asm ("a6") = (umb_t)(arg6);
	register umb_t a6 asm ("a7") = (umb_t)(sysno);

	// 内联汇编调用ecall指令
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");

    // 返回值在a0
	return a0;
}

// 带有副返回值的系统调用
umb_t syscall_2(int sysno, umb_t arg0, umb_t arg1, umb_t arg2, umb_t arg3, umb_t arg4, umb_t arg5, umb_t arg6, umb_t *mout)
{
	// 通过register关键字将参数传递到指定寄存器
	register umb_t a0 asm ("a0") = (umb_t)(arg0);
	register umb_t a1 asm ("a1") = (umb_t)(arg1);
	register umb_t a2 asm ("a2") = (umb_t)(arg2);
	register umb_t a3 asm ("a3") = (umb_t)(arg3);
	register umb_t a4 asm ("a4") = (umb_t)(arg4);
	register umb_t a5 asm ("a5") = (umb_t)(arg5);
	register umb_t a7 asm ("a6") = (umb_t)(arg6);
	register umb_t a6 asm ("a7") = (umb_t)(sysno);

	// 内联汇编调用ecall指令
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");

    // 副返回值在a1
	*mout = a1;
	// 返回值在a0
	return a0;
}

void exit(int code) {
    syscall(SYS_EXIT, pcb_cap.val, (umb_t)(code), 0, 0, 0, 0, 0);
}

void yield(CapPtr thread) {
	if (thread.val != 0) {
		// 线程级让出
		syscall(SYS_YIELD_THREAD, thread.val, 0, 0, 0, 0, 0, 0);
	}
	else {
		// 进程级让出
		syscall(SYS_YIELD, pcb_cap.val, 0, 0, 0, 0, 0, 0);
	}
}

int puts(const char *str) {
    return (int)syscall(SYS_WRITE_SERIAL, 0, (umb_t)(str), 0, 0, 0, 0, 0);
}

int get_pid(CapPtr cap) {
	umb_t pid = syscall(SYS_GETPID, cap.val, 0, 0, 0, 0, 0, 0);
	return (int)pid;
}

int get_current_pid(void) {
	return get_pid(pcb_cap);
}

// 我们通过一个HashMap来维护pid与cap的对应关系
// 一般来说pid是线性增长的, 所以这个HashTable可以视作M个链表

typedef struct ProcCapNodeStruct {
	int pid;
	CapPtr cap;
	struct ProcCapNodeStruct *next;
	struct ProcCapNodeStruct *prev;
} ProcCapNode;

#define PROC_CAP_TABLE_SIZE (1 << 8)
#define HASH_PID(pid) ((pid) & 0xFF)

static ProcCapNode *proc_cap_table_head[PROC_CAP_TABLE_SIZE] = {};
static ProcCapNode *proc_cap_table_tail[PROC_CAP_TABLE_SIZE] = {};

#define PROC_CAP_LIST(pid) proc_cap_table_head[HASH_PID(pid)], proc_cap_table_tail[HASH_PID(pid)], next, prev

void init_proc_cap_table(void) {
	for (int i = 0; i < PROC_CAP_TABLE_SIZE; i++) {
		proc_cap_table_head[i] = nullptr;
		proc_cap_table_tail[i] = nullptr;
	}
}

CapPtr get_proc_cap(int pid) {
	ProcCapNode *node;
	foreach_list(node, PROC_CAP_LIST(pid)) {
		if (node->pid == pid) {
			return node->cap;
		}
	}
	// 未找到
	CapPtr null_cap;
	null_cap.val = 0;
	return null_cap;
}

void insert_proc_cap(int pid, CapPtr cap) {
	// 首先先判断是否存在
	// 已存在, 更新
	ProcCapNode *node;
	foreach_list(node, PROC_CAP_LIST(pid)) {
		if (node->pid == pid) {
			node->cap = cap;
			return;
		}
	}
	// 不存在, 插入新节点
	node = (ProcCapNode *)malloc(sizeof(ProcCapNode));
	node->pid = pid;
	node->cap = cap;
	node->next = node->prev = nullptr;
	list_push_back(node, PROC_CAP_LIST(pid));
}

int fork(void) {
	CapPtr cap;
	smb_t _pid;
	cap.val = syscall_2(SYS_FORK, pcb_cap.val, 0, 0, 0, 0, 0, 0, (umb_t *)&_pid);
	int pid = (int)_pid;
	// 将cap与pid插入表中
	insert_proc_cap(pid, cap);
	// 将pid与cap对应起来
	if (pid == 0){
		// 子进程, 更新pcb_cap
		pcb_cap = cap;
		// 子进程也要插入自己的cap
		insert_proc_cap(get_current_pid(), cap);
	}
	return pid;
}

void *mapmem(CapPtr cap) {
    // TODO: 实现映射内存系统调用
    return nullptr;
}

CapPtr create_thread(void *entrypoint, int priority)
{
	return (CapPtr){
		.val = syscall(SYS_CREATE_THREAD, pcb_cap.val, (umb_t)(entrypoint), (umb_t)(priority), 0, 0, 0, 0)
	};
}

void wait_notifications(CapPtr thread, CapPtr notif_cap, qword *wait_bitmap)
{
	if (thread.val != 0) {
		// 线程级等待
		syscall(SYS_WAIT_NOTIFICATION_THREAD, thread.val, notif_cap.val, (umb_t)(wait_bitmap), 0, 0, 0, 0);
	}
	else {
		// 进程级等待
		syscall(SYS_WAIT_NOTIFICATION, pcb_cap.val, notif_cap.val, (umb_t)(wait_bitmap), 0, 0, 0, 0);
	}
}

/**
 * @brief 等待通知
 *
 * @param thread 线程能力. 如果是INVALID_CAP_PTR, 则表示以进程级别让出CPU
 * @param notif_cap 通知能力
 * @param notification_id 等待的通知ID
 */
void wait_notification(CapPtr thread, CapPtr notif_cap, int notification_id)
{
	qword wait_bitmap[NOTIFICATION_BITMAP_QWORDS] = {0};
	int qword_index = notification_id / (8 * sizeof(qword));
	int bit_index   = notification_id % (8 * sizeof(qword));
	wait_bitmap[qword_index] |= (1UL << bit_index);
	wait_notifications(thread, notif_cap, wait_bitmap);
}

/**
 * @brief 设置通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 */
void notification_set(CapPtr notif_cap, int notification_id)
{
	syscall(SYS_SET_NOTIFICATION, notif_cap.val, (umb_t)(notification_id), 0, 0, 0, 0, 0);
}

/**
 * @brief 重置通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 */
void notification_reset(CapPtr notif_cap, int notification_id)
{
	syscall(SYS_RESET_NOTIFICATION, notif_cap.val, (umb_t)(notification_id), 0, 0, 0, 0, 0);
}

/**
 * @brief 检查通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool check_notification(CapPtr notif_cap, int notification_id)
{
	umb_t ret = syscall(SYS_CHECK_NOTIFICATION, notif_cap.val, (umb_t)(notification_id), 0, 0, 0, 0, 0);
	return ret != 0;
}

CapPtr get_pcb_cap(void) {
	return pcb_cap;
}

CapPtr get_main_thread_cap(void) {
	return main_thread_cap;
}

CapPtr get_notification_cap(void) {
	return default_notif_cap;
}