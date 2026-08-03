# Sustcore Kernel

Sustcore 内核使用 freestanding C++23，当前支持 RISC-V64/SBI 与 LoongArch64/LABOOT。

活动 BSP 路径已经完成 BootInfo/FDT 持久化、PageDatabase、Buddy、全局 SLUB 堆、普通 C++
constructors、KernelSpace、KernelMM、boot/init 内存回收和 runtime trap vector 安装。

当前调度器是 BSP-only 的协作式 FIFO 实现：kinit 使用既有 BSP 栈，动态 worker 各自拥有
64 KiB 内核栈，运行队列使用 `tay::intrusive_list`，所有上下文切换经由双架构共同的
`__switch_to` ABI。启动主流程会运行 kinit 与两个 worker 的严格轮转测试，然后停机。

当前尚未恢复 Process、用户 Thread、timer 抢占、通用 IRQ dispatcher、IPI、AP 启动、SMP
调度和设备 probe。历史目录与旧文档中关于完整 Stage 2/SMP 已就绪的描述不能用于判断当前
运行行为。

内核开发约束见 `AGENTS.md`，早期初始化和并发边界见 `aidoc/kernel/early_setup.md`。
