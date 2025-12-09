# TCB能力与线程调度规范

本文档描述了sustcore系统中TCB(Thread Control Block, 线程制块)能力的设计与操作规范。TCB能力是系统中用于管理和控制线程的关键能力，赋予持有者对线程的创建、调度、终止等操作权限。

## Sustcore线程系统与TCB能力概述

Sustcore操作系统采取混合式线程模型。出于系统设计(RPC, 中断处理等)的需要, 线程对内核而言不能是透明的。同时出于效率考虑, Sustcore也不希望线程切换都需要进入内核态。因此Sustcore设计了一套混合式线程模型。每个线程都在内核中有一个对应的线程控制块(TCB), 用于保存线程的状态和上下文信息。线程的创建、调度和终止等操作需要通过TCB能力来进行管理和控制。同时, Sustcore将通过用户态线程库来实现用户态线程的调度和管理, 以提高线程切换的效率。而对应的用户态线程库通过TCB能力来访问和操作内核中的线程控制块, 实现对线程的管理和调度。

对线程的调度分为内核态调度和用户态调度两类。当发生时钟中断或系统调用等需要进入内核态的事件时, 线程切换将由内核态调度器负责, 以确保系统的稳定性和安全性。在用户态下, 相应的用户态线程库将负责线程的调度和管理, 以提高线程切换的效率。同时, 用户态线程库通过TCB能力来访问和操作内核中的线程控制块, 实现对线程的管理和调度。

### 1. 一般的线程调度流程

采取基于priority的抢占式调度算法。线程调度的基本流程如下:
1. 当线程A执行时, 发生时钟中断或系统调用等需要进入内核态的事件, 线程A陷入到内核态中, 进入调度器处理流程
2. 内核态调度器根据线程的优先级和状态等信息,选择下一个要运行的线程B
   - 具体而言, 选择有着最高优先级且处于就绪状态的线程B
3. 内核保存线程A的上下文信息到其对应的TCB中, 并加载线程B的上下文信息到CPU寄存器中
4. 切换到线程B的执行流, 继续执行线程B的代码

需要注意的是, 线程调度分为两个阶段: 进程调度与线程调度。内核首先进行进程调度, 在进程调度结束后进行进程内的线程调度。也就是说, 内核态调度器首先选择下一个要运行的进程, 然后在该进程内选择下一个要运行的线程。

### 2. 用户态线程库

用户态线程库可以通过TCB能力来访问和操作内核中的线程控制块, 实现对线程的管理和调度。用户态线程库可以实现多种线程调度算法, 例如时间片轮转、优先级调度等, 以满足不同应用的需求。

### 3. fastpath线程切换流程

fastpath是针对RPC调用等高频率线程切换场景设计的一种优化机制。 当线程A需要调用线程B时, 如果线程B处于就绪状态, 则可以通过fastpath机制直接切换到线程B, 避免消息传递和上下文切换的开销。 具体流程如下:

1. 线程A通过fastpath调用线程B, 陷入到内核态中, 进入fastpath处理流程
2. 线程B必须通过其TCT注册了fastpath入口点. 如果线程B注册了fastpath入口点且可用,
则内核直接将线程A的上下文临时作为线程B的上下文, 并跳转到线程B的fastpath入口点执行。
3. 线程B执行完fastpath逻辑后, 通过特定的系统调用返回到内核态, 将自身的上下文作为线程A的上下文, 然后切换回线程A继续执行。

## 规范

### 1. TCB能力定义

#### 1.1 TCB能力权限

TCB能力一共包含以下权限:

```c
typedef struct {
    bool priv_create;    // 允许创建线程
    bool priv_terminate; // 允许终止线程
    bool priv_suspend;   // 允许挂起线程
    bool priv_resume;    // 允许恢复线程
    bool priv_setprio;   // 允许设置线程优先级
    bool priv_getinfo;   // 允许获取线程信息
    bool priv_share;     // 允许将该能力分享给其他进程
    bool priv_getctx;    // 允许获取线程的上下文信息
    bool priv_setctx;    // 允许设置线程的上下文信息
    bool priv_fastpath;  // 允许注册fastpath入口点
} TCBCapPriv;
```

