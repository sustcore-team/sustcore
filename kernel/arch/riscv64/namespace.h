/**
 * @file namespace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V64 架构子系统的命名空间适配。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

namespace boot {}
namespace cap {}
namespace cpu {}
namespace device {}
namespace device::interrupt {}
namespace firmware {}
namespace init {}
namespace kernel {}
namespace memory {}
namespace scheduler {}
namespace smp {}
namespace task {}
namespace tay {}

// 架构子树只映射同名的通用子系统，避免把实现依赖泄漏到无关命名空间。
namespace riscv64::boot {
    using namespace ::boot;
}
namespace riscv64::cap {
    using namespace ::cap;
}
namespace riscv64::cpu {
    using namespace ::cpu;
}
namespace riscv64::device {
    using namespace ::device;
}
namespace riscv64::device::interrupt {
    using namespace ::device::interrupt;
}
namespace riscv64::firmware {
    using namespace ::firmware;
}
namespace riscv64::init {
    using namespace ::init;
}
namespace riscv64::kernel {
    using namespace ::kernel;
}
namespace riscv64::memory {
    using namespace ::memory;
}
namespace riscv64::scheduler {
    using namespace ::scheduler;
}
namespace riscv64::smp {
    using namespace ::smp;
}
namespace riscv64::task {
    using namespace ::task;
}
namespace riscv64::tay {
    using namespace ::tay;
}
