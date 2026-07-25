# CRTN.S
# 与gcc自带的crtbegin.o与crtend.o配合组成C运行时初始化和清理代码
# 提供init节与fini节的尾代码(Epilogue)

    .section .init
    ret

    .section .fini
    ret
