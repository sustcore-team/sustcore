# 辅助类与基础设施

| 主题 | 头文件/类型 |
| --- | --- |
| RAII 与所有权 | `guard`、`owner`、`unique_ptr`、`refcount` |
| 错误处理 | `expected`、`unexpected`、`error_code`、`panic` |
| 可调用对象 | `function_ref`、`inplace_function`、`composition`、`overloaded` |
| 分配 | `allocator`、`allocator_traits`、freestanding hooks |
| 文本设施 | `format`、`logger`、`path`、`units` |
| 低层辅助 | `range`、`container_of`、RTTI/reflection 实验接口 |

详细文档：

- [所有权与 RAII](ownership-raii.md)
- [错误、分配与 panic](errors-allocation.md)
- [可调用对象与 utility](callables.md)
- [格式化、日志及其他辅助类型](services.md)
