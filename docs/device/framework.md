# Device Framework

本文总结 `kernel/device/` 提供的统一设备抽象层: 它如何把平台相关的设备描述转换成内核可消费的统一节点、资源和设备模型。

## 目标

设备框架需要完成四件事:

1. **屏蔽平台差异**: 驱动不直接读取 DTB/ACPI 原始结构, 统一通过 `DeviceNode` 查询属性。
2. **集中维护全局设备状态**: 由 `DeviceModel` 保存设备节点、内存区、CPU 信息和中断管理器。
3. **把节点属性转成运行时资源**: 将设备节点中的 MMIO 和中断声明转换成 `MMIOResource` / `VIrqResource`。
4. **为驱动层提供稳定入口**: 驱动只依赖 `DeviceNode`、资源对象和 `DriverModel`。

## 核心对象

### `DevicePropView`

`device::DevicePropView` 是统一属性视图。它既能直接引用底层原始属性字节，也能承载框架预解析后的结构化结果。

支持的主要类型有:

- `STRING` / `STRING_LIST`
- `INTEGER` / `INTEGER_LIST`
- `BYTE_ARRAY`
- `REGION_LIST`
- `VIRQ_LIST`

其中 `REGION_LIST` 和 `VIRQ_LIST` 不是通用原始属性类型，而是设备后端主动构造出来的语义化结果:

- `mmio` 被解析成 `std::vector<PhyArea>`
- `irqs` 被解析成 `std::vector<virq_t>`

`VIRQ_LIST` 还支持延迟求值。FDT 后端不会在节点创建时立刻分配/解析所有 virq，而是把解析逻辑放进 loader，等驱动真正访问 `node.irqs()` 时再执行。

### `DeviceNode`

`device::DeviceNode` 是平台无关的统一设备节点接口。驱动层只应依赖这个接口，而不是具体的 FDT 节点结构。

必须实现的最小接口:

```cpp
virtual const char *name() const noexcept = 0;
virtual const char *platform() const noexcept = 0;
virtual Optional<DevicePropView> property(std::string_view name) const = 0;
```

框架在其上提供了几个高层便捷接口:

- `compatibles()`
- `is_compatible_with(...)`
- `mmio_regions()`
- `irqs()`
- `interrupt_parent()`

这意味着驱动通常不需要理解 `reg`、`interrupts` 等平台属性名，只要读取统一语义:

- `compatible`
- `mmio`
- `irqs`
- `interrupt-parent`

## `DeviceModel`

`device::DeviceModel` 是设备系统的核心聚合对象，也是单例。

它维护:

- `std::vector<owner<DeviceNode *>> _devices`
- `std::vector<MemRegion> _regions`
- `CpuGroupInfo _cpus`
- `driver::IrqManager _interrupt`
- `std::vector<owner<DeviceProvider *>> _providers`

### 职责

- 保存所有已注册的统一设备节点
- 保存并规范化物理内存区域
- 保存 CPU 列表、频率和拓扑
- 持有全局 `IrqManager`
- 驱动各个 `DeviceProvider` 将平台信息导入模型

### Provider 机制

`DeviceProvider` 是设备信息来源接口:

```cpp
virtual void register_device(DeviceModel &model) const = 0;
virtual const char *name() const = 0;
```

当前项目里有两个 provider:

- `fdt::FDTProvider`
- `device::KernelProvider`

`KernelProvider` 只负责把内核镜像 `[skernel, ekernel)` 注册为 `RESERVED` 内存区。

### 初始化顺序

`kernel/main.cpp` 中的 `init_device_model()` 明确了设备系统的启动顺序:

1. `DeviceModel::init()`
2. `DriverModel::init()`
3. `MMIOManager::init()`
4. `register_provider(new FDTProvider(dtb_ptr))`
5. `register_provider(new KernelProvider())`

这说明:

- 设备模型先初始化
- 驱动注册表和 MMIO 映射器比 provider 更早初始化
- FDT 导入是整个设备模型构建的主入口

## 内存区域模型

`MemRegion` 由 `PhyArea + MemoryStatus` 组成，状态包括:

- `FREE`
- `RESERVED`
- `ACPI_RECLAIMABLE`
- `ACPI_NVS`
- `IOMMU`
- `BAD_MEMORY`

`DeviceModel::collect_memory_regions()` 不只是简单追加，而是调用 `_normalize_memory_regions()` 做规范化。

