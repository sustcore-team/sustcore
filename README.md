# Sustcore

Sustcore 是一个面向 RISC-V 与 LoongArch64 的 Capability-based 混合内核, 其灵感来源于著名的 seL4 内核, 但在设计上有着较大的差异.
作为一个混合内核, Sustcore 将 VFS, 内存管理, 进程管理以及部分重要的驱动与文件系统等功能收归内核, 以提高效率, 降低复杂度并增强安全性,
而将网络协议栈, 文件系统, 驱动等功能放在用户态(未实现), 以兼备微内核的安全性与模块化, 以及宏内核的效率与易用性.

Sustcore 是从 0 开始设计与实现的, 其不基于任何现有的内核. 项目主体代码自行实现, 同时通过依赖系统使用 libfdt、ELF、Multiboot 及部分 C/C++ 与 Linux 头文件等第三方组件.
因此, 例如 ext4 文件系统, riscv64与loongarch64架构支持, 自旋锁等功能都是自己实现的.

在本系统的编写过程中, 我们大量参考了以下的操作系统与书记

1. [seL4](https://github.com/seL4/seL4)
    > 本内核的灵感来源, 也是大多数系统调用的设计依托.
2. [Linux](https://github.com/torvalds/linux)
   > 大多数机制的实现与linux兼容的系统调用都大量地参考了linux内核的实现, 以保证兼容性与可移植性.
   > 此外, ext4 文件系统, 页缓存机制, slab 分配器与 buddy 分配器, loongarch 下的 RTC 时钟驱动实现等功能也参考了 linux 内核的实现.
3. [NAOS](https://github.com/aether-os-studio/naos)
    > 一个简单而强悍的宏内核. 例如 ELF 加载器, 辅助向量填充, loongarch 架构支持, virtio 与 pci 驱动以及部分系统调用的实现都参考了这个系统. 此外, 在进行 debug 时, 我们也常常通过与该系统进行对比来定位和解决问题.
5. [Managarm](https://github.com/managarm/managarm)
    > 另一个著名的微内核, 其是使用 C++ 实现的, 在本内核进行 C++ 运行时支持的实现时部分地参考了该内核的实现.
6. [frigg](https://github.com/managarm/frigg)
    > Managarm 的 C++ 库, 其实现了许多 C++ 模板容器与工具, 本项目的 C++ 容器与工具的实现也参考了该库的实现.
7. [Tayhuang OS](https://github.com/TayhuangOS-Development-Team/TayHuangOS)
    > 本人的早期操作系统项目. 其构建系统是本内核构建系统的前身. 也为本内核提供了部分头文件(如 types.h, stdint.h 等)与部分工具(stat.sh, calc_magic, cc_modifier, comments_stat, get_loop_devices)
8. Orange'S: 一个操作系统的实现
   > 操作系统开发与实践的入门级书籍. 是本人实现 Tayhuang OS 时的主要书籍
9.  Operating System: Three Easy Pieces
10.  计算机的心智: 操作系统之哲学原理
    > 提供了重要的理论知识以支撑开发过程中的设计决策
11. [操作系统实验文档](https://yuk1i.github.io/os-next-docs/)
    > 提供了对 riscv64 启动流程的部分介绍以供参考
12. RiscV 官方文档
    > 是进行 riscv64 架构相关的开发时重要的参考资料
    > 实现 plic 驱动, riscv64 架构下的 trap 处理, riscv64 架构下的上下文切换, riscv64 页表实现等主要都是参考了 riscv64 官方文档自己思考并类比 x86_64 架构的实现来完成的
13. LoongArch 官方文档
    > 是进行 loongarch64 架构相关的开发时重要的参考资料
    > 然而, 几乎所有的 loongarch64 架构相关的开发都是参考 naos 实现的, 因为官方文档的内容过于简略, 以至于无法直接使用, 只能通过 naos 的实现来理解 loongarch64 架构的设计理念与实现方式.
    > 希望龙芯官方能够出个好点的文档......
14. [Rust Book](https://doc.rust-lang.org/stable/book/)
    > 虽然本内核是使用 C++ 实现的, 但是 Rust Book 中提及的 Result<T, E> 错误处理机制对本内核的错误处理机制有着重要的启发作用, 也因此在本内核中实现了类似的错误处理机制以代替C++的异常机制, 以提高效率与可控性.
15. [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
    > C++ 的官方规范, 对本内核的 C++ 代码风格有着重要的指导作用, 同时也启发了本内核中 tay::owner, tay::nonnull 等零成本类型标注的设计

此外, 项目曾探索使用 C++ 反射机制优化 RPC 胶水代码的编写, 以提高效率与可维护性. 当前构建系统默认使用 Clang/LLVM，内核构建流程尚未启用依赖反射的功能或测试目标.

本项目的 github 链接是 [sustcore-team/sustcore](https://github.com/sustcore-team/sustcore)
演示视频文件, 文档与ppt见该 github 仓库的 release 页面.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/sustcore-team/sustcore)

# 编译, 运行与调试

## 编译

当前构建系统默认使用 Clang/LLVM：`clang`、`clang++`、`ld.lld`、`llvm-ar` 和 `llvm-objcopy`；运行时还需要相应架构的 QEMU（`qemu-system-riscv64` 或 `qemu-system-loongarch64`）。工具路径和编译选项可在配置集的 `clang.toml` 中覆盖。

首次构建时，依次执行：

```sh
make init
make switch arch=riscv64 mode=debug
make configure config=default
make build-kernel
```

`arch` 可选 `riscv64` 或 `loongarch64`，`mode` 可选 `debug` 或 `release`。`configure` 会一次生成所有 freestanding 架构的依赖缓存；之后只需用 `make switch` 改变持久选择，不需要重新配置。`build-kernel` 会先构建当前架构可见的库，再调用 `kernel/Makefile` 链接内核；initrd 是由 `make build-initrd` 单独生成的独立产物。

为 clangd 更新当前配置的编译数据库：

```sh
make update
```

也可以只更新指定架构和模式的数据库，而不改变 `make switch` 保存的当前选择：

```sh
make update arch=loongarch64 mode=release
```

编译数据库保存在 `build/<mode>/<arch>/compile_commands.json`。`make switch` 与 `make configure` 会将当前选择对应的数据库原子复制到 `build/compile_commands.json`；目标尚未生成时会删除旧副本，避免 clangd 继续使用错误架构的编译参数。VS Code clangd 插件可固定使用 `--compile-commands-dir=build`。

Host 库与 testbench 的编译数据库可独立生成和选择：

```sh
make update-host mode=debug
make clangd-host mode=debug
make clangd-target
```

Host 数据库位于 `build/<mode>/host/<host-triple>/compile_commands.json`；sanitizer profile 会使用对应的隔离子目录。`clangd-host` 与 `clangd-target` 只原子替换稳定入口，不修改 `make switch` 保存的架构和模式。

独立的本机构建工具位于 `host-tool/`，可整体构建、单独构建或直接运行：

```sh
make build-hosttool
make build-host-tools
make build-host-tool tool=hello-world
make run-host-tool tool=hello-world
```

`build-hosttool` 是目标构建的统一 Host 工具前置阶段，聚合依赖全部
`build-hosttool-<id>`。库、模块、initrd 和内核的 `build-*` 入口会自动先完成该阶段；
`build-host-tools` 保留为兼容入口。

`host-tool/hello-world` 用于验证该管线，运行后输出 `hello world`。工具的 Host triple 隔离产物位于 `build/<mode>/host/<host-triple>/host-tool/`，同时原子发布到 `build/<mode>/host-tool/`，供 freestanding 子构建使用。`mk-usrboot` 已通过该接口注册，可执行：

```sh
make build-host-tool tool=mk-usrboot
build/debug/host-tool/mk-usrboot module.elf -o module.usrboot
```

## 测试、示例与性能测试

运行 host testbench 前，配置集的 `clang.toml` 必须包含可通过 `make validate-host` 验证的 `[host]` 工具链。库通过 metadata 中的 `testbench.test`、`testbench.headercheck`、`testbench.bench`、`testbench.freestanding` 和 `testbench.example` 列表显式注册对应 TOML 文件。功能测试使用当前构建模式，默认构建并运行所有已注册的 test 程序：

```sh
make host-test
make host-test lib=tayclib
make host-test lib=taycpplib sanitize=address,undefined
```

runner 会继续执行全部匹配用例，最后汇总每项 `PASS`、`FAIL` 或 `SKIP`；任一用例失败时 `make host-test` 返回非零状态。`lib=<id>` 只选择指定库，`sanitize` 可选 `address`、`undefined` 或 `address,undefined`。

freestanding testbench 只针对当前交叉架构进行编译或链接，不会尝试运行生成物：

```sh
make freestanding-check
make freestanding-check lib=taycpplib arch=riscv64
```

`check-lib` 和 `build-lib-matrix` 会自动执行匹配的 freestanding checks。

示例程序位于 `testbench/example`。`make example` 只构建全部示例，`host-example` 则构建并顺序运行；两者都支持通过 `lib=` 过滤所属库：

```sh
make example
make example lib=taycpplib
make host-example
make host-example lib=tayclib
```

当前示例覆盖 `itoa`、`range`、`refc`、`owner`、`expected` 和 `panic`。其中 panic 示例的 `SIGABRT` 与 stderr 输出由 runner 按 metadata 自动验收。

性能测试位于 `testbench/bench`。`make bench` 默认使用 release 模式构建全部 benchmark executable，但不会运行：

```sh
make bench
make bench mode=debug
```

使用 `host-bench` 构建并顺序运行 benchmark；未显式指定 `mode` 时同样使用 release：

```sh
make host-bench
make host-bench lib=tayclib
make host-bench lib=taycpplib mode=release
```

测试程序输出到 `build/<mode>/host/<host-triple>/test/`，benchmark 和示例程序分别输出到同级 `bench/` 与 `example/`；sanitizer 构建使用独立的 `sanitize/<profile>/` 子目录。

## 运行

在内核已构建后，使用 `make runonly` 启动 QEMU。该目标不会重新构建内核，并通过 QEMU 的 `-kernel` 参数加载当前 `kernel-path`。

## 调试

在内核已构建后，使用 `make dbgonly` 以 QEMU 的 `-s -S` 选项启动调试。随后可用 GDB 连接到 `localhost:1234`；可参考仓库中的 VS Code 配置进行调试。

# 代码规范

## 命名规范

变量名与函数名采用 `c_style`. 命名空间也应采用 `c_style`, 可以使用缩写并尽量避免下划线的使用. 类型名则采用 `UpperCamelCase`. 宏采用 `MACRO`.

## 代码规范

文件开头应该有文件头注释, 包含文件名, 作者, 版本等信息. 格式参考

```cpp
/**
 * @file filename
 * @author author (email)
 * @brief A brief description of the file
 * @version 0.1.0-dev.1
 * @date the date
 *
 * @copyright Copyright (c) 2026
 *
 */
```

可考虑使用fileHeaderComment插件自动生成文件头注释.

无参数的函数应写作 `return_type func_name(void)`.
注意注释含量与密度.
命名应采用语义化命名方案, 可采用熟知缩写.

# 环境配置

构建配置位于 `config/<name>/*.toml`，通过 `make configure config=<name>` 生成 `script/.cache/` 下的 Make 片段。为兼容旧命令，显式传入的 `arch=` 或 `mode=` 会被忽略并给出警告；构建选择应使用 `make switch`。`make clean` 删除全部构建产物但保留配置，`make cleandist` 还会删除所有可再生成的配置与依赖缓存，只保留 `.switch.mk` 中的架构和模式选择。当前默认配置集为 `config/default/`；构建系统说明见 [构建系统概览](./aidoc/buildsystem/overview.md) 与 [配置流程](./aidoc/buildsystem/configuration.md)。
