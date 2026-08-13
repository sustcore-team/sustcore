/**
 * @file objfwd.h
 * @brief KernelObject 体系的集中前向声明。
 */

#pragma once

namespace cap {
    class KernelObject;
    class CSpace;
    class IntegerObject;
}  // namespace cap

namespace memory {
    class MemorySegment;
}  // namespace memory

namespace task {
    class AddressSpace;
    class Process;
    class Thread;
}  // namespace task
