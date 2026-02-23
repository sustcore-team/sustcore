# CRTI.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的头代码(Prologue)

# 如果定义了 PREINIT_FUNCTION，链接器会将其地址放入 .init_array 节中，确保在程序启动时调用它
#ifdef PREINIT_FUNCTION
    .section .init_array, "a", @init_array
    .dc.a PREINIT_FUNCTION
#endif

/* _init */
    .section .init, "ax", @progbits
    .global _init
    .type   _init, @function
_init:
    /* 功能由 init_array 接管 */
    /* 即使没有 init_array 接管, GCC也会帮我们把初始化代码放置于此处 */

/* _fini */
    .section .fini, "ax", @progbits
    .global _fini
    .type   _fini, @function
_fini:
    /* 同样, 这里是空的 */
