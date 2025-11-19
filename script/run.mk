# 权益之计
qemu-args-riscv64 ?= -m size=256m,maxmem=256m \
			 -bios default \
			 -serial stdio \
 			 -rtc base=localtime \
 			 -name "Sustcore" \
			 -machine virt \
			 -kernel ./build/bin/kernel/sustcore.bin

qemu-attached-args-riscv64 ?=

qemu-debug-riscv64 ?= -s -S

ifeq ($(architecture), riscv64)
	qemu-args := $(qemu-args-riscv64)
	qemu-attached-args := $(qemu-attached-args-riscv64)
	qemu-debug := $(qemu-debug-riscv64)
endif

.PHONY: run-risc
run:
	$(qemu) $(qemu-args)

.PHONY: run_dbg
run_dbg:
	$(qemu) $(qemu-args) $(qemu-attached-args) $(qemu-debug)