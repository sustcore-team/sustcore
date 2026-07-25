# CRTN.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的尾代码(Epilogue)
# crtn.S - 提供 _init 和 _fini 函数的结尾部分

.section .init
	ret

.section .fini
	ret
