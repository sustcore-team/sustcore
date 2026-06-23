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
all: autotest
autotest: kernel-rv kernel-la
build: build-kernel
run: build + run-only
dbg: build + dbg-only
run-only: 只启动 qemu
dbg-only: 只启动带 -s -S 的 qemu
clean: rm -rf build
```

通常情况下，`make build`、`make run` 和 `make dbg` 是最常用的目标。`make all` 会构建 `kernel-rv` 与 `kernel-la`，而不是运行 QEMU。

### 默认参数

除非从命令行、生成的 `config.mk` 或其他被包含的 Make 片段中覆盖，顶层 `Makefile` 会设置以下默认值：

```make
architecture ?= $(config-arch)
mode-boot ?= bios
filesystem ?= ext4
build-mode ?= release
image-sectors ?= 262144
```

`architecture` 是最重要的开关。它默认来自所选 JSON 配置中的 `arch` 字段，并且可以通过命令行覆盖，例如 `make build architecture=loongarch64`。

## 配置生成

构建过程在编译期间并不直接读取 `config.json`。相反，它会首先生成 Make/头文件工件。

### 输入

配置按以下顺序从最先存在的文件中选取：

1.  `config.json`
2.  `script/config.default.json`

`script/config.default.json` 默认存放的是评测机环境对应的配置文件；本地自己的工具链需要自定义 `config.json`。如果没有 `config.json`，则自动使用默认配置。需要注意这里的逻辑和 Makefile 不同，不是“以 `script/config.default.json` 为基底，再用 `config.json` 覆盖”，而是“仅选择其中一个文件”。一旦仓库根目录存在 `config.json`，默认配置文件中的架构块、功能列表、日志配置和 QEMU 参数都不会自动继承。

因此，本地 `config.json` 必须包含当前要使用的架构块。例如命令行指定 `architecture=loongarch64` 时，生成器会读取 `loongarch64` 块；如果该块不存在，配置生成会失败。

### 架构覆盖关系

`arch` 字段决定 Makefile 围绕的架构，对于本地开发，决定了你 `make run` 跑起来的架构，也就是端到端构建和运行时使用的架构。

```json
{
    "arch": "riscv64",
    "riscv64": {},
    "loongarch64": {}
}
```

命令行传入的 `architecture` 优先级更高，这也是评测时用的 `make all` 的基础，会对所有架构构建。类似于 `arch` 设置成各个架构并分别执行 `make build`。

```sh
make build architecture=loongarch64
```

顶层 `Makefile` 会把这个命令行架构作为生成器的最后一个参数传入 `config_gen.py`、`logger_gen.py` 和 `feature_gen.py`。也就是说，命令行覆盖不只是影响 Make 变量，还会影响 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk` 读取哪个架构块。

因为生成器写出的 Make 变量大多使用 `?=`，命令行变量仍然可以覆盖 JSON 生成值。例如 `make build build-mode=debug` 会覆盖 JSON 中的 `"mode": "release"`。

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

*   `tools/config_gen/config_gen.py` 将 JSON 转换为 Make 变量，例如 `build-mode`、`<arch>-compiler-prefix`、`config-additional-flags-*`、`qemu-memory-args`、`qemu-attached-args` 和 `features`。
*   `tools/logger_gen/logger_gen.py` 读取 `kernel/logger.json` 以及配置覆盖项，并生成包含 `DECLARE_LOGGER(...)` 条目的 `kernel/logger.h`。
*   `tools/feature_gen/feature_gen.py` 将配置的功能名称映射到 `-D...=1` 宏，并将其写入 `kernel/feature.mk` 作为 `kernel-feature-defs`。

这样就将运行时配置、内核功能选择和日志记录器策略从手写的 Makefile 中分离了出来。

### JSON 字段

当前配置文件的顶层字段和架构块字段如下：

```json
{
    "mode": "release",
    "arch": "riscv64",
    "riscv64": {
        "compiler-prefix": "riscv64-unknown-elf-",
        "additional-flags": {},
        "qemu": {},
        "features": [],
        "logger": {},
        "linuxss": {
            "logger": {},
            "logger-disable-all": true
        },
        "logger-disable-all": true
    }
}
```

`mode` 会生成 `build-mode ?= ...`。如果省略，则由 Makefile 默认到 `release`。常用值是 `debug` 和 `release`，实际含义由 `flags.mk` 中的 `ifeq ($(build-mode), debug)` 决定。

`arch` 是默认架构名。该架构名必须能在同一个 JSON 文件中找到同名对象，例如 `"arch": "riscv64"` 需要存在 `"riscv64": { ... }`。

