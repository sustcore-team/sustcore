# Process System

本文总结 `kernel/task/task_struct.h` 与 `kernel/task/task.cpp` 中的进程系统。Sustcore 的进程由 `PCB` 表示，线程由 `TCB` 表示；进程拥有地址空间和 capability holder，线程负责承载可调度上下文。

## 核心对象

### `PCB`

`task::PCB` 是进程控制块，并继承 `util::tree_base::TreeBase<PCB>`，预留了进程树关系。

主要字段包括:

- `pid`: 进程 ID，由 `TaskManager` 分配。
- `is_kernel`: 是否为内核进程。
- `exit_code`: 退出码。
- `exiting`: 是否正在退出。
- `recycle_queued`: 是否已进入延迟回收队列。
- `threads`: 该进程拥有的 TCB 侵入式链表。
- `tmm`: 进程地址空间管理器。
- `cholder`: 进程 capability holder。
- `entrypoint`: 用户镜像入口地址。
- `pcb_cap`: 进程自身 PCB capability 所在槽位。
- `main_tcb_cap`: 主线程 TCB capability 所在槽位。

用户进程必须同时拥有有效的 `TaskMemoryManager` 和 `CHolder`。内核进程没有用户 capability holder，并绑定主内核页表。

### `TaskManager`

`TaskManager` 是任务系统单例，负责:

- 分配 `pid` / `tid`
- 创建、填充、终止 PCB/TCB
- 加载 ELF 并创建进程
- fork / exec 当前进程
- 创建内核进程与内核线程
- 维护 `pid -> PCB` 映射
- 执行延迟 PCB 回收

初始化方式是项目常见的显式单例:

- `TaskManager::init()`
- `TaskManager::initialized()`
- `TaskManager::inst()`

PCB 和 TCB 都使用 KOP 对象池，`task::init_kop()` 会初始化 `kop::pcb` 与 `kop::tcb`。

## 进程创建

### ELF 预加载

用户进程创建通常从 `preload()` / `preload_into()` 开始。

`preload(image_cap, spec, prm)` 会:

1. 创建新的 `cap::CHolder`。
2. 调用 `preload_into()` 填充 `TaskSpec` 和 `LoadPrm`。
3. 出错时移除新创建的 holder。

`preload_into(image_cap, holder, spec, prm)` 会:

1. 分配一个页表根页。
2. 构造 `TaskMemoryManager`，其内部会从 `env::inst().main_kernel_pgd()` 合并主内核页表映射。
3. 校验 `image_cap` 在 `holder` 中存在，payload 类型是 `VFILE`，并且具备 `perm::vfile::EXEC`。
4. 把 holder、tmm 和 image file cap 记录到 `TaskSpec` / `LoadPrm`。

因此用户态进程加载路径现在完全依赖“已打开的程序文件 capability”，而不是路径字符串。

### ELF 加载

`loader::elf::load(spec, load_prm)` 会:

1. 从 `holder` 中取出 `image_file_cap` 对应的 `VFile` payload，并要求该 capability 具备 `EXEC`。
2. 校验 ELF64、RISC-V、`ET_EXEC`。
3. 为每个 `PT_LOAD` 段创建 `MemoryPayload` 和 VMA。
4. 为堆创建 `HEAP` VMA，并把 heap memory cap 插入 holder。
5. 将段内容写入对应 `MemoryPayload`。
6. 执行 `fence.i`。
7. 将 VMA 从加载期权限切换为用户可访问最终权限。

加载器不直接把所有页预映射到页表。实际物理页映射依赖缺页异常路径懒完成。

### 填充 PCB

`populate_task(pcb, spec, schd_class, reuse_main_tcb)` 把加载结果接入 PCB:

1. 设置 `pcb->tmm`、`pcb->cholder`、`pcb->entrypoint`。
2. 若尚无 `pcb_cap`，在自身 holder 中插入 `cap::PCBPayload`。
3. 构造 `StartupInfo`。
4. 创建主线程或复用当前主线程。
5. 创建用户栈 `MemoryPayload` 和 `STACK` VMA。
6. 创建 `cap::TCBPayload` 并记录 `main_tcb_cap`。
7. 将启动信息写入用户栈。

启动信息包含:

- heap 起始地址
- stack 起始地址
- entrypoint
- PCB capability
- 主 TCB capability
- heap memory capability
- stack memory capability

