# 配置生成管线

## 两类配置

当前系统把生成的 Make 片段分为两类。

## 构建系统配置

这些片段通过 `script/.cache/config.mk` 包含，随后由 `script/env/global.mk` 加载。

当前成员：

- `clang.mk`
- `path.mk`
- `qemu.mk`
- `kernel.mk`

它们描述：

- 工具链选择；
- 输出目录布局；
- QEMU 运行参数；
- 内核启动模式默认值。

## 项目配置

这些片段不通过 `config.mk` 包含，而是由顶层或目标局部 Makefile 显式包含。

当前成员：

- `libraries.mk`
- `build-libs.mk`
- `programs.mk`
- `testbench.mk`
- `deps/<id>.mk`

它们描述：

- 全局库注册；
- 生成的 `build-libs` 目标；
- 模块 Makefile 和构建目标索引；
- 解析后的目标依赖集合；
- Host 测试、基准测试和头文件检查的分派信息。

`make configure` 还会生成组件专用构建上下文，但不会从 `config.mk` 或顶层 Makefile 直接包含它们：

- `ctx/lib-<id>.mk`
- `ctx/module-<id>.mk`
- `ctx/hostprog-<id>.mk`
- `ctx/kernel.mk`

每个上下文设置 `owner-id` 和 `owner-root`，并用 `?=` 提供 `obj-root` 和 `target` 默认值。构建索引通过 `ctx=` 把匹配的上下文传给组件子 Make，使这些通用变量只在单个组件中生效。

`script/env/global.mk` 按如下方式公开生成目录：

```make
path-deps := $(path-cache)/deps
path-ctx  := $(path-cache)/ctx
```

Host 验证还会生成 `host.mk`。该片段不通过 `config.mk` 包含；只有专用 Host 子 Make 在 `make validate-host` 成功后才会加载它。随后，Host 命令以原子方式生成 host 可见 owner 的 `deps/host-<id>.mk`。

## `make switch`

`make switch arch=<arch> mode=<mode>` 只把构建选择持久化到：

- `script/.cache/.switch.mk`

它不会重新生成库元数据或依赖缓存。保存构建选择后，该目标还会刷新供 clangd 使用、且被版本控制忽略的 `build/compile_commands.json` 副本。

## `make configure`

`make configure config=<name>` 读取：

- `config/<name>/*.toml`

并生成：

- 构建系统配置片段；
- 库注册表片段；
- 模块构建索引；
- 组件构建上下文；
- owner 依赖片段。

一次配置会解析所有已知 freestanding 架构。因此，改变持久化的架构或模式不会重新生成依赖片段。旧式 `arch=` 和 `mode=` 参数仍会被接受，但只会给出警告，不会影响配置输出或 `.switch.mk`。

配置生成只检查 `[host]` 的结构，不运行 Host 探测，因此没有 Host 工具链的机器仍可使用 freestanding 配置。

## Host 工具链配置

`clang.toml` 接受以下本机工具链配置：

```toml
[host]
clang = "clang"
"clang++" = "clang++"
ar = "llvm-ar"
sysroot = "/"
cppstdlib = "auto"
cflags = []
cxxflags = []
ldflags = []
```

`sysroot` 为必填项。`cppstdlib` 接受 `auto`、`libstdc++` 或 `libc++`。各类 flags 在 TOML 中使用数组，并分别用于 C 编译、C++ 编译和编译器驱动链接。

`make validate-host [host-arch=<arch>]` 会验证编译器家族、C/C++ 目标一致性、本机架构、LLVM ar、系统头文件搜索、C++ 标准库提供方，以及 C/C++ 的编译—链接—运行探测。所有检查通过后才写入 `host.mk`。该目标拒绝 `arch=`；`host-arch=` 只用于断言探测到的架构。

可以使用 `HOST_CLANG`、`HOST_CLANGXX`、`HOST_LLVM_AR`、`HOST_SYSROOT`、`HOST_CPPSTDLIB`、`HOST_CFLAGS`、`HOST_CXXFLAGS` 和 `HOST_LDFLAGS` 显式覆盖配置。Flags 覆盖使用 shell token 语法，并替换配置数组。

## 编译数据库

`make update [arch=<arch>] [mode=<mode>]` 通过 Bear 运行活动的 `build-kernel` 流程，并写入：

```text
build/<mode>/<arch>/compile_commands.json
```

省略 `arch` 或 `mode` 时，使用 `make switch` 持久化的值。显式覆盖可更新另一份数据库，但不会改变当前构建选择。

## 构建维度选择器

目标构建路径选定环境、架构和模式后，会导出以下 `y`/`n` 选择器：

```text
is-host                 is-freestanding
is-riscv64              is-loongarch64
is-debug                is-release
is-riscv64-debug        is-riscv64-release
is-loongarch64-debug    is-loongarch64-release
is-freestanding-riscv64 is-freestanding-loongarch64
is-host-<native-arch>
```

生成的 Make 片段通过计算出的名称追加匹配值，例如 `owner-dep-ids-$(is-riscv64)`、`library-ids-$(is-freestanding-riscv64)` 和 `testbench-program-ids-$(is-host)`，随后只公开相应的 `*-y` 桶。环境、架构、模式和活动的架构—模式组合各自恰好选择一个值。

## 缓存生命周期

- `switch` 只修改 `.switch.mk` 和稳定的 clangd 数据库选择。
- `configure` 重新生成配置、注册表、组件上下文和所有 freestanding 依赖变体；成功后会移除过期的 Host 验证和依赖片段。
- `clean` 删除完整的已配置构建输出，但保留配置。
- `cleandist` 先执行 `clean`，再删除所有生成的缓存条目，只保留 `.switch.mk`。

构建入口会诊断缺失或不完整的配置，并要求在 `cleandist` 后重新执行 `make configure`。

`make update-host [mode=<mode>] [sanitize=<set>]` 会验证本机工具链、解析 Host 依赖，并通过 Bear 捕获全部 Host 库、测试和基准测试翻译单元，但不运行 testbench 可执行文件。输出为：

```text
build/<mode>/host/<host-triple>/compile_commands.json
build/<mode>/host/<host-triple>/sanitize/<profile>/compile_commands.json
```

Bear 会先写入临时文件。只有 JSON 数组验证成功后才发布，因此捕获失败不会替换之前的数据库。

`make switch` 和 `make configure` 在写入自身缓存状态后，都会以原子方式把所选数据库复制到稳定的 clangd 入口：

```text
build/compile_commands.json
```

如果所选数据库尚未生成，则删除旧稳定副本，避免 clangd 使用另一架构的 flags。clangd 可通过 `--compile-commands-dir=build` 使用稳定目录。

使用 `make clangd-host [mode=<mode>] [sanitize=<set>]` 选择已生成的 Host 数据库，使用 `make clangd-target [arch=<arch>] [mode=<mode>]` 选择 freestanding 数据库。这两个目标只原子更新稳定副本，不修改 `.switch.mk`。`make switch` 和 `make configure` 仍会选择持久化的 freestanding 变体。

## 采用这种拆分的原因

项目级依赖状态不应通过 `global.mk` 被静默注入每个子 Make。

该拆分使：

- `global.mk` 专注于环境和构建系统默认值；
- 顶层和目标局部 Makefile 负责项目依赖数据。
