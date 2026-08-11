# Sustcore 智能体开发指南

本文档规定在 Sustcore 仓库中工作的智能体必须遵守的约束，并记录当前构建系统的真实状态。仓库正处于构建系统和内核目录重构期；任何判断都应以当前源码、生成脚本和本文档为准。

## 工作原则

- 当前生效的是由 `script/` 驱动的 Make/TOML 构建管线，不是 `.vscode/sustcore/` 中的旧实现。旧目录只能作为历史和设计参考，不能用于判断当前编译、链接或运行行为。
- 修改前先阅读目标目录中的 `AGENTS.md`、根目录 `style.md`、相邻源码和相关 `aidoc/` 文档。更深层目录的 `AGENTS.md` 可补充或收紧本文件规则。
- `style.md` 汇总当前全项目的详细代码风格和实现惯例。以现有代码为基准，优先遵守 `.clang-format`、`.editorconfig`、`style.md` 和相邻文件风格，不引入局部的新风格。
- 若 `style.md` 与本文件或更深层目录的 `AGENTS.md` 冲突，以适用范围更具体的 `AGENTS.md` 为准；若与格式化配置冲突，以 `.clang-format` 和 `.editorconfig` 为准。
- 注释应解释意图、前置条件、不变量和设计取舍，避免逐句复述代码。新增自然语言说明优先使用中文，并保留必要的英文技术术语和标识符。
- 不要手工修改 `script/.cache/` 或构建目录中的生成文件；应修改其配置、元数据或生成器。
- 仓库可能包含其他人的未提交修改。只处理当前任务涉及的文件，不覆盖或回退无关改动。
- 代码修改后应执行与风险相称的构建和测试。仅修改 Markdown 文档时可不构建，但仍需检查格式、链接和差异。
- 运行 QEMU 或等待型调试命令时必须使用 `timeout`，避免进程长期占用终端。
- 不要畏惧于大规模重构代码. 当示意你进行代码重构时, 不要担心修改了大量行数, 也不要过于保守, 保留太多旧的代码的结构与实现细节, 而是确保重构后的代码更加简洁, 风格统一, 易于理解.

## 代码风格与接口约定

- 编写或评审代码前应阅读 `style.md` 中与目标语言和子系统有关的章节；本节只保留必须优先遵守的核心约束。
- 内核领域类型、concept、traits、静态策略、适配器和普通类型别名使用 `UpperCamelCase`。跨子系统复用的同步、guard 和所有权工具类型使用 `c_style_case`（全小写并以下划线分词），例如 `interrupt_guard`、`synchronized`、`lock_guard` 和 `locked_ref`；直接包装 `taycpplib` 容器的局部实现别名也使用 `c_style_case`，例如 `area_vector` 和 `chunk_list`。函数、变量和命名空间使用 `snake_case`；私有成员通常使用 `member_`；宏、配置开关和枚举值使用 `UPPER_CASE_STYLE`。
- 头文件保护使用 `#pragma once`。包含顺序、花括号、缩进和指针排版遵循当前目录及 `.clang-format`。
- 禁止使用 C++ 异常。可恢复失败优先使用 `tay::expected<T, E>`、`tay::unexpected` 和 `tay::error_code`，并利用 `and_then`、`transform`、`or_else`、`transform_error` 等接口清晰传播错误。
- 对不应忽略的结果使用 `[[nodiscard]]`；仅在语义准确时使用 `noexcept`、`constexpr`、`constinit` 和 `static_assert`。
- 资源持有者应使用 RAII，并明确拥有、借用和回滚关系。优先复用 `tay::guard`、`tay::owner`、`tay::unique_ptr`、侵入式容器和现有分配器设施。
- 复杂函数应拆分为职责单一的辅助函数；仅供当前翻译单元使用的函数和类型放入匿名命名空间。
- 不检查边界的快速接口要求调用者满足前置条件；可恢复的越界、容量不足或分配失败应沿用所在模块既有错误语义。
- 普通静态全局对象可以直接定义，但必须遵守其启动阶段和常量初始化约束。内核子树的具体规则见 `kernel/AGENTS.md` 与 `aidoc/kernel/early_setup.md`。

## 数据结构选择

- 当实现需要特殊数据结构时，必须首先检查 `taycpplib` 的公开接口、文档和现有实现，确认是否已经提供满足要求的容器、侵入式结构、所有权包装、固定容量结构或算法。
- 如果 `taycpplib` 已有合适的数据结构，应优先直接复用，并按照其所有权、分配、容量、迭代器失效和错误处理语义设计调用方；不得在业务子系统中重复实现同类结构。
- 如果 `taycpplib` 没有所需的数据结构，必须停止继续编写相关实现，不得临时改用自制容器、未经审核的标准库结构或第三方替代品来绕过缺口。
- 停止后应明确告知用户：缺少的数据结构是什么、必须具备哪些操作与复杂度、所有权和分配约束是什么、现有 `taycpplib` 类型为何不足，以及建议是否将该结构加入 `taycpplib`。
- 在制定相关计划时，必须在计划中显式写明缺少的数据结构及其必要能力，并将补充该结构或取得用户决策列为后续实现的前置条件；不得把该缺口隐藏在普通实现步骤中。
- 只有在用户了解该缺口并明确决定处理方式后，才能继续实现依赖该数据结构的代码。

