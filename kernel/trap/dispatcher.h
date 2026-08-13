/**
 * @file dispatcher.h
 * @brief 架构无关的运行期 trap 分派入口。
 */

#pragma once

#include <arch/frame.h>

namespace kernel::trap {
    /** @brief 分派一次已经保存完整 TrapFrame 的运行期 trap。 */
    void dispatch(hal::TrapFrame &frame) noexcept;
}  // namespace kernel::trap
