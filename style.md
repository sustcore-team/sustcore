# Sustcore 当前代码风格

本文档总结 Sustcore 当前活动代码树中已经形成的代码风格，主要依据：

- `.clang-format` 与 `.editorconfig`；
- `kernel/` 下的内核实现；
- `libs/` 下的 C/C++ 基础库；
- `script/` 下的 Python 与 Make 构建系统；
- 当前测试、TOML 元数据和 Markdown 文档。

本文档是一份描述现状的草案，不代表其中每一种既有写法都已经成为最终规范。
`.vscode/sustcore/` 属于历史实现，不用于归纳当前风格。

## 一、通用原则

- 优先遵守项目已有格式化配置和相邻文件风格，不手工制造与格式化器冲突的排版。
- 代码应尽量直接表达所有权、失败、初始化阶段和并发边界，避免依赖隐含约定。
- 注释主要解释设计意图、前置条件、不变量和失败后的保证，不逐行翻译代码。
- 内核和公共基础库中的新增说明优先使用中文；API 名称、架构名称和标准术语保留原文。
- 修改生成结果时，应修改生成器、配置或元数据，不直接编辑 `script/.cache/` 和构建目录。
- 新代码应贴合所在子系统。内核领域代码与 Tay 的标准库风格接口在命名上存在有意差异。

## 二、文件与基础格式

### 字符与缩进

- 文件编码为 UTF-8。
- 行尾使用 LF。
- 使用空格缩进，缩进宽度为 4。
- 删除行尾空白。
- 当前 `.editorconfig` 设置 `insert_final_newline = false`，不强制文件末尾存在换行。
- C/C++ 默认行宽上限为 100 列。

### C/C++ 格式化

项目的 `.clang-format` 以 Google 风格为基础，并具有以下主要覆盖项：

- 指针和引用符号靠近变量一侧，例如 `Type *pointer`、`Type &reference`。
- 命名空间内容整体缩进。
- `case` 标签相对 `switch` 缩进。
- 空代码块和空函数可以保留在单行，普通短函数和短 `if` 不压缩到单行。
- 函数、类、结构体、枚举和命名空间的左花括号通常不单独换行。
- 多行控制语句允许按格式化结果换行放置花括号。
- 连续赋值、宏和短 `case` 语句会在适当范围内对齐。
- 参数和实参允许紧凑装箱，由 clang-format 决定换行位置。

不要为了手工对齐而大范围改动无关代码；可机械格式化的排版以 `.clang-format` 为准。

## 三、C/C++ 文件组织

### 文件头

内核 C/C++ 文件普遍使用 Doxygen 文件头，常见字段为：

```cpp
/**
 * @file example.cpp
 * @author ...
 * @brief 文件职责的简短说明
 * @version ...
 * @date ...
 *
 * @copyright ...
 */
```

较新的小文件有时只保留 `@file` 和 `@brief`。文件头应说明文件职责，而不是重复文件名。

### 头文件保护

- C/C++ 头文件使用 `#pragma once`。
- 不新增传统的 `#ifndef`/`#define` include guard，除非必须兼容特殊工具链。

### `#include` 分组

当前代码通常使用以下分组：

1. 项目和组件头文件；
2. 空行；
3. C/C++ 标准头文件。

项目内部头文件通常也使用尖括号，例如：

```cpp
#include <memory/virtual/page_table.h>
#include <tay/expected.h>

#include <cstddef>
#include <utility>
```

同一分组内保持稳定、易查找的顺序，不为纯字母排序大范围扰动已有文件。

### 命名空间

- 命名空间名称使用小写 `snake_case`，例如 `memory`、`kernel::log`、`memory::paging`。
- 命名空间内容按项目配置缩进 4 空格。
- 命名空间结束处通常注明名称：

```cpp
}  // namespace memory
```

- 只在当前翻译单元使用的类型、常量和辅助函数放入匿名命名空间。

## 四、命名约定

### 内核与公共领域类型

内核对象、领域数据结构和公开概念通常使用 `UpperCamelCase`：

```cpp
class KernelSpace;
class ClientSpace;
struct PageAllocation;
enum class PageTableKind;
```

类型别名如果表达领域对象或标识符，也通常使用 `UpperCamelCase`：

```cpp
using KernelLayoutId = u64_t;
using PageTableOwnerId = u64_t;
```

C ABI 或基础整数类型仍可使用 `_t` 后缀，例如 `addr_t`、`u64_t`。