## 当前构建入口

- `make init`
  - 创建所需缓存目录并准备 Python 辅助脚本。
- `make switch arch=<arch> mode=<mode>`
  - 更新 `script/.cache/.switch.mk`。
  - 仅持久化所选架构和模式，不重新生成依赖。
- `make configure config=<name>`
  - 读取 `config/<name>/*.toml`。
  - 生成配置、注册表、组件上下文和所有 freestanding 架构的依赖片段。
  - 旧式 `arch=` 和 `mode=` 参数会被忽略并给出警告。
- `make validate-host [host-arch=<arch>]`
  - 验证本机 Clang、Clang++ 和 LLVM ar 工具链。
  - 生成 `script/.cache/host.mk`，且不会修改 `.switch.mk`。
- `make build-libs`
  - 为当前架构构建可见的库。
- `make build-kernel`
  - 通过 `kernel/Makefile` 构建内核。
- `make build-hosttool`
  - 聚合构建全部 `build-hosttool-<id>`，并作为 freestanding 库、模块、initrd 和内核 `build-*` 入口的公共前置阶段。
- `make build-host-libs` / `make build-host-lib lib=<id>`
  - 构建经过验证的本机静态库，或执行纯头文件库检查。
- `make build-host-tools` / `make build-host-tool tool=<id>` / `make run-host-tool tool=<id>`
  - 构建全部或指定的本机构建工具，或构建后直接运行指定工具。
- `make host-test [lib=<id>]` / `make host-bench [lib=<id>]`
  - 构建并运行已注册的本机测试或基准测试。
- `make example [lib=<id>]` / `make host-example [lib=<id>]`
  - 构建示例，或按顺序构建并运行本机示例。
- `make bench`
  - 以 release 模式构建全部已注册本机基准测试，但不运行。
- `make check-lib lib=<id>` / `make build-lib-matrix lib=<id>`
  - 检查当前变体，或检查库声明支持的全部 freestanding/host 变体。
- `make freestanding-check [lib=<id>]`
  - 为当前目标架构执行已注册的编译/链接契约检查，但不运行跨架构产物。
- `make update [arch=<arch>] [mode=<mode>]`
  - 通过 Bear 更新目标 compilation database，不改变 `make switch` 持久化的选择。
- `make update-host [mode=<mode>] [sanitize=<set>]`
  - 验证本机工具链并捕获 Host 库、构建工具及 testbench 的编译命令，不运行程序。
- `make clangd-host` / `make clangd-target`
  - 在已经生成的 Host 与 freestanding 数据库之间切换稳定的 clangd 副本。
- `make runonly`
  - 不重建内核，直接启动 QEMU。
- `make dbgonly`
  - 不重建内核，以 `-s -S` 启动 QEMU。
- `make clean` / `make cleandist`
  - `clean` 删除当前配置的构建输出；`cleandist` 还删除生成缓存，但保留 `.switch.mk`。

使用 `runonly`、`dbgonly` 或其他可能持续运行的目标时，应采用类似 `timeout 15s make runonly` 的形式。GDB 可通过 `target remote :1234` 连接调试桩。

## 当前缓存模型

`script/.cache/` 下的生成 Make 片段分为两类。

### 构建系统配置

通过 `script/.cache/config.mk` 加载：

- `clang.mk`
- `path.mk`
- `qemu.mk`
- `kernel.mk`

这些文件描述编译器选择、输出目录布局、QEMU 设置和内核启动模式。

### 项目配置

由顶层或目标局部 Makefile 显式加载：

- `libraries.mk`
- `build-libs.mk`
- `programs.mk`
- `host-tools.mk`
- `testbench.mk`
- `deps/*.mk`
- `ctx/*.mk`

这些文件描述全局库注册表、生成的库构建目标、Host 工具、程序与测试索引、组件上下文以及解析后的依赖。

注册表中的 `library-ids-all` 只用于跨环境校验和矩阵枚举。活动库列表、架构专用 CRT/链接字段、构建目标、host 程序和头文件检查通过 `is-host`、`is-freestanding`、`is-<arch>` 及组合选择器写入相应的 `*-y` 桶后再消费。

Freestanding 依赖片段包含所有已知目标架构，并通过 `is-<arch>` 选择当前值；执行 `make switch` 不会重新生成这些文件。

