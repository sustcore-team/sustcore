# 库注册表与依赖解析

## 元数据结构

库通过 `metadata.toml` 注册，当前结构使用 `[[libmeta]]`。

示例：

```toml
[[libmeta]]
id = "sbi"
libname = "libsbi.a"
makefile = "Makefile"
target = "build-static"
version = "0.1.0-dev.1"
support-archs = ["riscv64"]
support-environments = ["freestanding", "host"]
testbench.test = ["testbench/test/metadata.toml"]
testbench.headercheck = ["testbench/headercheck/metadata.toml"]
testbench.bench = ["testbench/bench/metadata.toml"]
testbench.freestanding = ["testbench/freestanding/metadata.toml"]
testbench.example = ["testbench/example/metadata.toml"]

include-c = ["include"]
include-cpp = ["include"]
include-asm = ["include"]

[libmeta.host]
libname = "libsbi-host.a"
makefile = "Makefile"
target = "build-host-static"
```

## 当前字段含义

- `id`
  - 全局逻辑标识符。
- `libname`
  - Freestanding 静态库文件名；空字符串表示该变体为纯头文件库。
- `makefile`
  - 相对于元数据文件的路径。
- `target`
  - 用于构建该库的子 Make 目标。
- `version`
  - 具体的 SemVer 2.0 库发布版本。
- `support-archs`
  - 支持的 freestanding 架构允许列表。
- `support-environments`
  - 包含 `freestanding` 和/或 `host` 的允许列表；默认只支持 freestanding。
- `include-c/cpp/asm`
  - 分别为各语言导出的 include 根目录。
- `testbench.test/headercheck/bench/freestanding/example`
  - 相对于当前库元数据的 testbench TOML 文件显式列表。
- `host.libname/makefile/target`
  - 可选的 Host 专用构建覆盖；省略字段继承通用值，`host.libname = ""` 只把 Host 变体标记为纯头文件库。

## 当前规则

- `id` 在所有依赖 owner 中必须全局唯一。
- 一个元数据文件可以包含多个 `[[libmeta]]`。
- 缺少 `support-archs` 表示“在所有架构上可用”。
- `support-archs` 绝不会限制本机 Host 可见性。
- 缺少 `include-*` 表示“不导出任何内容”。
- `libname = ""` 表示 freestanding 变体是纯头文件库。
- `[libmeta.host]` 覆盖可以独立增加或移除 Host 静态库。

## 版本命名与依赖要求

`metadata.toml` 使用 `version` 表示库提供的具体版本。它必须是不带 `v` 前缀的完整 SemVer 2.0 形式，例如：

```toml
version = "0.1.0"
version = "0.2.0-rc.1"
version = "0.2.0-dev.3+git.abc1234"
```

`dependencies.toml` 中的 `version` 表示版本范围要求，而不是依赖的具体版本。支持的形式包括：

- `*`：任意版本；
- 精确或不完整版本，例如 `1.2.3`、`1.2` 和 `1.x`；
- 比较器和范围交集，例如 `>=1.2.0 <2.0.0`；
- 兼容范围，例如 `^0.2.0`、`~1.2` 和 `1.2 - 2.0`；
- 逻辑 OR，例如 `^1.2 || ^2.0`。

构建元数据会被保留用于追踪，但不参与版本优先级和范围匹配。默认情况下，版本范围排除预发布版本；要匹配预发布版本，范围子句必须显式包含相同 `MAJOR.MINOR.PATCH` 核心版本的预发布比较器。

## 注册表生成

共享注册表由以下脚本构建：

- `script/py/metadata/registry.py`
- `script/py/generators/build_libs.py`

生成输出：

- `script/.cache/libraries.mk`

该注册表片段包含：

- 用于跨环境验证和矩阵枚举的 `library-ids-all`；
- `library-ids-$(is-freestanding-<arch>)` 和 `library-ids-$(is-host)`；
- 根据活动选择器的 `library-ids-y` 桶解析出的 `library-ids`；
- `library-<id>-version`；
- `library-<id>-libname`；
- `library-<id>-makefile`；
- `library-<id>-target`；
- `library-<id>-archive`；
- `library-<id>-is-header-only`；
- `library-<id>-include-c`；
- `library-<id>-include-cpp`；
- `library-<id>-include-asm`；
- `library-<id>-crt0-$(is-<arch>)`、`crti`、`crtn` 和 `ldscript` 变体。

生成的构建目标输出为：

- `script/.cache/build-libs.mk`

该构建目标片段包含：

- `build-lib-<id>`；
- `build-lib-targets-$(is-freestanding-<arch>)`；
- `host-build-lib-targets-$(is-host)`；
- `build-libs`。

每个可构建目标都通过 `ctx=` 把匹配的 `$(path-ctx)/lib-<id>.mk` 传给库子 Make。上下文声明库 owner 和源文件根目录，并为对象目录 `$(path-obj)/libs/<id>` 与最终静态库 `$(path-bin)/libs/<libname>` 提供默认值。Freestanding 与 Host 的静态库名及纯头文件状态互相独立选择。纯头文件变体仍会收到上下文，但其所选 archive 值展开为空。

## 组件构建片段

每个可构建的库或模块使用三个根片段：

