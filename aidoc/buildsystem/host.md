# Host 构建、测试、示例与基准测试

## 当前范围

Host 命令负责验证本机 Clang 工具链、解析环境专用依赖，并在隔离目录中构建库、测试、示例、头文件检查和基准测试。

选择配置后执行验证：

```text
make configure config=custom
make validate-host
make validate-host host-arch=x86_64
make build-host-libs
make host-test [lib=tayclib] [sanitize=address,undefined]
make example [lib=taycpplib]
make host-example [lib=taycpplib]
make bench
make host-bench [lib=taycpplib]
make update-host [mode=debug] [sanitize=address,undefined]
make clangd-host [mode=debug] [sanitize=address,undefined]
make clangd-target [arch=riscv64] [mode=debug]
```

验证过程绝不会调用 `make switch`，不会读取缓存的目标架构，并且只有全部探测成功后才发布 Host 配置。

## 验证契约

C 和 C++ 命令都必须是 Clang，并报告相同的目标 triple。规范化后的架构必须同时匹配 `uname -m` 和可选的 `host-arch` 断言。归档命令必须报告 LLVM 版本。

每个编译器探测都使用配置的 `--sysroot`。详细头文件搜索只能使用该 sysroot、Clang resource directory 和显式选择的 GCC 安装；其他系统根目录会被拒绝。随后使用对应的编译器驱动链接 C/C++ 探测程序，并作为本机可执行文件运行。

对于 C++，`cppstdlib=libstdc++` 选择 `-stdlib=libstdc++`，`cppstdlib=libc++` 选择 `-stdlib=libc++`。`auto` 会记录标准头文件探测发现的提供方。可以在 `cxxflags` 中使用 `--gcc-install-dir=...` 固定 GCC 安装。

## 生成状态

验证成功后，以原子方式写入 `script/.cache/host.mk`。其中记录：

- 规范化的 Host 架构和完整目标 triple；
- 解析后的编译器驱动与归档器调用路径；
- 编译器、归档器和 C++ 标准库版本；
- sysroot 以及实际 C/C++/链接 flags；
- 覆盖全部已验证工具链输入的指纹；
- 非致命编译器探测发现的可选特性。

专用 Host 子 Make 加载 `script/env/host-buildpath.mk`，生成：

```text
build/<mode>/host/<host-triple>/bin/
build/<mode>/host/<host-triple>/obj/
build/<mode>/host/<host-triple>/test/
build/<mode>/host/<host-triple>/bench/
build/<mode>/host/<host-triple>/example/
```

Freestanding 构建路径仍为 `build/<mode>/<arch>/`。共享 C++ 规则会在编译命令末尾强制加入且仅加入一个环境宏：`TAY_ENV_HOST=1` 或 `TAY_ENV_FREESTANDING=1`。

静态片段 `script/toolchain/c.mk`、`cpp.mk`、`ar.mk` 和 `ld.mk` 由两个环境共享。`toolchain/environment.mk` 把 `is-host`/`is-freestanding` 解析为 `y`/`n`；每个工具链片段通过这些计算变量名写入两个候选值，并只消费最终的 `y-toolchain-*` 值。验证后的 Host 值仍来自生成的 `script/.cache/host.mk`。

Sanitizer 构建使用 `build/<mode>/host/<host-triple>/sanitize/<profile>/`，未启用 sanitizer 的路径保持不变。支持的配置为 `address`、`undefined` 和 `address,undefined`。

## 命令语义

- `build-host-libs` 构建所有声明了静态库的 Host 变体。
- `build-host-lib lib=<id>` 构建所选 Host 静态库；如果该 Host 变体是纯头文件库，则运行 Host 头文件检查。
- `host-test` 构建并运行全部匹配的功能测试，包括 abort/stderr 断言。单个测试失败后仍会继续，为每个所选程序报告 `PASS`、`FAIL` 或 `SKIP`；只要有任一程序失败，汇总结果即失败。
- `example` 构建全部匹配的演示程序，但不运行。
- `host-example` 按顺序运行全部匹配的演示，包括声明的 abort/stderr 预期。
- `bench` 默认使用 release，只构建全部已注册基准测试。
- `host-bench` 默认使用 release，并通过同一汇总运行器依次执行全部匹配的性能基准测试。
- `host-header-check` 独立编译每个适用的公开头文件。
- `freestanding-check` 执行已注册的目标编译/链接检查，不运行跨架构输出。
- `update-host` 通过 Bear 捕获 Host 库以及全部测试、基准和示例翻译单元，但不运行可执行文件。
- `clangd-host` 和 `clangd-target` 选择稳定的 clangd 数据库，不改变持久化的目标构建选择。

每条 Host 命令都会先验证工具链，再根据已验证的本机架构解析 `deps/host-<owner>.mk`，随后进入递归 Host 子 Make。这些步骤都不会更新 `.switch.mk`。

## Testbench 布局

库 testbench 分别使用功能、性能和示例目录，同时保留 `kind = "test"|"bench"|"example"` 元数据接口：

```text
libs/<library>/testbench/test/metadata.toml
libs/<library>/testbench/headercheck/metadata.toml
libs/<library>/testbench/bench/metadata.toml
libs/<library>/testbench/freestanding/metadata.toml
libs/<library>/testbench/example/metadata.toml
```

所属 `[[libmeta]]` 通过 `testbench.test`、`testbench.headercheck`、`testbench.bench`、`testbench.freestanding` 和 `testbench.example` 列表注册每个文件。测试、基准和示例文件包含相应的 `[[hostprog]]` 条目；头文件检查只包含 `[[headercheck]]`，freestanding 检查只包含 `[[freestanding-check]]`。未注册文件会被忽略，也不会执行旧式目录扫描。

汇总 Python 运行器只处理可执行的 Host testbench。头文件检查仍由专用目标处理。Freestanding 契约使用通用编译/链接检查器，绝不会作为本机程序运行。