#### 1.2 Thread状态定义

```c
typedef enum {
    TS_READY,      // 线程就绪状态
    TS_RUNNING,    // 线程运行状态
    TS_BLOCKED,    // 线程阻塞状态
    TS_SUSPENDED,  // 线程挂起状态
    TS_TERMINATED  // 线程终止状态
} ThreadState;
```

#### 1.3 TCB数据结构

```c
typedef struct TCBStruct {
    void *entry_point;     // 线程入口点
    ThreadState state;     // 线程状态
    int priority;          // 线程优先级
    void *ctx;             // 线程上下文信息
    void **ip;             // 线程指令指针
    void **sp;             // 线程栈指针
} TCB;
```

### 2. 对TCB能力的操作

#### 2.1 创建线程

前提: `TCBCapPriv::priv_create`

允许线程创建一个新的线程控制块，并返回对应的TCB能力。

```c
CapPtr tcbcap_create_thread(PCB *pcb, void *entry_point, int priority);
```

#### 2.2 设置线程优先级

前提: `TCBCapPriv::priv_setprio`
允许线程设置指定线程的优先级。

```c
void tcbcap_set_priority(PCB *pcb, CapPtr cap, int priority);
```

#### 2.3 获取线程信息

前提: `TCBCapPriv::priv_getinfo`
允许线程获取指定线程的状态和优先级信息。

```c
ThreadState tcbcap_get_state(PCB *pcb, CapPtr cap);
```
```c
int tcbcap_get_priority(PCB *pcb, CapPtr cap);
```

#### 2.4 终止线程

前提: `TCBCapPriv::priv_terminate`
允许线程终止指定的线程。

```c
void tcbcap_terminate_thread(PCB *pcb, CapPtr cap);
```

#### 2.5 获取线程上下文

前提: `TCBCapPriv::priv_getctx`
允许线程获取指定线程的上下文信息。
更准确地说, 其返回地是保存线程上下文的内存地址。

```c
void *tcbcap_get_context(PCB *pcb, CapPtr cap);
```

#### 2.6 设置线程上下文

前提: `TCBCapPriv::priv_setctx`
允许线程设置指定线程的上下文信息。
更准确地说, 传入的参数是保存线程上下文的内存地址。
所以线程管理器无需每次都通过系统调用来设置上下文,
只需在第一次构造TCB时设置好上下文地址,
线程切换时将内存中的上下文加载到CPU寄存器中即可。

```c
void tcbcap_set_context(PCB *pcb, CapPtr cap, void *ctx);
```

#### 2.7 挂起线程

前提: `TCBCapPriv::priv_suspend`
允许线程挂起指定的线程。
```c
void tcbcap_suspend_thread(PCB *pcb, CapPtr cap);
```

#### 2.8 恢复线程

前提: `TCBCapPriv::priv_resume`
允许线程恢复指定的线程。
```c
void tcbcap_resume_thread(PCB *pcb, CapPtr cap);
```

### 3. fastpath机制

#### 3.1 fastpath要求

注册了fastpath入口点的线程必须满足以下要求:
1. fastpath入口点必须是一个有效的函数地址, 并且该函数必须遵循特定的调用约定, 以保证进入该函数就像普通函数调用一样。
2. fastpath函数必须在执行完毕后通过特定的系统调用返回到内核态, 以确保线程切换的正确性。
3. fastpath入口点的执行时间应尽可能短, 以减少对系统调度的影响。
4. fastpath入口点的实现必须确保线程安全, 避免引入竞态条件或死锁等问题。

#### 3.2 fastpath注册

前提: `TCBCapPriv::priv_fastpath`
允许线程注册fastpath入口点。

```c
void tcbcap_register_fastpath(PCB *pcb, CapPtr cap, void *fastpath_entry);
```