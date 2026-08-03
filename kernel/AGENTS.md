# Sustcore 内核智能体开发指南

本文件适用于 `kernel/` 子树，并在根目录 `AGENTS.md` 的基础上补充内核专用约束。当前启动流程与并发边界的完整说明见 `aidoc/kernel/early_setup.md`。

## 启动阶段与全局对象

- 普通 C++ 全局构造函数只能在 `PageDatabase`、Buddy 和全局 SLUB 堆到达 `HEAP_READY` 后执行。
- `HEAP_READY` 之前可达的代码属于启动运行时，只能依赖常量初始化完成的全局对象，不得依赖 `.init_array` 条目。
- 启动期全局对象应显式使用 `constexpr` 或 `constinit` 表达常量初始化要求，并保持无动态分配、无隐式运行时初始化。
- 新增或调整全局对象前，应核对其首次访问阶段、构造依赖和回收边界。不要通过函数内部静态对象隐藏初始化顺序。
- BSP 负责执行一次普通 C++ constructors；AP 不得重复执行构造器，也不得进入已经回收的 init 代码。
- `boot/` 下的代码还必须遵守 `kernel/boot/README.md` 中的启动运行时约束。

## 中断与并发

- Sustcore 不使用 Big Kernel Lock。共享状态必须由所属对象或子系统的锁、原子协议或 per-CPU 所有权保护。
- 硬中断处理器不得分配内存、阻塞或取得依赖远端调度才能释放的锁。
- 硬中断可以确认设备状态、更新固定容量队列、唤醒已有等待者并请求重新调度；上下文切换必须推迟到通用 trap-return 路径。
- 可能同时在任务上下文与本地中断中获取的锁，必须使用内核提供的 irq-save Context Guard 保护。
- 页表解除映射和相关物理页回收必须遵守 TLB shootdown 的 generation/acknowledgement 协议；远端 shootdown handler 不得获取页表对象锁。
- 跨 CPU 唤醒必须先把线程加入目标 CPU 的 run queue，再发送 reschedule IPI，不能用全局锁绕过目标队列所有权。

## 架构与目录边界

- 通用代码只依赖 `kernel/arch/*.h` 暴露的窄接口。
- 架构专用 trap frame、寄存器、页表格式和中断控制器实现应位于 `kernel/arch/<isa>/`。
- 修改 RISC-V64 或 LoongArch64 启动、页表、IPI、timer 或中断代码时，应验证另一架构的公共接口没有被意外破坏。
- `kernel/boot` 只负责固件交接、BootInfo/FDT 校验与持久化、早期内存和最终页表切换；永久内核的组合根位于 `kernel/init/bsp.cpp`。

## 验证要求

- 内核代码修改至少应执行目标架构的 `make build-kernel`；涉及公共架构接口时应构建 RISC-V64 和 LoongArch64 的相关 debug/release 变体。
- 涉及启动、SMP、中断、timer、调度、分配器或页表的修改，应在适用配置下运行内核 selftest。
- 运行 QEMU 或调试目标必须使用 `timeout`。调试模式使用 `make dbgonly`，并通过 GDB 的 `target remote :1234` 连接。
- 仅修改 Markdown 文档时可以不构建，但必须检查 Markdown 格式和链接。
