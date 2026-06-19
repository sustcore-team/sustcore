/**
 * @file fwd.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 全局共享前向声明
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace cap {
    class Payload;
    class Capability;
    class CGroup;
    class CHolder;
}  // namespace cap

class TaskMemoryManager;

namespace device {
    class DeviceModel;
    class DevResManager;
    class MMIOManager;
}  // namespace device

namespace driver {
    class DriverModel;
    class IrqDomain;
    class IrqManager;
    struct IrqEvent;
    enum class IrqTrigger;
}  // namespace driver

namespace task {
    struct TCB;
    struct PCB;
}  // namespace task

namespace env {
    class HartContext;
}  // namespace env