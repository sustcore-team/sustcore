ifeq ($(architecture), x86)
arch-include := x86.mk
else 
ifeq ($(architecture), x86_64)
arch-include := x86_64.mk
else 
ifeq ($(architecture), riscv64)
arch-include := riscv64.mk
else 
ifeq ($(architecture), loongarch64)
arch-include := loongarch64.mk
endif
endif
endif
endif

arch-include ?= riscv64.mk

include $(path-script)/build/arch/$(arch-include)