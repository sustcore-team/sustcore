/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程相关结构
 * @version alpha-1.0.0
 * @date 2026-04-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <string_view>
#include <cap/cholder.h>
#include <mem/vma.h>
#include <sustcore/addr.h>

/**
 * @brief Load Parameter
 *
 * 用于标记加载参数, 需要注意的是, 诸如Task Memory, Image File等资源类数据存放在TaskSpec中
 *
 */
struct LoadPrm
{
    // 进程文件的能力索引
    CapIdx image_file_cap;
    // 进程文件的路径
    std::string_view src_path;
};

/**
 * @brief Task Specification
 *
 * Task Manager从中构造出task对象
 * 其由Task Manager构造, 并由loader填充一部分
 * 其包含了 Task 所持有的一些资源
 * 所有的资源均由Task Manager构造
 *
 */
struct TaskSpec
{
    // 进程内存管理
    util::owner<TaskMemoryManager *> tmm;
    // 进程Capability Holder
    // 进程文件能力已经存放在该CHolder中了
    CHolder * holder;
    // 入口点
    VirAddr entrypoint;
};