`compiler-prefix` 会生成 `<arch>-compiler-prefix ?= ...`，用于覆盖 `script/build/arch/*.mk` 中的默认工具链前缀。例如：

```json
"loongarch64": {
    "compiler-prefix": "loongarch64-linux-gnu-"
}
```

生成：

```make
loongarch64-compiler-prefix ?= loongarch64-linux-gnu-
```

`additional-flags` 是按架构追加的临时 flag 通道，支持 `c`、`cpp`、`asm` 和 `ld` 四个数组，可用于临时设置工具链的特殊编译选项：

```json
"loongarch64": {
    "additional-flags": {
        "c": ["-mno-lsx"],
        "cpp": ["-mno-lsx"],
        "asm": [],
        "ld": []
    }
}
```

`tools/config_gen/config_gen.py` 会把非空列表生成为 `config-additional-flags-c`、`config-additional-flags-cpp`、`config-additional-flags-asm` 和 `config-additional-flags-ld`。

这些变量随后由 `flags.mk` 追加到公共 C/C++/汇编/链接 flags。这里适合放工具链 workaround、实验选项或本地环境选项，例如某些 LoongArch64 工具链需要的 `-mno-lsx`。架构必须依赖的 ABI、代码模型和位宽 flag 应继续放在 `script/build/arch/*.mk` 或 `flags.mk` 中。

汇编 flag 有一个容易踩坑的细节：`script/build/asm.mk` 会把 `flags-asm` 中的每个条目转换为 `-Wa,<flag>` 再传给 `gcc -x assembler-with-cpp`。因此 `additional-flags.asm` 中通常应写 assembler 选项本身，而不是已经带 `-Wa,` 的完整 GCC 选项。

`qemu.memory` 会生成 `qemu-memory-args ?= -m size=...,maxmem=...`。`size` 和 `maxmem` 必须同时出现，只写其中一个会报错。

`qemu.attached` 是额外追加到 QEMU 命令行尾部的参数数组，会生成 `qemu-attached-args ?= ...`。数组元素会用空格拼接；如果某个参数本身包含空格，需要在单个字符串中写好传给 shell 的形式，例如 `"-d trace:int"`。

`features` 是内核编译期功能列表。每个名字必须存在于 `kernel/feature.json`，否则 `feature_gen.py` 会报错。生成结果写入 `kernel/feature.mk`，最终进入内核 C++ 编译参数，例如 `-D__CONF_KERNEL_TESTS=1`。

`logger` 是按 logger 名称覆盖日志等级的对象。logger 名称必须存在于 `kernel/logger.json`，等级支持 `debug`、`info`、`warn`、`error`、`fatal` 和 `disable`。未知 logger 名称或未知等级都会让 `logger_gen.py` 失败。

`logger-disable-all` 只有严格等于 JSON 布尔值 `true` 时才生效。它的优先级高于 `logger` 中的单项覆盖，会把所有 logger 都生成成 `LogLevel::DISABLE`。省略该字段、写成 `false`，或误写成字符串 `"true"` 都不会触发全局禁用。

`linuxss.logger` 和 `linuxss.logger-disable-all` 作用于 `module/linux-subsystem/logger.h`。它们的语义与内核 logger 配置一致，但 logger 名称集合来自 `module/linux-subsystem/logger.json`，当前包含 `lxsc` 和 `lxrt`。

### 生成文件的注意事项

`config.mk`、`kernel/logger.h`、`module/linux-subsystem/logger.h` 和 `kernel/feature.mk` 都是生成文件，不应该手写维护。相关 Make 规则带有 `FORCE`，所以每次构建入口都会尝试重新生成；生成器内部会在内容未变化时避免重写文件。

