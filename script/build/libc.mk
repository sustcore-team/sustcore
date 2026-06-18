default-libc ?= placeholder-libc

libc.placeholder-libc.crt-head :=
libc.placeholder-libc.crt-tail :=
libc.placeholder-libc.module-linker-script :=

libc.kmod.crt-head := $(path-objects)/kmod/$(architecture)/crt0.o \
	$(path-objects)/kmod/$(architecture)/crti.o
libc.kmod.crt-tail := $(path-objects)/kmod/$(architecture)/crtn.o
libc.kmod.module-linker-script.riscv64 := $(path-e)/libs/kmod/kmod-riscv64.ld
libc.kmod.module-linker-script.loongarch64 := $(path-e)/libs/kmod/kmod-loongarch64.ld
libc.kmod.module-linker-script := $(or $(libc.kmod.module-linker-script.$(architecture)),$(path-e)/libs/kmod/kmod-riscv64.ld)
