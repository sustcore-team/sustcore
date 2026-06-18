default-libc ?= placeholder-libc

libc.placeholder-libc.crt-head :=
libc.placeholder-libc.crt-tail :=
libc.placeholder-libc.module-linker-script :=

libc.kmod.crt-head := $(path-objects)/kmod/$(architecture)/arch/$(architecture)/crt0.o \
	$(path-objects)/kmod/$(architecture)/arch/$(architecture)/crti.o
libc.kmod.crt-tail := $(path-objects)/kmod/$(architecture)/arch/$(architecture)/crtn.o
libc.kmod.module-linker-script.riscv64 := $(path-e)/libs/kmod/arch/riscv64/kmod.ld
libc.kmod.module-linker-script.loongarch64 := $(path-e)/libs/kmod/arch/loongarch64/kmod.ld
libc.kmod.module-linker-script := $(or $(libc.kmod.module-linker-script.$(architecture)),$(path-e)/libs/kmod/arch/riscv64/kmod.ld)