- `Makefile` 标识组件根目录并选择产物层。
- `flags.mk` 声明组件局部编译 flags 和 include 路径。
- `collect.mk` 为组件根目录调用 `script/build/collector.mk`。

项目静态库在 `script/build/component.mk` 之上叠加 `script/build/static-library.mk`。组件层负责 buildpath/上下文加载、依赖包含、源文件规范化、工具链选择、对象规则、Host 工具链 stamp 和 depfile。静态库层只增加 `ar` 和 `build-static` 入口。因此，普通库 Makefile 为：

```make
component-root := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(component-root)/../../script/build/static-library.mk
```

通用归档规则使用 `ar-target ?= $(target)`，因此普通静态库仍直接生成上下文中的 `target`；需要在最终发布前增加后处理的组件可以覆盖 `ar-target`，让归档器只生成中间产物。链接规则对应使用 `ld-target ?= $(target)`。覆盖变量只改变归档器或链接器写入的文件，组件 Makefile仍需为最终 `target` 声明后处理规则。

源文件选择只能出现在 `include.mk` 中。收集器读取根目录和所有嵌套的 `include.mk`，按语言分类其中的 `src-y` 与 `src-n` 条目，并相对于组件根目录为嵌套路径添加前缀。每个 `include.mk` 只能登记其所在目录的源码，不得通过相对路径收集子目录；子目录应提供独立的 `include.mk`。同类源码可以合并在一条赋值中，不同实现类别应另起一条 `src-y +=` 或 `src-n +=`，但赋值行之间不留空行。

## Testbench 元数据

每个 `[[libmeta]]` 显式注册其所有 testbench 元数据文件：

```toml
testbench.test = ["testbench/test/metadata.toml"]
testbench.headercheck = ["testbench/headercheck/metadata.toml"]
testbench.bench = ["testbench/bench/metadata.toml"]
testbench.freestanding = ["testbench/freestanding/metadata.toml"]
testbench.example = ["testbench/example/metadata.toml"]
```

五个 `testbench` 字段都是可选数组。省略字段等价于空数组，表示该库没有为该类别注册元数据。路径必须指向相对于库元数据存在的 TOML 文件。一个元数据文件可以包含多个 `[[libmeta]]`，每个条目拥有独立的 testbench 列表。

测试和基准文件注册可执行程序：

```toml
[[hostprog]]
id = "example-test"
kind = "test"
makefile = "Makefile"
target = "build"
output = "example-test"
```

`kind = "test"`、`kind = "bench"` 和 `kind = "example"` 只能分别出现在对应 `testbench.test`、`testbench.bench` 和 `testbench.example` 字段列出的文件中。`testbench.headercheck` 列出的头文件检查文件只能包含 `[[headercheck]]` 条目。扫描器不会按目录或文件名发现未注册文件。

Freestanding 文件注册为所选目标架构构建、但绝不在 Host 上执行的编译/链接检查：

```toml
[[freestanding-check]]
id = "example-contract"
kind = "link"
language = "c++"
sources = ["consumer.cpp", "provider.cpp"]
expect = "success"
```

`kind` 为 `compile` 或 `link`；`expect` 为 `success` 或 `failure`。源文件必须与已注册的元数据文件位于同一目录，并匹配声明的 C 或 C++ 语言。这些检查可通过 `make freestanding-check`、`check-lib` 和 `build-lib-matrix` 使用。

## 依赖文件结构

依赖 owner 使用：

```toml
[[dependencies]]
lib = "mini-cstd"

[[riscv64.dependencies]]
lib = "sbi"

[[host.dependencies]]
lib = "tayclib"
```

解析器会合并公共段、所选环境段和每个适用的架构段。一次 `make configure` 会解析并验证全部已知 freestanding 架构。重复依赖只有在版本表达式完全相同时才会去重；表达式冲突会报错。随后解析器会：

- 读取顶层 `[[dependencies]]`；
- 读取架构专用 `[[<arch>.dependencies]]`；
- 与当前注册表匹配；
- 验证 owner 显式列出了所有传递依赖；
- 在缺失依赖诊断中复用子依赖的版本表达式。

生成输出：

- `script/.cache/deps/<id>.mk`
- Host 验证后的 `script/.cache/deps/host-<id>.mk`

每个文件在内部使用构建维度选择器，并只导出最终选中的值：

- `<id>-dep-ids`
- `<id>-dep-archives`
- `<id>-includes-c`
- `<id>-includes-cpp`
- `<id>-includes-asm`

例如，公共依赖追加到 `<id>-dep-ids-y`，架构依赖通过 `<id>-dep-ids-$(is-<arch>)` 追加；最终 `<id>-dep-ids` 从活动的 `y` 桶赋值。Host 片段使用相同接口，但只在本机架构验证后生成。

## 当前示例

- `mini-cstd`
  - 与架构无关；
  - 不导出 include 路径。
- `fdt`
  - 与架构无关；
  - 导出 libfdt 头文件。
- `sbi`
  - `support-archs = ["riscv64"]`；
  - 只对 `riscv64` 可见。

## 当前限制

- 不支持重复的 `id`；
- 不支持同一 `id` 下的多个版本；
- 同一目录中的所有 `[[libmeta]]` 条目共享库局部 `dependencies.toml`。
