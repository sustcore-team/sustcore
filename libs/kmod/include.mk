sources += setup.cpp syscall.cpp printf.cpp malloc.cpp runtime.cpp file.cpp

riscv64-sources += arch/riscv64/crt0.S arch/riscv64/crti.S arch/riscv64/crtn.S \
	arch/riscv64/asm_syscall.S
loongarch64-sources += arch/loongarch64/crt0.S arch/loongarch64/crti.S arch/loongarch64/crtn.S \
	arch/loongarch64/asm_syscall.S
