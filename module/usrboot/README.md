# Usrboot

Usrboot 是 Sustcore 的第一个用户态程序, 也是 Sustcore 中唯一一个由内核直接加载的用户态程序. 其在编译为 ELF 格式后, 将由特殊的 mk-usrboot 程序处理成简易可处理的二进制格式, 以便内核在启动时直接加载.

Usrboot 内嵌了 initrdfs, initrdfs 是 Sustcore 的第一个用户态文件系统, 同时也包含了 Sustcore 的 initrd. Initrd 中含有若干个用户态程序, 其中最重要的是 init. Usrboot 会在 initrdfs 启动后, 将其挂载到 /initrd/ 下, 拉起 /initrd/init 程序, 并在这之后完成自己的任务.

vfs 则位于 sustcore 内核之中, 因此 usrboot 无需加载.

当前 usrboot 提供基于 `tay::logger` 的 `logger::debug`、`logger::info`、`logger::warn`、
`logger::error` 和 `logger::panic`。日志通过 `ec_write` syscall 输出；长期运行或致命错误
等待路径通过 `usrboot_yield()` 主动让出处理器，避免阻塞内核协作式 FIFO 调度器。

# Usrboot 格式

Usrboot 文件的格式十分简单，仅由 Header + Body 两部分组成。公共定义位于 `libs/libusrboot/include/usrboot.h`，Kernel 与 Host `mk-usrboot` 使用同一份头文件。格式固定为 64 位 little-endian，Header 大小固定为 120 字节：

```
struct usrboot_segment
{
    ub_addr64 vaddr; // 段的起始地址, 以字节为单位
    ub_off64 off; // 段在整个文件中的偏移, 以字节为单位
    ub_sz64 filesz; // 段在文件中的大小, 以字节为单位
    ub_sz64 memsz; // 段在内存中的大小, 以字节为单位
};host-tool/mk-usrboot

struct usrboot_header {
    ub_u64 magic; // 魔数, 固定为 "USRBOOT_"(64位)
    ub_sz64 body_size; // Body 的大小, 以字节为单位
    ub_addr64 entry; // 入口地址, 即 usrboot 的入口函数地址
    // 即 RX 段
    struct usrboot_segment seg_rx; // read-execute 段
    // 即 RW 段
    struct usrboot_segment seg_rw; // read-write 段
    // 即 RO 段
    struct usrboot_segment seg_ro; // readonly 段
};

```
魔数字节为 `USRBOOT_`。各段的 `off` 相对于整个 usrboot 文件的起点（Header 第一个字节），而不是 Body 起点。Body 按 RX、RW、RO 顺序紧密保存各段的文件内容，因此 RX 段通常从 `sizeof(usrboot_header)` 开始。BSS 等无文件内容的尾部不写入 Body，加载方根据 `memsz - filesz` 清零。

构建管线使用两个产物阶段：链接器通过 `ld-target` 生成 `usrboot.elf`，随后 `mk-usrboot` 生成最终 `usrboot`：

```text
objects -> usrboot.elf -> mk-usrboot -> usrboot
```

`mk-usrboot` 注册在 `host-tool/mk-usrboot`，依赖 `libusrboot` 与 `elf` 纯头文件库，可通过 `make build-host-tool tool=mk-usrboot` 独立构建。支持以下等价调用顺序：

```text
mk-usrboot <input> -o <output>
mk-usrboot -o <output> <input>
```

转换器验证 ELF64 字节序、机器类型、Program Header 范围、RX/RW/RO 段分类、虚拟地址重叠和入口地址。失败不会替换已有输出。最终产物供后续内核嵌入或加载流程消费；格式转换阶段本身不修改内核链接结果。