### 工具类与工具类型别名

不表达具体内核领域对象、用于组合执行上下文、同步和所有权语义的可复用工具类型使用
`c_style_case`，即全小写并以下划线分词：

```cpp
hal::interrupt_guard
kernel::lock_guard
kernel::synchronized
kernel::locked_ref
```

是否属于工具类型由用途决定，而不是由其是否为模板决定。`PageTable`、`ClientSpace`、
`Slub` 和 `BootInfoBuilder` 等表达内核领域职责的类型仍使用 `UpperCamelCase`。

#### 工具类型与实现别名的边界

`c_style_case` 不能按“没有对象身份”或“只是编译期机制”机械扩张。下列类型仍使用
`UpperCamelCase`：

- concept 与 traits，例如 `ContextTrait`、`EarlyConsoleTraits` 和 `PageTableTraits`；
- 静态策略和架构操作集合，例如 `PageTableOps`；
- locator、adapter 和 callback 类型，例如 `ChunkHookLocator`、`RQHookLocator`、
  `LeafVisitor` 和 `kernel::log::Output`；
- 普通机械别名和函数指针别名，例如 `EntryType`、`PteType`、`initializer_t`、
  `BootInfoBuilderType` 和 `Ops`；
- 自身实现了行为或不变量的内部类，例如 `SlubList`。

`c_style_case` 只用于以下两类：

1. 跨子系统复用、语义接近基础库设施的同步、guard 和所有权工具，例如
   `interrupt_guard`、`synchronized`、`lock_guard` 和 `locked_ref`；
2. 直接为 `taycpplib` 容器实例提供局部短名、不增加独立行为或领域语义的实现别名，例如
   `area_vector`、`chunk_list`、`kernel_layout_list`、
   `reserved_layout_list`、`hhdm_layout_list` 和 `exclusion_list`。

第二类名称只是底层容器类型的局部缩写；一旦封装开始维护自己的状态、不变量或操作，就应
提升为使用 `UpperCamelCase` 的正常类型，而不能继续以容器别名规则命名。
`HeapSlubList` 是具有 `find`、`find_exact` 和 `for_each` 行为的 `detail::SlubList` 配置，
不属于直接容器别名，保持 `UpperCamelCase`。

上述规则不按名称后缀机械套用。下列类型继续使用 `UpperCamelCase`：

- 具有对象身份、所有权或生命周期的子系统对象，例如 `KernelSpace`、`ClientSpace`、
  `PageTable`、`KernelMM`、`Buddy`、`PageDatabase` 和 `EarlyConsole`；
- 表达协议、状态、错误或领域值的类型，例如 `RootBinding`、`PageFlags`、`TrapInfo`、
  `KernelLayout`、`BootInfoHeader` 以及各种 `...Id`；
- 虽然名称包含 `Allocator`、`Builder`、`Pool`、`Walker` 或 `Sink`，但封装了明确领域不变量
  和资源生命周期的类型，例如 `PageAllocator`、`RetirementSink`、`Walker`、
  `BootInfoBuilder`、`Slub`、`SlubPool` 和 `MixedSlabsAllocator`；
- 必须保持外部 ABI 布局或公共语义的地址、固件和启动协议类型。

执行这些重命名时应按一个完整接口族同步声明、定义、concept、模板实参、文档和测试，
不得只修改别名声明后保留混合命名。

### Tay 标准库风格类型

Tay 中模仿标准库语义的容器、算法和辅助类型通常使用小写 `snake_case`：

```cpp
tay::expected
tay::static_vector
tay::intrusive_list
tay::lock_guard
```

不要仅为了统一外观而把 Tay 类型改成内核领域类型的命名风格。

### 函数、方法与变量

- 函数、方法、局部变量和参数使用 `snake_case`。
- 布尔查询通常使用直接描述状态的名称，例如 `initialized()`、`nonnull()`、`hhdm_covers()`。
- 可能失败且具有非失败对应操作的接口常使用 `try_` 前缀，例如 `try_allocate()`、`try_create()`。
- 工厂接口常使用 `create()`，并通过返回类型表达失败。
- 初始化接口使用 `initialize()` 或 `init()`；同一子系统内应保持一致，不在局部重新发明名称。

### 成员变量

当前大多数内核和 Tay 实现使用尾下划线区分非公开数据成员：

```cpp
PhyAddr root_{};
size_t free_pages_ = 0;
bool initialized_ = false;
```

