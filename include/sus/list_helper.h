/**
 * @file list_helper.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 链表操作辅助函数
 * @version alpha-1.0.0
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

// 遍历链表
#define __foreach_list(x, head, tail, next, prev) \
    for (x = (head); x != nullptr; x = x->next)
#define foreach_list(x, list) __foreach_list(x, list)

// 计算链表长度
#define __list_length(len, head, tail, next, prev) \
    do {                                           \
        len = 0;                                   \
        foreach_list(x, head, tail, next, prev) {  \
            len++;                                 \
        }                                          \
    } while (0)
#define list_length(len, list) __list_length(len, list)

// 从链表中删去节点
#define __list_remove(node, head, tail, next, prev)               \
    do {                                                          \
        /** 分类讨论 */                                       \
        /** prev -> node -> next */                               \
        /** prev -> node */                                       \
        /** node -> next */                                       \
        /** node = head = tail */                                 \
        if ((node)->prev != nullptr && (node)->next != nullptr) { \
            (node)->prev->next = (node)->next;                    \
            (node)->next->prev = (node)->prev;                    \
        } else if ((node)->prev != nullptr) {                     \
            (node)->prev->next = nullptr;                         \
            tail               = (node)->prev;                    \
        } else if ((node)->next != nullptr) {                     \
            (node)->next->prev = nullptr;                         \
            head               = (node)->next;                    \
        } else {                                                  \
            head = nullptr;                                       \
            tail = nullptr;                                       \
        }                                                         \
        (node)->next = nullptr;                                   \
        (node)->prev = nullptr;                                   \
    } while (0)
#define list_remove(node, list) __list_remove(node, list)

// 初始化链表
#define __list_init(head, tail, next, prev) \
    do {                                    \
        head = nullptr;                     \
        tail = nullptr;                     \
    } while (0)
#define list_init(list) __list_init(list)

// 向链表尾部添加节点
#define __list_push_back(node, head, tail, next, prev) \
    do {                                               \
        if (tail == nullptr) {                         \
            head       = node;                         \
            tail       = node;                         \
            node->next = nullptr;                      \
            node->prev = nullptr;                      \
        } else {                                       \
            tail->next = node;                         \
            node->prev = tail;                         \
            node->next = nullptr;                      \
            tail       = node;                         \
        }                                              \
    } while (0)
#define list_push_back(node, list) __list_push_back(node, list)

// 向链表头部添加节点
#define __list_push_front(node, head, tail, next, prev) \
    do {                                                \
        if (head == nullptr) {                          \
            head       = node;                          \
            tail       = node;                          \
            node->next = nullptr;                       \
            node->prev = nullptr;                       \
        } else {                                        \
            head->prev = node;                          \
            node->next = head;                          \
            node->prev = nullptr;                       \
            head       = node;                          \
        }                                               \
    } while (0)
#define list_push_front(node, list) __list_push_front(node, list)

// 判断链表是否为空
#define __list_is_empty(head, tail, next, prev) ((head) == nullptr)
#define list_is_empty(list)                     __list_is_empty(list)

// 弹出链表头节点
#define __list_pop_front(node, head, tail, next, prev)   \
    do {                                                 \
        if (head == nullptr) {                           \
            node = nullptr;                              \
        } else {                                         \
            node = head;                                 \
            __list_remove(node, head, tail, next, prev); \
        }                                                \
    } while (0)
#define list_pop_front(node, list) __list_pop_front(node, list)

// 弹出链表尾节点
#define __list_pop_back(node, head, tail, next, prev)    \
    do {                                                 \
        if (tail == nullptr) {                           \
            node = nullptr;                              \
        } else {                                         \
            node = tail;                                 \
            __list_remove(node, head, tail, next, prev); \
        }                                                \
    } while (0)
#define list_pop_back(node, list) __list_pop_back(node, list)

// 接下来是有序链表的操作

// 遍历
#define __foreach_ordered_list(x, head, tail, next, prev, field, cmp) \
    for (x = (head); x != nullptr; x = x->next)
#define foreach_ordered_list(x, list) __foreach_ordered_list(x, list)

// 计算长度
#define __ordered_list_length(len, head, tail, next, prev, field, cmp) \
    do {                                                               \
        len = 0;                                                       \
        foreach_ordered_list(x, head, tail, next, prev, field, cmp) {  \
            len++;                                                     \
        }                                                              \
    } while (0)
#define ordered_list_length(len, list) __ordered_list_length(len, list)

// 初始化有序链表
#define __ordered_list_init(head, tail, next, prev, field, cmp) \
    do {                                                        \
        head = nullptr;                                         \
        tail = nullptr;                                         \
    } while (0)