### 规范化规则

当前实现会依次执行:

1. 合并相邻或重叠且状态相同的区域
2. 从 `FREE` 区域中减去所有非 `FREE` 覆盖部分
3. 非 `FREE` 区域之间若重叠，则把冲突部分标记为 `BAD_MEMORY`
4. 删除空区间
5. 按起始地址排序并再次合并同状态区域

因此 `DeviceModel::memory_regions()` 返回的是已经过冲突消解的最终视图，而不是原始 provider 输入。

## CPU 模型

CPU 抽象在 `kernel/device/cpu.h` 中。

### `Cpu`

`Cpu` 是架构无关接口，暴露:

- `id()`
- `model()`
- `frequency()`
- `state()`
- `caches()`
- `mmu_type()`
- `start() / stop() / send_ipi()`
- `local_intc()`

当前已实现的具体类型是 `RiscV64Cpu`。

### `CpuGroupInfo`

`DeviceModel::cpus()` 返回 `CpuGroupInfo`，其中包含:

- `cpus`: 当前系统的逻辑 CPU 对象列表
- `topology`: CPU 拓扑树

平台相关的 timebase 频率与 `ClockSource` 由 `DeviceModel::platform()` 提供。
在 RISC-V 平台上，具体实现为 `Riscv64Platform`，其时钟源类型为
`CSRTimeClockSource`。

### CPU 拓扑

拓扑由 `CpuTopology` / `CpuTopologyBuilder` 表示，层级包括:

- `THREAD`
- `CORE`
- `CLUSTER`
- `PACKAGE`
- `NUMA`

FDT 后端优先从 `cpu-map` 构造拓扑；失败时退回默认拓扑。

## 资源层

### `VIrqResource`

`VIrqResource` 封装一个单独的 `virq`。它本身不实现中断逻辑，只是把操作转发给 `DeviceModel::interrupt()`:

- `register_handler(...)`
- `unregister_handler()`
- `enable()`
- `disable()`
- `set_priority(...)`

所以驱动看到的是“设备拥有一个 virq 资源”，而不是“设备应该直接操作中断域”。

### `MMIOResource`

`MMIOResource` 封装一个单独的 MMIO 物理区间 `PhyArea`，并记录当前是否已经映射到内核地址空间。

### `DevResManager`

`DevResManager` 负责从 `DeviceNode` 中提取资源对象:

- `get_virq_resource(node)`
- `get_mmio_resource(node)`

其行为很直接:

- `node.irqs()` 的每个 virq 变成一个 `VIrqResource`
- `node.mmio_regions()` 的每个区域变成一个 `MMIOResource`

也就是说，资源层不做聚合，不解释“第 0 个 virq 是什么语义”；这由具体驱动自己定义。

## `MMIOManager`

`MMIOManager` 维护固定 KVA 窗口的 MMIO 映射策略。

### 映射模型

映射规则是:

```cpp
KVA = KVA_OFFSET + physical_mmio_address
```

它不是动态分配虚拟地址，而是把 MMIO 物理地址直接映射到固定偏移的内核虚拟地址区间。

### 主要接口

- `map_to_kernel(const MMIOResource &)`
- `unmap_from_kernel(const MMIOResource &)`

映射时会:

1. 把物理区间按页对齐
2. 在主内核页表中建立映射
3. 刷新 TLB
4. 把 `MMIOResource::_mapped` 置为 `true`

解除映射时执行对称操作。

## 生命周期边界

设备框架和驱动框架的边界很明确:

- `device/*` 负责描述“系统里有哪些设备、资源和中断入口”
- `driver/*` 负责描述“如何把这些设备实例化成可运行驱动”

其中最关键的约束是:

- `DeviceNode` 不拥有平台原始对象的构建逻辑
- `DriverBase` 不直接访问 DTB/FDT 结构
- 平台后端负责把平台格式翻译成统一节点

## 典型数据流

以一个 FDT 设备节点为例，数据流如下:

1. FDT provider 扫描到原始节点
2. 原始节点被包装为 `FDTDeviceNode`
3. `DeviceModel` 接管该 `DeviceNode`
4. 驱动工厂匹配该节点的 `compatible`
5. `DevResManager` 从节点提取 `MMIOResource` / `VIrqResource`
6. 驱动实例持有资源并运行

因此设备框架本质上是“平台描述 -> 统一节点 -> 资源对象”的中间层。