静态成员也沿用相同规则，例如 `instance_`、`ready_`。访问器通常使用去掉尾下划线后的名称。

### 常量、宏与枚举

- `constexpr` 常量和编译期容量通常使用 `UPPER_CASE_STYLE`。
- 宏和配置开关使用 `UPPER_CASE_STYLE`。
- 枚举类型遵循其所在库的类型命名方式，枚举值使用 `UPPER_CASE_STYLE`。

```cpp
static constexpr size_t MAX_ORDER = 30;

enum class CacheMode : u8_t {
    NORMAL,
    DEVICE,
};
```

- 对仅用于模板实现的内部标识符，应跟随相邻代码，避免在新公共接口中使用保留标识符形式。

### 模板参数

- 类型模板参数通常使用 `UpperCamelCase`，例如 `T`、`Allocator`、`Storage`、`Compare`。
- 非类型模板参数使用简短且含义明确的名称，例如 `N`、`Order`。
- 模板约束优先使用 C++ concepts 和 `requires` 表达。

## 五、类型与接口设计

### 强类型优先

- 地址、单位、句柄、所有权和状态应使用现有强类型，不退回无约束裸整数。
- 地址相关代码优先使用 `PhyAddr`、`VirAddr`、`HvaAddr`、`KpaAddr`、`KvaAddr` 和对应 area 类型。
- 仅在硬件 ABI、位域运算或明确的转换边界处取出底层整数。
- 强制转换应贴近边界，并选择能表达意图的 `static_cast`、`reinterpret_cast` 或项目转换接口。

### 类与资源对象

- 单一所有者对象通常不可复制，并根据语义决定是否允许移动。
- 无继承需要的最终实现类常标记 `final`。
- 析构函数负责释放由对象拥有的资源，避免把同一释放责任同时分散给析构函数和外部 `cleanup()`。
- 使用构造函数建立必定成功的基本不变量；可能失败的资源获取或后续初始化通过工厂和 `expected` 表达。
- 默认构造、复制、移动或析构语义明确时使用 `= default` 或 `= delete`。
- 数据载体优先使用简单 `struct` 和默认成员初始化。
- 配置类数据结构常使用聚合初始化和 designated initializer：

```cpp
PageFlags{
    .readable = true,
    .writable = false,
    .executable = false,
};
```

### 属性和编译期约束

- 不应忽略的返回值使用 `[[nodiscard]]`。
- 不抛出异常的接口使用 `noexcept`，尤其是内核底层、析构和锁相关接口。
- 可以在编译期求值的对象和函数使用 `constexpr`。
- 必须在常量初始化阶段建立的全局对象使用 `constinit`。
- 永不返回的入口使用 `[[noreturn]]`。
- 使用 `static_assert` 固化对象尺寸、对齐、模板约束和硬件 ABI 假设。
- 不为追求标记数量而添加属性；属性必须反映真实语义。

## 六、错误处理与失败语义

### 无异常接口

Freestanding 内核代码不依赖 C++ 异常传播。可恢复失败主要使用：

```cpp
tay::expected<T, E>
tay::expected<void, E>
tay::error_code
tay::Err(error)
```

- 成功返回值可使用直接值、`return {};` 或项目已有便利接口。
- 错误路径优先前置返回，避免多层嵌套。
- 传播错误时保留原始错误语义，除非当前抽象层确实需要转换错误。
- `and_then`、`transform`、`or_else` 和 `transform_error` 适合能提高可读性的组合流程；简单分支不必强行写成链式表达式。
- Tay 为 Host 兼容性保留的条件异常代码不代表内核可以依赖异常。

### Panic、断言与普通错误

- 参数无效、资源不足、越界等调用者可处理的情况返回 `expected` 错误。
- `assert` 用于表达调试期不变量和编程错误，不代替可恢复错误处理。
- `kernel::log::panic()` 或 `tay::panic()` 用于契约已经破坏、状态不可能继续或当前接口无法恢复的失败。
- 回滚失败、页表状态变化和核心分配器元数据损坏等内部一致性错误通常直接 panic。

### 回滚

- 多阶段操作应先验证输入，再获取资源，再提交可见状态。
- 中途失败必须恢复已经修改的映射、计数、链表或所有权状态。
- 使用 RAII guard 或清晰的局部回滚逻辑，保证每个资源只有一个释放责任。
- 回滚路径本身如果违反已经验证的不变量，应报告为不可恢复错误，而不是静默忽略。

## 七、所有权、容器与内存

