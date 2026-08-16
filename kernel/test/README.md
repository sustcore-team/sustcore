# Kernel selftest

内核运行时测试通过 `kernel::test::run_phase()` 按初始化阶段串行执行。用例在
`framework.cpp` 的显式静态表中注册，不依赖全局构造函数、动态分配注册表或链接器 section
自动发现。

当前阶段如下：

- `POST_TIMER_INITIALIZATION`：IRQ、timer 和内核堆已经可用，但 init 内存尚未回收；
- `POST_SCHEDULER_INITIALIZATION`：kernel process、kinit Thread 和 Scheduler 已发布；
- `POST_WORK_QUEUE_INITIALIZATION`：永久 BSP WorkQueue 已启动，可执行实际 timer completion；
- `PRE_IDLE`：usrboot 已提交，kinit 即将转为 idle Thread。

新增用例时应：

1. 在独立测试翻译单元中实现 `void(Context &) noexcept` 入口；
2. 在 `cases.h` 声明入口，并在 `framework.cpp` 为其选择最早满足前置条件的阶段；
3. 使用 `kernel::test::require()` 或 `kernel::test::fail()` 报告失败；
4. 保证入口返回前已经回收临时 Thread、Worklet、Capability、timer node 和其他借用资源；
5. 不在用例通过后遗留后台测试任务。

测试源码仅在 `enable-kernel-selftests=y` 时参与内核构建。生产启动代码只负责在既定边界调用
`run_phase()`，不得直接调用具体测试入口。
