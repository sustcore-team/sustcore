# 内核构建流程

## 文件

内核构建目前拆分为：

- `kernel/Makefile`
- `kernel/flags.mk`
- `kernel/collect.mk`
- `kernel/include.mk`
- `kernel/enable.mk`
- `kernel/variant.riscv64.mk`
- `kernel/variant.loongarch64.mk`
- `kernel/dependencies.toml`

## 内核入口

顶层 Makefile 调用：

```make
$(MAKE) -f $(path-e)/kernel/Makefile \
    global-env=$(global-env) \
    arch=$(arch) \
    q=$(q) \
    ctx=$(path-ctx)/kernel.mk \
    kernel-path=$(kernel-path) \
    build
```

`kernel/Makefile` 是被动的子构建入口。

## 输出路径

顶层 Makefile 持有：

```make
kernel-path ?= $(path-bin)/kernel/sustcore.bin
```

`make configure` 为固定 owner `kernel` 生成 `ctx/kernel.mk`。内核子 Make 从该上下文取得根目录、对象目录和目标：

```make
owner-id := kernel
owner-root := /.../kernel
obj-root ?= $(path-obj)/kernel
target ?= $(kernel-path)
```

## 源文件收集

内核源文件发现目前使用：

- `script/build/component.mk`
- `script/build/collector.mk`
- `kernel/collect.mk`
- 根目录和子目录中的 `include.mk`

`collect.mk` 只调用共享收集器。每个 `include.mk` 通过 `src-y` 和 `src-n` 声明所在目录的源文件；内核根目录直接拥有源文件时也遵循这一规则。`include.mk` 不得登记子目录路径，子目录必须用自己的 `include.mk` 声明源码。同一实现类别可以写在同一条 `src-y +=` 中，不同类别使用新的 `src-y +=` 行，但赋值行之间不留空行。

内核 Makefile 通过 `component-config-mks` 和 `component-extra-objects` 提供架构变体文件与 initrd 附加对象，并在通用对象编译完成后只增加链接器和 objcopy 层。

当前活动源文件模型：

- `src-y`
- `src-n`
- 展开为：
  - `sources-y-asm`
  - `sources-y-c`
  - `sources-y-cpp`

## 启动方式选择

启动方式来自：

- `script/.cache/kernel.mk`
  - `<arch>-boot := sbi|laboot`
- `kernel/enable.mk`

当前规则：

- `riscv64` 默认使用 `sbi`；
- `loongarch64` 默认使用 `laboot`；
- 命令行 `enable-sbi` / `enable-laboot` 可以覆盖默认值。

## 注入的依赖

内核不再硬编码大部分库依赖，而是消费：

- `deps/kernel.mk`

`kernel/Makefile` 中当前的注入点为：

- `archives += $(or $(kernel-dep-archives-$(arch)),$(kernel-dep-archives))`
- `includes-c += ...`
- `includes-cpp += ...`
- `includes-asm += ...`

## 当前状态

内核构建目前支持：

- 对象文件编译；
- 依赖文件生成；
- 链接调用；
- `kernel-path` 覆盖；
- 按启动方式选择链接脚本。

内核仍在重构，完整的运行时与链接覆盖仍在演进。