- 使用 RAII 表达资源生命周期。
- `tay::unique_ptr` 表达独占堆对象；`tay::owner` 只标注所有权语义，不自动完成释放。
- 借用指针或引用不得超过被借用对象的生命周期。
- 固定容量场景优先使用 `static_vector`、`static_fifo`、`static_bitmap` 等不分配容器。
- 动态容器通过 `tay::allocator` 分配，并用 `expected` 报告可恢复的分配失败。
- 内核对象已经包含链接节点时，可使用 `tay::intrusive_list` 等侵入式容器，容器不接管对象生命周期。
- 乘法计算分配尺寸时检查整数溢出。
- 硬中断和其他明确禁止分配的路径不得使用可能触发堆分配的容器或回调包装。

## 八、并发与执行上下文

- Sustcore 不使用 Big Kernel Lock；锁应靠近其保护的数据和子系统。
- 共享状态使用对象局部锁、`kernel::synchronized`、原子变量或明确的 per-CPU 所有权。
- 基础锁只负责同步；中断和抢占状态由相应 Context Guard 管理。
- 同一把锁可能在任务上下文和本地中断中获取时，任务上下文必须使用 irq-save 保护。
- 临界区保持短小，不在持有自旋锁时执行可能阻塞、远端等待或不受控分配的操作。
- 原子操作必须明确其发布、获取和所有权语义，不把 `memory_order_relaxed` 用作默认装饰。
- 硬中断处理器不分配、不阻塞，也不取得需要调度其他线程才能释放的锁。
- 远端 IPI、TLB shootdown 和跨 CPU 唤醒遵守各自的发布与 acknowledgement 协议。

## 九、全局对象与初始化阶段

- 普通静态全局对象可以直接定义，但其构造时机必须符合所在启动阶段。
- `HEAP_READY` 前可达的对象必须能够常量初始化，不依赖 `.init_array` 或动态分配。
- 启动期对象优先使用 `constexpr` 构造和 `constinit` 定义。
- 需要运行时资源的单例在堆就绪后完成初始化，并通过显式 readiness 状态保护访问。
- 避免使用函数内部静态对象隐藏构造顺序和并发初始化。
- BSP 只执行一次普通 C++ constructors；AP 不重复构造全局对象，也不进入已回收的 init 代码。

## 十、函数实现风格

- 函数保持单一职责，复杂流程拆分为文件局部辅助函数。
- 参数检查和快速失败放在函数前部，主成功路径保持顺直。
- 对单语句条件分支，当前代码经常省略花括号；多语句分支使用花括号，并以 clang-format 结果为准。
- 使用 `auto` 减少重复类型，尤其适用于迭代器、模板返回值和工厂结果；当显式类型能表达关键语义时保留类型名。
- 局部不可变值使用 `const`，编译期常量使用 `constexpr`。
- 避免无法解释的裸数值；页大小、位掩码、错误码和硬件字段优先使用命名常量。
- 文件局部辅助函数放入匿名命名空间；C 文件使用 `static` 限定内部链接。
- 汇编使用 `asm volatile`，并完整声明输入、输出和 clobber，硬件语义应由注释说明。

## 十一、注释、日志与文档

### 注释

- 文件职责、公开接口和复杂类型使用 Doxygen 注释。
- `@brief` 简短说明职责；必要时使用 `@param`、`@return`、`@note` 和 `@warning`。
- 注释解释“为什么”和“必须满足什么”，不重复显而易见的赋值或循环。
- 并发顺序、内存屏障、所有权转移、回滚保证和硬件 ABI 必须留下足够说明。
- 修改旧代码时不必为了统一语言而无关地重写整段注释。

### 日志

- 使用当前子系统已有日志接口，不直接引入另一套输出方式。
- 重要初始化里程碑和成功完成的系统阶段使用 `info`。
- 不可恢复的一致性错误使用 `panic`。
- 错误日志应提供足以定位问题的上下文，但避免在多层重复记录同一失败。
- 中断、分配器和锁路径中的日志必须考虑递归、分配和死锁风险。

### Markdown

- 项目说明使用中文，命令、路径、类型名、API 和配置键保持原文。
- 标题按层级组织，不跳级。
- 列表、标题和正文之间保留空行。
- 代码示例使用带语言标记的围栏代码块。
- 相对链接以当前文档目录为基准，并保持可解析。

## 十二、C 代码

