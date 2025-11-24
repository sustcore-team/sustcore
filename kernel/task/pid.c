/**
 * @file pid.c
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief PID分配器实现
 * @version alpha-1.0.0
 * @date 2025-11-24
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "pid.h"
#include <mem/alloc.h>
#include <basec/logger.h>
#include <stddef.h>
// #include <assert.h>

static RecycleAllocator PID_ALLOCATOR;

// PID节点结构体
typedef struct PidNode {
    umb_t pid;
    struct PidNode *next;
} PidNode;


void pid_init(void) {
    PID_ALLOCATOR.current = 0;
    PID_ALLOCATOR.recycled = NULL;
}

umb_t pid_alloc(void) {
    if (PID_ALLOCATOR.recycled != NULL) {
        PidNode *node = PID_ALLOCATOR.recycled;
        PID_ALLOCATOR.recycled = node->next;
        umb_t pid = node->pid;
        kfree(node);
        return pid;
    } else {
        return PID_ALLOCATOR.current++;
    }
}

void pid_dealloc(umb_t pid) {
    // assert(pid < PID_ALLOCATOR.current);
    
    // Check if already recycled
    PidNode *iter = PID_ALLOCATOR.recycled;
    while (iter != NULL) {
        if (iter->pid == pid) {
            log_error("pid %d has been deallocated!", pid);
            // assert(0);
        }
        iter = iter->next;
    }

    PidNode *node = (PidNode *)kmalloc(sizeof(PidNode));
    node->pid = pid;
    node->next = PID_ALLOCATOR.recycled;
    PID_ALLOCATOR.recycled = node;
}