#define ordered_list_init(list) __ordered_list_init(list)

// 向有序链表中插入节点
// 其有序性在此处得到维护
#define __ordered_list_insert(node, head, tail, next, prev, field, cmp)       \
    do {                                                                      \
        if (head == nullptr) {                                                \
            head       = node;                                                \
            tail       = node;                                                \
            node->next = nullptr;                                             \
            node->prev = nullptr;                                             \
        } else {                                                              \
            /* 找到插入位置 */                                          \
            typeof(node) curr = head;                                         \
            while (curr != nullptr && !(cmp((node)->field, (curr)->field))) { \
                curr = curr->next;                                            \
            }                                                                 \
            if (curr == head) {                                               \
                /* 插入到头部 */                                         \
                node->next = head;                                            \
                node->prev = nullptr;                                         \
                head->prev = node;                                            \
                head       = node;                                            \
            } else if (curr == nullptr) {                                     \
                /* 插入到尾部 */                                         \
                tail->next = node;                                            \
                node->prev = tail;                                            \
                node->next = nullptr;                                         \
                tail       = node;                                            \
            } else {                                                          \
                /* 插入到中间 */                                         \
                node->next       = curr;                                      \
                node->prev       = curr->prev;                                \
                curr->prev->next = node;                                      \
                curr->prev       = node;                                      \
            }                                                                 \
        }                                                                     \
    } while (0)
#define ordered_list_insert(node, list) __ordered_list_insert(node, list)

// 从有序链表中删除节点
#define __ordered_list_remove(node, head, tail, next, prev, field, cmp) \
    do {                                                                \
        /** 分类讨论 */                                             \
        /** prev -> node -> next */                                     \
        /** prev -> node */                                             \
        /** node -> next */                                             \
        /** node = head = tail */                                       \
        if ((node)->prev != nullptr && (node)->next != nullptr) {       \
            (node)->prev->next = (node)->next;                          \
            (node)->next->prev = (node)->prev;                          \
        } else if ((node)->prev != nullptr) {                           \
            (node)->prev->next = nullptr;                               \
            tail               = (node)->prev;                          \
        } else if ((node)->next != nullptr) {                           \
            (node)->next->prev = nullptr;                               \
            head               = (node)->next;                          \
        } else {                                                        \
            head = nullptr;                                             \
            tail = nullptr;                                             \
        }                                                               \
        (node)->next = nullptr;                                         \
        (node)->prev = nullptr;                                         \
    } while (0)
#define ordered_list_remove(node, list) __ordered_list_remove(node, list)

// 弹出有序链表头节点
#define __ordered_list_pop_front(node, head, tail, next, prev, field, cmp) \
    do {                                                                 \
        if (head == nullptr) {                                           \
            node = nullptr;                                              \
        } else {                                                         \
            node = head;                                                 \
            __ordered_list_remove(node, head, tail, next, prev, field, cmp); \
        }                                                                \
    } while (0)
#define ordered_list_pop_front(node, list) __ordered_list_pop_front(node, list)

// 弹出有序链表尾节点
#define __ordered_list_pop_back(node, head, tail, next, prev, field, cmp) \
    do {                                                                \
        if (tail == nullptr) {                                          \
            node = nullptr;                                             \
        } else {                                                        \
            node = tail;                                                \
            __ordered_list_remove(node, head, tail, next, prev, field, cmp); \
        }                                                               \
    } while (0)
#define ordered_list_pop_back(node, list) __ordered_list_pop_back(node, list)

// 判断有序链表是否为空
#define __ordered_list_is_empty(head, tail, next, prev, field, cmp) ((head) == nullptr)
#define ordered_list_is_empty(list)                     __ordered_list_is_empty(list)

// 判断有序链表有序性是否被破坏 (调试用)
#define __ordered_list_is_sorted(head, tail, next, prev, field, cmp, sorted) \
    do {                                                               \
        sorted = true;                                         \
        if (head != nullptr) {                                      \
            typeof(head) curr = head;                               \
            while (curr->next != nullptr) {                         \
                if (!cmp(curr->field, curr->next->field)) {         \
                    sorted = false;                                 \
                    break;                                          \
                }                                                   \
                curr = curr->next;                                  \
            }                                                       \
        }                                                           \
    } while(0)
#define ordered_list_is_sorted(list) __ordered_list_is_sorted(list)

// 接下来是一些辅助宏，用于比较
#define less_than(a, b)    ((a) < (b))
#define greater_than(a, b) ((a) > (b))
#define equal_to(a, b)     ((a) == (b))

// 将其包装成升序/降序
#define ascending(a, b)  less_than(a, b)
#define descending(a, b) greater_than(a, b)