`config.mk` 被顶层 `Makefile` 和组件构建模板包含，用于把 JSON 配置转成 Make 变量。直接修改 `config.mk` 很容易在下一次构建时被覆盖。

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
kernel/boot/sbi/sbi.ld          # riscv64
kernel/boot/laboot/laboot.ld    # loongarch64
```

这些脚本由 `kernel/options.mk` 的 `variant.<arch>.script-ld` 选择。它们负责描述引导段、内核虚拟地址布局、`.text` / `.rodata` / `.data` / `.bss` 等节，并在 `.rodata` 中预留 initrd 附件位置。

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

### 架构和工具链 | `arch.mk`

`script/build/arch.mk` 分发到以下文件之一：

*   `script/build/arch/riscv64.mk`
*   `script/build/arch/loongarch64.mk`
*   `script/build/arch/x86.mk`
*   `script/build/arch/x86_64.mk`

架构片段内置的默认前缀包括：

*   `riscv64` -> 前缀 `riscv64-unknown-elf-`
*   `loongarch64` -> 前缀 `loongarch64-unknown-elf-`

这些值由 `script/build/arch/*.mk` 使用 `?=` 提供。JSON 中的 `compiler-prefix` 会生成同名 Make 变量，因此可以覆盖架构片段内置默认值，例如评测机默认需要：

```json
{
    "arch": "loongarch64",
    "loongarch64": {
        "compiler-prefix": "loongarch64-linux-gnu-"
    }
}
```

`tools/config_gen/config_gen.py` 会将其生成为：

```make
loongarch64-compiler-prefix ?= loongarch64-linux-gnu-
```

如果命令行也传入了同名 Make 变量，例如：

```sh
make build architecture=loongarch64 loongarch64-compiler-prefix=loongarch64-linux-gnu-
```

命令行值的优先级最高。

### 标志 | `flags.mk`

`flags.mk` 集中了基础的独立环境配置：

*   C 使用 `-std=gnu18`
*   C++ 使用 `-std=gnu++23`
*   两者都使用 `-nostdlib`、`-fno-builtin`、`-ffreestanding`
*   `riscv64` 追加 `-mcmodel=medany`
*   `loongarch64` 追加 `-mcmodel=normal`
*   release 模式使用 `-Ofast` 和 `--strip-all`
*   debug 模式使用 `-O0 -g`

默认情况下，内核和模块的 C++ 构建也会禁用 RTTI 和异常：

```make
flags-no-rtti-cpp := -fno-rtti
flags-no-exceptions-cpp := -fno-exceptions
```

对应的 `__SUS_NO_RTTI__` 和 `__SUS_NO_EXCEPTIONS__` 宏由内核 `options.mk` 中的 `base-feature-defs` 生成。

`script/build/arch/*.mk` 还会在后端追加架构基础 flag，例如 `-DBITS=64`、x86 的 `-m32` 或 x86_64 的 `-mno-red-zone`。这些属于架构构建模型，相对固定，不建议写在 JSON 的 `additional-flags` 字段。

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

对于 `initrd.tar`，`$(basename $(notdir ...))` 的结果是 `initrd`，因此节名会变成 `.attach.initrd`。

### 嵌入

`kernel/boot/sbi/sbi.ld` 和 `kernel/boot/laboot/laboot.ld` 都显式地将：

```ld
*(.attach.initrd)
```

放置在 `s_initrd` 和 `e_initrd` 之间的内核 `.rodata` 区域内。

因此，内核镜像将打包好的 initrd 作为一个正常的链接节包含在内，而非作为附属文件。

## 构建顺序如何工作

有效的构建顺序是：

1.  生成 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk`。
2.  在 `make-initrd` 中暂存 initrd 源码树。
3.  在 `build-libs` 中构建所有库。
4.  在 `build-mods` 中构建所有模块，并将模块复制进 initrd 暂存目录。
5.  在 `build-kernel` 中链接内核。

根目录 `Makefile` 用链式先决条件表达这个顺序：`build` 依赖 `build-kernel`，`build-kernel` 依赖 `build-mods`，`build-mods` 依赖 `make-initrd` 和 `build-libs`。这样即使在并行构建或重构构建图时，仍能保证先创建 initrd 暂存树，再将模块复制进去，最后链接内核并嵌入 initrd。

## 运行和调试接口

顶层 `Makefile` 提供日常运行目标，`script/run.mk` 提供 QEMU 参数和跳过构建的底层目标。

### 正常运行

对于 `riscv64`，QEMU 大致按如下方式启动：

```sh
qemu-system-riscv64 \
  -m size=256m,maxmem=256m \
  -bios default \
  -name "Sustcore-rv64" \
  -machine virt \
  -kernel build/<mode>/bin/kernel/sustcore.bin \
  -nographic \
  -rtc base=localtime
```

`run` 会先执行 `build`，再启动当前 `$(path-kernel)` 指向的内核。需要跳过构建、直接启动已有内核时使用 `make run-only`。`config.mk` 可以覆盖内存大小并追加 `qemu-attached-args`。

### 调试运行

`dbg` 会先执行 `build`，再以调试参数启动 QEMU。底层的 `dbg-only` 添加：

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
make run

# 构建并启动一个等待 GDB 连接的 QEMU 实例
make dbg

# 同时构建 riscv64 和 loongarch64 的 kernel-rv/kernel-la 产物
make all

# 覆盖架构或模式
make build architecture=loongarch64
make build build-mode=debug
```

对于配置驱动的构建，将覆盖项放入 `config.json` 中，然后让生成器自动刷新 `config.mk`、`kernel/logger.h` 和 `kernel/feature.mk`。
