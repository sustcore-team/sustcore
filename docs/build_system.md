# 构建系统

本文档描述项目的构建系统：顶层入口、如何生成配置、组件如何声明自身以及最终内核镜像如何生成。

## 构建系统需要完成的工作

要生成一个可启动的 Sustcore 内核，构建系统必须完成五项任务：

1.  **加载并规范化构建配置** —— 选择架构、构建模式、文件系统和 QEMU 设置，然后将其暴露为 Make 变量。
2.  **生成派生的构建输入** —— 在任何实际编译开始之前，从 JSON 源文件生成 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk`。
3.  **构建共享库和模块** —— 将所有组件源码编译为特定架构的静态库和可加载模块。
4.  **组装 initrd 有效载荷** —— 将运行时所需的源码树子集复制到 `build/.../bin/initrd/src`，然后将其打包为附件。
5.  **链接最终内核并运行** —— 将内核与其库和嵌入的附件链接，然后使用所选的架构设置启动 QEMU。

该系统有意设计为端到端由 Make 驱动。每个组件的 `Makefile` 仅仅是薄薄的入口点；几乎所有实际逻辑都位于 `script/` 目录下。

## 构建入口点

仓库根目录下的 `Makefile` 是唯一的公开入口点。最重要的目标有：

```make
all: build + run
build: make-initrd build-mods build-kernel
dbg: build + run_dbg
run: 启动 qemu
run_dbg: 使用 -s -S 启动 qemu
clean: rm -rf build
```

通常情况下，只有 `make build`、`make run`、`make all` 和 `make dbg` 是有意义的目标。

### 默认参数

除非从命令行或 `config.mk` 中覆盖，顶层 `Makefile` 会设置以下默认值：

```make
architecture ?= riscv64
mode-boot ?= bios
filesystem ?= ext4
build-mode ?= release
image-sectors ?= 262144
```

`architecture` 是最重要的开关。README 文档将 `riscv64-unknown-elf` 和 `loongarch64-unknown-elf` 列为支持的交叉工具链。

## 配置生成

构建过程在编译期间并不直接读取 `config.json`。相反，它会首先生成 Make/头文件工件。

### 输入

配置按以下顺序从最先存在的文件中选取：

1.  `config.json`
2.  `script/config.default.json`

如果没有 `config.json`，则自动使用默认配置。

### 生成的文件

三个生成的文件是每个主要构建目标的先决条件：

```make
config.mk
kernel/logger.h
kernel/feature.mk
```

它们由以下规则生成：

```make
config.mk: $(config-json) tools/config_gen/config_gen.py
kernel/logger.h: $(config-json) kernel/logger.json tools/logger_gen/logger_gen.py
kernel/feature.mk: $(config-json) kernel/feature.json tools/feature_gen/feature_gen.py
```

### 每个生成器输出什么

*   `tools/config_gen/config_gen.py` 将 JSON 转换为 Make 变量，例如 `build-mode`、`<arch>-compiler-prefix`、`qemu-memory-args`、`qemu-attached-args` 和 `features`。
*   `tools/logger_gen/logger_gen.py` 读取 `kernel/logger.json` 以及配置覆盖项，并生成包含 `DECLARE_LOGGER(...)` 条目的 `kernel/logger.h`。
*   `tools/feature_gen/feature_gen.py` 将配置的功能名称映射到 `-D...=1` 宏，并将其写入 `kernel/feature.mk` 作为 `kernel-feature-defs`。

这样就将运行时配置、内核功能选择和日志记录器策略从手写的 Makefile 中分离了出来。

## 目录和工件布局

`script/env/global.mk` 定义了核心路径布局：

```make
path-build := $(path-e)/build/$(build-mode)
path-bin := $(path-build)/bin
path-objects := $(path-build)/objects
path-attach := $(path-bin)/attachment
path-initrd := $(path-bin)/initrd
path-kernel := $(path-bin)/kernel/sustcore.bin
```

因此，所有构建输出都位于 `build/<mode>/` 目录下。

### 重要的输出位置

| 工件 | 路径 |
|---|---|
| 静态库 | `build/<mode>/bin/libs/<arch>/` |
| 模块 | `build/<mode>/bin/mods/<arch>/` |
| 内核 | `build/<mode>/bin/kernel/sustcore.bin` |
| 目标文件 | `build/<mode>/objects/...` |
| Initrd 暂存树 | `build/<mode>/bin/initrd/src/` |
| 二进制附件 | `build/<mode>/bin/attachment/` |

构建在库/模块级别按架构分离，并在顶层 `build/<mode>` 目录按模式分离。

## 分层边界

构建系统分为四层。

*   **根目录 `Makefile`** —— 编排整个构建过程，定义公开目标，并对库、模块、initrd 和内核的构建进行排序。
*   **`script/env/*.mk`、`script/tool.mk`、`script/run.mk`** —— 定义全局路径、宿主工具、QEMU 启动参数和便利帮助函数。
*   **`script/build/*.mk`** —— 实现每个组件使用的通用编译/链接/归档规则。
*   **组件本地的 `options.mk` 和 `include.mk` 文件** —— 声明一个组件是什么、它拥有哪些源文件、链接哪些库以及输出到哪里。

像 `kernel/Makefile` 或 `module/default/Makefile` 这样的组件 `Makefile` 仅包含共享组件模板：

```make
include $(path-script)/build/component.mk
```

该模板是真正的组件前端。

## 组件声明如何工作

每个可构建单元都由一个 `options.mk` 文件以及一个或多个 `include.mk` 文件来描述。

### `options.mk`

`options.mk` 声明组件元数据。关键字段有：

```make
component-kind := static-library | module | kernel
component-name := ...
component-variants := ...
component-target := ...
component-objdir := ...
module-output := ...
module-libraries := ...
variant.<name>.target := ...
variant.<name>.dir-obj := ...
variant.<name>.script-ld := ...
variant.<name>.libraries := ...
```

示例：

*   `libs/basecpp/options.mk` 声明一个带有 `default` 和 `kernel` 变体的 `static-library`。
*   `module/default/options.mk` 声明一个输出 `default.mod` 并链接 `basecpp kmod` 的 `module`。
*   `kernel/options.mk` 声明一个 `kernel`，链接 `kersbi kerbasecpp fdt`，并请求一个名为 `initrd.tar.attachment.o` 的附件。

### `include.mk`

`include.mk` 文件仅贡献源文件列表：

```make
sources += main.cpp
sources += baseio.cpp tostring.c string.c ...
```

`script/build/component.mk` 递归地查找组件根目录下的每一个 `include.mk`，对它们求值，并自动添加相对路径前缀。像 `kernel/` 这样的大型组件正是通过这种方式来维护每个子目录的源文件声明，而无需手动维护一个庞大的列表。

## 支持的组件类型

`script/build/component.mk` 识别三种活动组件类型。

### 静态库

静态库使用 `mode=s` 构建，然后使用 `ar` 归档。

示例：

*   `libs/sbi` -> `libkersbi.a`
*   `libs/basecpp` -> `libbasecpp.a`，以及为内核变体生成的 `libkerbasecpp.a`
*   `libs/kmod` -> `libkmod.a`
*   `libs/rpc` -> `librpc.a`

`libs/kmod` 是特殊的：它被标记为 `library-is-libc := true`，因此其归档文件包含 `crt0.o`、`crti.o`、`crtbegin.o`、`crtend.o` 和 `crtn.o`。后续的模块构建通过 `script/build/libc.mk` 重用这些启动/清理对象。

### 模块

模块使用 `mode=m` 构建，使用 `ld` 链接，然后复制到 initrd 暂存区：

```make
$(copy) $(variant.default.target) $(path-initrd)/$(module-output)
```

这意味着模块既是 `bin/mods/<arch>/` 下的独立工件，也是生成的 initrd 内部的有效载荷。

### 内核

内核使用 `mode=k` 构建，并使用来自以下位置的架构特定链接脚本链接为单个 ELF 镜像：

```make
kernel/arch/$(architecture)/kernel.ld
```

对于 `riscv64`，这将虚拟内核基址置于 `0xffffffff80200000`，并且还预留了一个 `.rodata` 区域用于嵌入的附件。

## 通用编译流水线

`script/build/Makefile.build` 是每种组件类型使用的后端。它包括：

*   `arch.mk` —— 用于选择架构片段
*   `compilers.mk` —— 用于派生 `gcc`、`g++`、`ld` 和 `ar`
*   `asm.mk`、`c.mk`、`cpp.mk` —— 用于编译规则
*   `ld.mk` 和 `ar.mk` —— 用于最终链接
*   `attach.mk` 和 `tar.mk` —— 用于处理二进制附件

### 源代码编译

通用规则很直观：

*   `%.c` -> `%.o` 使用 `$(compiler-c)`
*   `%.cpp` -> `%.o` 使用 `$(compiler-cpp)`
*   `%.S` -> `%.o` 在 `assembler-with-cpp` 模式下使用 `$(compiler-c)`
*   `%.c` / `%.cpp` 还会生成匹配的 `.d` 依赖文件

依赖文件会被自动包含：

```make
include $(foreach dep, $(deps), $(dir-obj)/$(dep))
```

因此，增量式重新构建可以追踪头文件的更改，而无需手写依赖关系。

### 链接和归档

*   内核和模块通过 `ld-link` 使用 `ld`。
*   静态库通过 `ar-link` 使用 `ar`。

库解析始终针对：

```make
-L"$(path-bin)/libs/$(architecture)"
```

并且被包裹在：

```make
--start-group $(libraries-ld) --end-group
```

中以容忍静态库之间的循环引用。

## 编译器和标志模型

工具链带有架构前缀：

```make
compiler-c ?= $(prefix-compiler)gcc
compiler-cpp ?= $(prefix-compiler)g++
compiler-ld ?= $(prefix-compiler)ld
compiler-ar ?= $(prefix-compiler)ar
```

### 架构选择

`script/build/arch.mk` 分发到以下文件之一：

*   `script/build/arch/riscv64.mk`
*   `script/build/arch/loongarch64.mk`
*   `script/build/arch/x86.mk`
*   `script/build/arch/x86_64.mk`

在当前文档记录的工作流中，重要的是：

*   `riscv64` -> 前缀 `riscv64-unknown-elf-`
*   `loongarch64` -> 前缀 `loongarch64-unknown-elf-`

这些默认值由 `script/build/arch/*.mk` 使用 `?=` 提供，也可以在对应架构的 JSON 配置中覆盖：

```json
{
    "arch": "loongarch64",
    "loongarch64": {
        "compiler-prefix": "loongarch64-linux-elf-"
    }
}
```

`tools/config_gen/config_gen.py` 会将其生成为：

```make
loongarch64-compiler-prefix ?= loongarch64-linux-elf-
```

### 共享标志

`flags.mk` 集中了基础的独立环境配置：

*   C 使用 `-std=gnu23`
*   C++ 使用 `-std=gnu++26`
*   两者都使用 `-nostdlib`、`-fno-builtin`、`-ffreestanding`
*   release 模式使用 `-Ofast` 和 `--strip-all`
*   debug 模式使用 `-O0 -g`

默认情况下，内核和模块的 C++ 构建也会禁用 RTTI 和异常：

```make
flags-no-rtti-cpp := -D__SUS_NO_RTTI__ -fno-rtti
flags-no-exceptions-cpp := -D__SUS_NO_EXCEPTIONS__ -fno-exceptions
```

内核还会额外地注入来自 `kernel/feature.mk` 的功能宏。

## Initrd 和附件流程

在正常的 `make build` 过程中，initrd 并非由外部的文件系统镜像构建器生成。相反，它是在常规构建流水线内部组装的。

### 暂存

`make-initrd` 重新创建 `$(path-initrd)/src` 并将以下目录树复制到其中：

*   `include/`
*   `kernel/`
*   `libs/`
*   `module/`
*   `script/`
*   `tools/`

这会在 `build/<mode>/bin/initrd/src/` 下生成一个源码快照。

### 打包

`script/build/tar.mk` 可以将 `$(path-bin)` 下的任何目录转换为 `$(path-attach)` 下的一个压缩包。内核请求一个附件：

```make
attachments := initrd.tar.attachment.o
```

该目标文件由 `script/build/attach.mk` 从 `$(path-attach)` 中的一个二进制文件构建。在 `riscv64` 上，`make-attachment` 使用 `objcopy` 将该压缩包转换为一个 ELF 目标文件，并将其节（section）重命名为：

```make
.attach.<basename>
```

对于 `initrd.tar`，就变成了 `.attach.initrd.tar`。

### 嵌入

`kernel/arch/riscv64/kernel-phy.ld` 显式地将：

```ld
*(.attach.initrd.tar)
```

放置在 `s_initrd` 和 `e_initrd` 之间的内核 `.rodata` 区域内。

因此，内核镜像将打包好的 initrd 作为一个正常的链接节包含在内，而非作为附属文件。

## 构建顺序如何工作

有效的构建顺序是：

1.  生成 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk`。
2.  在 `build-libs` 中构建所有库。
3.  在 `build-mods` 中构建所有模块。
4.  在 `make-initrd` 中暂存 initrd 源码树。
5.  在 `build-kernel` 中链接内核。

当前根目录 `Makefile` 中存在一个需要注意的地方：`build` 将 `make-initrd` 列在 `build-mods` 之前，而 `build-mods` 会将完成的模块复制到 `$(path-initrd)` 中。在实践中，只有当 initrd 目录在模块复制发生之前已经存在，或者由更早的规则隐式创建时，这才能正常工作。预期的数据流显然是“暂存 initrd，然后将模块放入其中，再将其嵌入内核”，但在更改构建图时应谨慎处理目标的顺序。

## 运行和调试接口

`script/run.mk` 提供了公开的运行目标。

### 正常运行

对于 `riscv64`，QEMU 大致按如下方式启动：

```sh
qemu-system-riscv64 \
  -m size=256m,maxmem=256m \
  -bios default \
  -name "Sustcore-rv64" \
  -machine virt \
  -kernel build/<mode>/bin/kernel/sustcore.bin \
  -serial stdio \
  -rtc base=localtime
```

`config.mk` 可以覆盖内存大小并追加 `qemu-attached-args`。

### 调试运行

`run_dbg` 添加：

```sh
-s -S
```

以便 GDB 能够在执行开始前连接到 `localhost:1234`。

## 旧版镜像工作流

仓库中仍包含一个较旧的磁盘镜像路径：

*   `image`
*   `mount`
*   `umount`
*   `setup-workspace`
*   `setup-losetup`

这些使用了回环设备、`mkfs`、`mount`，以及来自 `script/image/` 和 `script/setup.mk` 的可选的、与 GRUB 相关的工具。

README 明确说明在当前工作流中不要使用 `make setup_workspace`，因为正常的启动路径会直接通过 QEMU 的 `-kernel` 加载内核。

## 典型用法

```sh
# 构建默认的 riscv64 release 内核
make build

# 构建并运行
make all

# 构建并启动一个等待 GDB 连接的 QEMU 实例
make dbg

# 覆盖架构或模式
make build architecture=loongarch64
make build build-mode=debug
```

对于配置驱动的构建，将覆盖项放入 `config.json` 中，然后让生成器自动刷新 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk`。
