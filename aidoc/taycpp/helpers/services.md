# 格式化、日志及其他辅助类型

## format

`<tay/format.h>` 及 `<tay/fmt/*.h>` 提供编译期检查的格式字符串、格式上下文、
格式引擎与内建 formatter。输出通过回调分块写出，结果为
`expected<size_t, format_error>`，因此可在无 iostream、无动态分配环境使用。
格式字符串错误会在编译期诊断；自定义类型可提供 formatter。

## logger

`<tay/logger.h>` 的 `logger<Output, MinimumLevel, ...>` 把 Output 作为状态化
策略保存。Output 可以持有串口、缓冲区或测试收集器状态，不要求全局静态
输出函数。日志级别包括 DEBUG/INFO/WARN/ERROR 等，低于编译期最低等级的
调用可被消除。日志结果沿用 format 的 `expected<size_t, format_error>`。

logger 会利用 `std::source_location` 生成文件、函数和行号信息；内核输出端
应避免递归获取同一锁。

## path、units、range

- `<tay/path.h>`：路径解析/组合辅助，失败使用 expected。
- `<tay/units.h>`：强类型单位与换算，避免裸整数混淆。
- `<tay/range.h>`：项目较早的 range 工具；新算法还使用
  `<tay/algo/ranges.h>` 中的 range CPO/概念。

## RTTI 与 reflection

`<tay/rtti.h>`、`<tay/reflection.h>` 属于实验/工具链相关设施。reflection
headercheck 仅在 host 且编译器支持静态反射特性时启用，不应假定所有目标
架构都可使用。内核常规代码仍应显式建模类型关系。
