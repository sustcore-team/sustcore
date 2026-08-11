# Initrdfs

Initrdfs 是 Sustcore 启动流程中首个被加载的用户态文件系统, 其主要作用是为 Sustcore 提供一个最小化的用户态环境, 以便在内核启动后能够顺利拉起 init 程序, 并最终完成 Sustcore 的启动流程.

Initrdfs 内部嵌入了 initrd.cpio 文件作为 initrd, initrd 中包含了 Sustcore 启动所需的最小化用户态程序, 如 init 程序.
Initrdfs 被 usrboot 加载并挂载到 /initrd/ 下, 之后等待程序的文件读写请求, 以便为 Sustcore 提供最小化的用户态环境.