- C 函数和变量使用 `snake_case`。
- 文件局部变量或辅助函数使用 `static`。
- 公共 ABI 在头文件中声明，C/C++ 共用头文件应正确处理语言边界。
- 指针排版、4 空格缩进、100 列和花括号风格与 `.clang-format` 一致。
- 使用 `NULL`、显式整数类型和项目属性宏时，跟随所在 C 库的现有接口。
- 字符串和缓冲区接口必须明确容量、NUL 终止和截断语义。
- 不依赖宿主 libc 的完整行为；`mini-cstd` 只实现项目当前需要的子集。

## 十三、Python 构建脚本

Python 代码整体采用接近 PEP 8 的风格：

- 4 空格缩进；
- 函数、变量和模块使用 `snake_case`；
- 类使用 `UpperCamelCase`；
- 模块级常量使用 `UPPER_CASE_STYLE`；
- 内部辅助函数使用单下划线前缀；
- 公共函数和复杂数据结构使用类型标注；
- 文件系统路径优先使用 `pathlib.Path`；
- 模块开头使用简短 docstring 说明职责；
- 导入按标准库、项目模块分组，并使用空行分隔；
- 解析或校验错误通常抛出带上下文的 `ValueError`；
- 生成文件尽量先写临时文件并原子发布，避免失败时破坏旧缓存。

测试使用 `unittest`：

- 测试类以被测主题命名并以 `Tests` 结尾；
- 测试方法使用 `test_*`；
- 参数组合使用 `subTest`；
- 测试应同时覆盖成功输入、边界条件和错误诊断。

## 十四、Make 构建文件

- 普通 Make 变量使用小写连字符，例如 `component-root`、`kernel-path`。
- 用户可覆盖的外部环境变量使用大写下划线，例如 `HOST_CLANG`。
- `?=` 用于可覆盖默认值。
- `:=` 用于需要立即求值的局部或派生值。
- `+=` 用于逐层累加 flags、include 路径、源文件和目标集合。
- 布尔选择使用 `y`/`n`，并通过 `is-*` 或计算变量名选择对应桶。
- 生成注册表和依赖片段沿用 `<owner>-<field>-<selector>` 一类连字符命名。
- recipe 行必须使用 Tab。
- 长表达式使用反斜杠换行，并保持后续行结构清楚。
- 通用组件层只负责共享行为，最终产物的链接或打包语义留在目标专用层。
- `include.mk` 负责声明源文件，普通组件 Makefile 不重复扫描源码目录。

## 十五、TOML 与元数据

- 字段名使用小写连字符，例如 `support-archs`、`support-environments`。
- 重复实体使用数组表，例如 `[[libmeta]]`、`[[dependencies]]`、`[[hostprog]]`。
- 字符串列表使用显式数组，不使用隐式分隔字符串。
- 版本号使用完整 SemVer 2.0；依赖文件中的版本字段使用项目支持的范围表达式。
- 路径相对于所属元数据文件或按对应生成器的约定解析。
- 同类字段集中排列；为了增强可读性，可以对相邻的简单赋值做有限对齐。
- 不为未注册的测试、示例或检查依赖目录扫描，必须通过显式 `testbench.*` 列表登记。

## 十六、测试与验证

- 测试名称应描述可观察行为，而不是内部实现步骤。
- 新接口至少覆盖正常路径、边界值、无效参数和资源失败。
- 容器和分配器应验证容量耗尽、溢出、回滚和生命周期。
- 并发代码应验证发布顺序、重复操作、远端 acknowledgement 和禁止执行上下文。
- 公开头文件应加入独立 header check。
- Freestanding 行为使用已注册的 compile/link check，不能只依赖 Host 测试。
- 代码修改后执行与风险匹配的构建和测试；仅修改 Markdown 时至少执行格式、链接和差异检查。
- 运行 QEMU 或调试目标必须使用 `timeout`。

## 十七、避免的写法

- 不在内核普通失败路径中使用 C++ 异常。
- 不用裸整数替代已有地址、单位、句柄或权限类型。
- 不通过函数内部静态对象隐藏全局初始化顺序。
- 不在硬中断路径中分配或阻塞。
- 不让侵入式容器承担其不拥有对象的销毁责任。
- 不直接编辑生成缓存和构建输出。
- 不为了风格统一而顺带重写无关文件。
- 不把 `.vscode/sustcore/` 的历史接口视为当前实现规范。
- 不忽略 `expected`、分配结果或其他标记为 `[[nodiscard]]` 的返回值。
- 不在注释中声称尚未由代码或测试保证的行为。