## 当前库系统

库通过 `metadata.toml` 注册，当前结构为：

```toml
[[libmeta]]
id = "example"
libname = "libexample.a"
makefile = "Makefile"
target = "build-static"
version = "0.1.0-dev.1"
support-archs = ["riscv64"]
testbench.test = ["testbench/test/metadata.toml"]
testbench.headercheck = ["testbench/headercheck/metadata.toml"]
testbench.bench = ["testbench/bench/metadata.toml"]
testbench.freestanding = ["testbench/freestanding/metadata.toml"]
testbench.example = ["testbench/example/metadata.toml"]

include-c = ["include"]
include-cpp = ["include"]
include-asm = ["include"]
```

当前规则：

- 一个 `metadata.toml` 可以包含多个 `[[libmeta]]`。
- `id` 在全局范围内必须唯一。
- `libname` 是生成的静态库文件名；空字符串表示该变体为纯头文件库。
- `support-archs` 是允许列表。
- 测试、头文件检查、基准测试、freestanding 检查和示例只从显式的 `testbench.*` 列表读取；所有列表均可省略，省略等价于未注册该类别。
- `build-libs` 由库元数据生成，并跳过纯头文件变体。

## 当前依赖系统

内核使用 `kernel/dependencies.toml`，结构如下：

```toml
[[dependencies]]
lib = "mini-cstd"

[[riscv64.dependencies]]
lib = "sbi"
```

依赖解析器当前能够：

- 将库版本校验为完整的 SemVer 2.0 版本；
- 处理精确、通配和不完整版本表达式；
- 处理比较器、范围交集和逻辑 OR；
- 处理 caret、tilde 和 hyphen 范围；
- 遵循 npm 风格的预发布版本范围匹配；
- 比较版本时忽略构建元数据。

当前不支持同一 `id` 下存在多个版本。

版本字段约定：

- `metadata.toml` 使用具体发布版本，例如 `0.1.0`、`0.2.0-rc.1` 或 `0.2.0-dev.3+git.abc1234`。
- `dependencies.toml` 使用版本范围；`*` 接受任意版本，`^0.1.0`、`~0.1` 和 `1.2 - 2.0` 表示兼容范围。
- 构建元数据仅用于追踪，不能把依赖固定到某个特定构建。

## 当前内核构建状态

内核 Makefile 栈拆分为：

- `kernel/Makefile`
- `kernel/flags.mk`
- `kernel/collect.mk`
- `kernel/include.mk`
- `kernel/enable.mk`
- `kernel/variant.riscv64.mk`
- `kernel/variant.loongarch64.mk`
- `kernel/dependencies.toml`

当前状态：

- 对象编译由 `script/build/component.mk` 和 `script/rules/*.mk` 统一处理。
- 静态库在组件层之上叠加 `script/build/static-library.mk`，并使用 `llvm-ar`。
- 内核链接由 `deps/kernel.mk` 解析出的库。
- `kernel-path` 由顶层 Makefile 控制并传给内核子 Make。

## Host 构建系统

- Host 配置位于 `clang.toml` 的 `[host]` 段。
- `host.sysroot` 是必填项，并用于每个编译和链接探测。
- 只接受本机 Clang、Clang++ 和 LLVM ar 配置。
- Host 架构来自编译器 triple 和 `uname`，绝不取自 `make switch` 缓存的目标架构。
- 验证后的 Host 输出位于 `build/<mode>/host/<host-triple>/`。
- Host 依赖在工具链验证后，从公共段、环境段和本机架构段解析。
- `host-tool/<name>/metadata.toml` 注册本机构建工具，输出位于 Host triple 下的 `host-tool/` 目录。
- Sanitizer 配置使用 host triple 下相互隔离的子目录。
- C++ 编译根据工具链环境只会收到 `TAY_ENV_HOST=1` 或 `TAY_ENV_FREESTANDING=1` 之一。

## 当前限制

- `build-libs` 依赖二次展开；修改 include 链或架构切换逻辑后必须重点验证。
- 当前内核树相较历史仓库仍不完整。
- C/C++ 运行时拆分仍在演进。
- 同一库 `id` 下的多版本尚不受支持。
- host 库、构建工具、测试、示例、头文件检查、基准测试、sanitizer 和库矩阵应通过各自专用目标操作。

## 文档索引

- 全项目代码风格与实现惯例：`style.md`
- 当前构建系统：`aidoc/buildsystem/`
- 内核早期初始化与并发契约：`aidoc/kernel/early_setup.md`
- Tay 基础库：`aidoc/taycpp/`
- 内核子树代理约束：`kernel/AGENTS.md`
- 历史实现与设计参考：`.vscode/sustcore/`

处理具体组件时，应继续阅读其头文件、源文件、元数据和相邻文档，不能只依赖本文件的概述。
