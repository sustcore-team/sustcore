/**
 * @file objfwd.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief KObject 体系的集中前向声明。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

namespace cap {
    class KObject;
    class CSpace;
}  // namespace cap

namespace memory {
    class MemSeg;
}  // namespace memory

namespace task {
    class AddrSpace;
    class Process;
    class Thread;
}  // namespace task
