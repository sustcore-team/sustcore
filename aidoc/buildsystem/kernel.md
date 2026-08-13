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

`make configure` 从 `kernel/metadata.toml` 的 `[[kernelmeta]]` 生成 `ctx/kernel.mk`。Kernel metadata
与模块的 `[[progmeta]]` 使用相同的 makefile、target、output 字段，但不允许 libc 字段。内核子
Make 从该上下文取得根目录、对象目录和目标：

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

内核 Makefile 通过 `component-config-mks` 提供架构变体文件，并在通用对象编译完成后只增加链接器层。`build-kernel` 只依赖当前架构可见的库，不依赖 initrd。

内核依赖公共的 `libusrboot` 纯头文件库，以使用与 Host 转换器一致的 `usrboot_header` 和 `usrboot_segment` 文件格式定义。该依赖只导出头文件，不增加静态归档。

Initrd 的输入由 `initrd/initrd.toml` 声明。`make configure` 通过
`script/py/generators/initrd.py` 生成 `script/.cache/initrd.mk`，其中包含模块实际产物、普通文件和
staging 路径的 Make 依赖及复制 recipe。`initrd.cpio` 由生成的 recipe 使用 GNU cpio
的 `newc` 格式创建；Python 不再在构建阶段直接组装归档。归档先写入临时文件，再原子替换
最终输出，因此未改变输入时不会重复生成。该归档是独立构建产物，不再通过 objcopy 转换为
目标文件，也不再链接到内核镜像的 `.attach.initrd` 段。

Usrboot 程序使用 owner ID `usrboot`，最终输出路径为 `module/usrboot`。模块子构建先通过通用链接规则生成 `usrboot.elf`，再按需构建稳定路径下的 Host `mk-usrboot`，将 ELF 转换为最终 `usrboot`。当前 initrd 配置以 `/modules/usrboot` 收集该最终产物。共享格式头文件由 `libusrboot` 提供。

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

Owner metadata 还可以声明二进制附件：

```toml
[attach]
[[attach.module]]
mod = "usrboot"
segment = ".rodata.usrboot"
```

configure 会生成附件依赖片段，并将附件 object 注入 kernel 或 module 的 `objects` 列表。
`script/rules/attachment.mk` 通过通用的 binary-to-ELF 规则生成 relocatable object，输入依赖
模块最终产物，section 名由 metadata 显式指定。usrboot attachment 作为只读输入节被内核
链接脚本收集到 `.rodata`，其镜像范围由 `s_usrboot` 与 `e_usrboot` 表征。

## 当前状态

内核构建目前支持：

- 对象文件编译；
- 依赖文件生成；
- 链接调用；
- `kernel-path` 覆盖；
- 按启动方式选择链接脚本。

内核仍在重构，完整的运行时与链接覆盖仍在演进。
