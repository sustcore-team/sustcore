内核启动代码

负责在内核启动时进行必要的初始化工作, 其保证进入内核主函数时, 一定满足以下几个需求:
1. 构建原始内核页表, 保证进入内核主函数时:
    1. 内核虚拟地址空间(KVA)已经被映射
    即以下区域被映射:
    ```
    [kernel_vstart, kernel_vend)
    ```
    kernel_vstart 与 kernel_vend 由 &skernel 和 &ekernel 说明
    物理地址到内核虚拟地址的映射关系为:
    ```
    vaddr = paddr + 0xffff_ffff_0000_0000
    ```
    2. 内核物理地址空间(KPA)开头 1GB 区域已经被映射
    即以下区域被映射
    ```
    [KPA_OFFSET + PM_LOW, KPA_OFFSET + PM_LOW + 1GB)
    ```
    其中 KPA_OFFSET = 0xffff_ffc0_0000_0000
    PM_LOW 在绝大部分机器上均为 0
    在 RISCV64 上, PM_LOW 为 0x8000_0000
2. 将 BootInfo 传递给内核
   1. BootInfo 应该以 BootInfoHeader 开头, 存放在 MemRegions 的 BOOT_RECLAIMABLE 区域中