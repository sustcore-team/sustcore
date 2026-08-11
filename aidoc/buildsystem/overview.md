# 构建系统概览

## 目标

当前构建系统是由 Make 驱动的管线，使用 TOML 配置、生成的缓存片段、组件专用 Makefile，以及用于编译和链接的轻量规则文件。

系统有意按层重建，而不是恢复旧的单体 `.vscode/sustcore` 构建前端。

## 主要层次

### `script/env`

定义共享环境：

- 工作区路径；
- 缓存路径；
- shell 辅助变量；
- 所选 `arch` / `mode`；
- 显式的 `freestanding` / `host` 构建环境。

关键文件：

- `script/env/global.mk`
- `script/env/buildpath.mk`
- `script/env/host-buildpath.mk`
- `script/env/shell.mk`
- `script/env/q.mk`

### `script/toolchain`

定义编译器、链接器、归档器以及面向 QEMU 的工具变量：

- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`
- `qemu.mk`

C/C++、链接和归档片段由 Host 与 freestanding 构建共享；验证后的 Host 值来自 `script/.cache/host.mk`。

### `script/rules`

只定义轻量构建规则：

- `asm.mk`
- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`

这些文件只消费已经解析的变量，不决定目标类型、源文件发现方式或架构选择。

`ld.mk` 和 `ar.mk` 分别通过 `ld-target ?= $(target)` 与 `ar-target ?= $(target)` 选择工具实际写入的产物。默认值保持普通链接和归档行为；需要转换格式的组件可以把工具输出指向中间文件，再单独生成最终 `target`。Usrboot 使用该机制先链接 `usrboot.elf`，随后由 Host 工具 `mk-usrboot` 生成最终 `usrboot`。

### `script/build`

定义共享组件层：

- `collector.mk` 发现组件 `include.mk` 声明的源文件。
- `component.mk` 选择构建环境、加载生成的上下文和依赖、规范化源文件与对象路径，并安装编译规则。
- `static-library.mk` 在 `component.mk` 之上增加归档工具链和静态库规则。

`component.mk` 有意止于对象文件生成。内核镜像、模块、Host 程序和静态库分别保留独立的最终产物层。

### `script/py`

Python 实现按职责分组：

- `commands/`：Make 调用的命令实现和流程协调；
- `config_emitters/`：把 `config/<name>/*.toml` 转换为 Make 配置；
- `generators/`：生成库、程序、testbench、组件上下文和 initrd 规则；
- `metadata/`：元数据记录类型、扫描和字段校验；
- `dependencies/`：依赖解析和 SemVer 范围匹配；
- `make_support/`：Make 值转义、公共文件头和多行赋值；
- `common/`：命令行参数、构建维度、仓库路径和原子文件发布。

Make 仍通过 `script/py/configure.py` 等稳定路径启动命令。这些顶层文件仅为兼容入口，实际实现位于上述 package 中；内部代码不得依赖顶层兼容入口互相导入。

### 目标局部 Makefile

例如：

- `kernel/Makefile`
- `libs/sbi/Makefile`
- `libs/mincstd/Makefile`
- `third_party/libs/libfdt/Makefile`

静态库 Makefile 现在只需标识组件根目录并包含 `script/build/static-library.mk`。内核、模块和 Host 程序层包含 `component.mk`，再加入各自的链接或打包语义。

## 高层流程

1. `make switch` 保存 `arch/mode`。
2. `make configure` 根据 TOML 和项目元数据生成缓存片段，以及所有 freestanding 架构的依赖。
3. 顶层 Make 读取共享缓存片段。
4. `build-hosttool` 验证 Host 工具链并构建全部目标侧构建工具。
5. `build-libs` 为当前架构构建可见的静态库；所有目标侧 `build-*` 入口均依赖 `build-hosttool`。
6. `build-kernel` 调用 `kernel/Makefile`。
7. `build-host-libs`、`build-host-tools`、`host-test` 和 `bench` 使用验证后的本机工具链。
8. `update-host` 捕获本机库、构建工具和 testbench 的编译命令。
9. `runonly` / `dbgonly` 启动 QEMU。

Host 基础设施有意与目标构建流程分离。`make validate-host [host-arch=<arch>]` 验证配置的本机 Clang 工具链并生成 `script/.cache/host.mk`；它不会读取或更新 `make switch` 选择的架构。

`make update [arch=<arch>] [mode=<mode>]` 通过 Bear 重新生成所选变体的 compilation database。命令行中的架构和模式覆盖值只选择要更新的数据库，不会改变 `make switch` 持久化的值。`make update-host` 生成对应的本机数据库，但不运行测试或基准程序。`clangd-host` 和 `clangd-target` 用于在这些环境之间切换稳定副本 `build/compile_commands.json`。

## 当前架构

当前已知架构为：

- `riscv64`
- `loongarch64`

库可通过 `support-archs` 按架构控制可见性。使用 `make switch` 改变 `arch` 或 `mode` 只会从已经生成的缓存中选择，不会重新运行依赖解析。

Host 构建使用三个互相独立的维度：

- `environment=host`；
- 由本机编译器探测并与 `uname` 核对的 `arch`；
- `mode=debug|release`。

其输出根目录为 `build/<mode>/host/<host-triple>/`。Sanitizer 构建会增加独立的 `sanitize/<profile>/` 子树。Host 库、测试、头文件检查和基准测试分别使用独立的静态库、对象、测试和基准输出目录。
