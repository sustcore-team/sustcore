# tayclib 与 mini-cstd

## tayclib

公开头文件位于 `libs/tayclib/include/tay/`：

- `bits.h`：`byte/word/dword/qword`、`i8_t/u8_t...`、`xlen_t/slen_t`、
  `addr_t/off_t/ssize_t`。类型随指针宽度选择 XLEN。
- `itoa.h`：`itoa_s/utoa_s/ltoa_s/ultoa_s/lltoa_s/ulltoa_s`，支持 2 到
  36 进制并对输出缓冲区做截断和 NUL 终止。
- `ansi.h`：ANSI CSI、颜色和图形模式宏。
- `attribute.h`、`macros.h`、`bool.h`：编译器属性、预处理辅助和 C 布尔
  兼容定义。

tayclib 公开 C ABI，头文件可从 C++ 使用。当前静态库的主要实现源是整数到
字符串转换。

## mini-cstd

实现源当前集中在 `ctype.c` 与 `string.c`：

- 字符分类/转换：`isspace/isalpha/isdigit/.../isodigit/tolower/toupper`；
- 内存：`memset/memcpy/memmove/memcmp/memchr`；
- 字符串：`strlen/strnlen/strcmp/strncmp/strcpy/strncpy/strcat/strncat`、
  `strchr/strrchr/strspn/strcspn/strpbrk/strtok`。

`stdlib.h` 声明 `malloc/free/calloc/realloc/strtoul`，但这些符号不都由
mini-cstd 当前源文件实现，最终运行时/内核必须提供所需定义。`assert.h`
声明 assertion/panic 入口，也属于平台集成边界。

### 与标准 C 库的差异

这是最小子集，不包含 stdio、时间、locale、完整数学库等。部分长度/计数
接口当前使用 `int` 而非标准签名常见的 `size_t`；移植第三方代码前应逐个
核对声明。`strtok` 含内部状态，不适合并发或可重入解析。