## 创建入口

### `create_init_task`

`create_init_task(spec)` 用 `ClassType::INIT` 创建系统 init 进程。该调度类只用于启动阶段的 init 线程，普通用户进程不能请求该类。

### `create_task`

`create_task(spec, schd_class)` 用指定调度类创建普通用户进程。

禁止使用:

- `INIT`
- `IDLE`
- `BOT`

创建成功后会调用 `Scheduler::wakeup_new(main_tcb)` 把主线程加入调度队列，并检查是否需要抢占当前线程。

### `load_elf` / `load_elf_into`

`load_elf(image_cap, schd_class)` 会创建新的 holder、预加载、加载 ELF，再创建进程。

`load_elf_into(image_cap, holder, schd_class, startup_blob, startup_blob_size)` 使用调用方已经配置好的 holder。该接口主要用于 syscall 创建子进程时先复制初始 capability，再加载新镜像。

## fork

`fork_current(ret_slot)` 只能由当前用户进程发起。

主要流程在 `populate_forked_task()`:

1. 为子进程创建新的 `CHolder`。
2. 为子进程分配新页表并构造 `TaskMemoryManager`。
3. 调用父进程 `tmm->clone_to_cow(child_tmm)` 克隆 VMA 和 COW 页表映射。
4. 遍历父 holder，复制 capability。
5. 对 memory capability，优先绑定子 TMM 中已经由 VMA 克隆出的子 memory payload。
6. 用父线程 trap context 构造子线程上下文。
7. 子线程 `sepc += 4`，并把 `a0/a1` 设置为 fork 子路径返回值。
8. 在父子 holder 的同一 `ret_slot` 插入子 PCB capability。
9. 唤醒子线程。

父进程得到 `ForkResult{child_pcb_cap, child_pid}`。syscall 层会把子 PCB capability 槽位写回用户缓冲区。

## exec

`exec_current(image_cap, reserved_caps, reserved_count)` 会调用 `exec_pcb()` 替换当前进程镜像。

`exec_pcb()` 的关键语义:

1. 禁止 exec 内核进程。
2. 先校验 `reserved_caps` 和 `pcb_cap` 都存在。
3. 在破坏旧镜像前预加载并加载新 ELF。
4. 移除 holder 中除 `pcb_cap` 和 reserved caps 之外的所有 capability。
5. 移除旧线程；若目标是当前进程，则复用当前 TCB。
6. 清空旧 `tmm`、entrypoint、main TCB cap。
7. 用新 `TaskSpec` 填充同一个 PCB。
8. 当前进程 exec 时立即切换到新页表。
9. 释放旧 `TaskMemoryManager`。

当前实现中，加载新镜像后会大幅修改地址空间和能力空间；源码里也标注了失败回滚仍需完善。

## 退出与回收

进程退出通常从 `cap::PCBObject::kill()` 开始:

1. 校验 `KILL` 权限。
2. 设置 `exit_code` 和 `exiting`。
3. 清空进程 holder。
4. 将该进程所有线程标为 `WAITING`。
5. READY 线程会从调度队列中移除。
6. 如果杀死当前进程，则设置当前线程 `NEED_RESCHED` 并触发调度。

当最后一个 `PCBPayload` 被释放时，`PCBPayload::destruct()` 会调用 `TaskManager::enqueue_recycle(pcb)`。真正释放发生在 `handle_trap()` 入口处的 `TaskManager::reap_recycled()`:

- 终止所有线程。
- 删除 `TaskMemoryManager` 并释放页表根页。
- 从 `CHolderManager` 移除 holder。
- 从 `_pid_map` 删除 PCB。
- 释放 PCB 对象。

## 当前限制

当前进程系统仍有一些限制:

- PCB 树关系尚未成为 wait/parent 语义的一部分。
- exec 破坏旧镜像后的失败回滚仍不完整。
- `load_init(const char *path)` 仍保留一条内核内部的路径加载入口，用于启动早期从 `/initrd/` 拉起 init 进程。
- `terminate_pcb()` 释放页表根页，但页表中间页释放仍有 TODO。
- `lookup_holder_id()` 只返回 holder id，不暴露更完整的进程查询接口。
- 退出回收依赖 trap 入口触发 `reap_recycled()`。
