# 权益之计
qemu-args-riscv64 ?= -bios default \
 			 -name "Sustcore-rv64" \
			 -machine virt \
			 -kernel ./build/bin/kernel/sustcore.bin

qemu-attached-args-riscv64 ?=
qemu-attached-args ?=

ifeq ($(architecture), riscv64)
	qemu-args := $(qemu-args-riscv64)
	qemu-attached-args += $(qemu-attached-args-riscv64)
endif

qemu-memory-args ?= -m size=256m,maxmem=256m

qemu-serial-args ?= -serial stdio 

qemu-rtc-args ?= -rtc base=localtime

qemu-debug-args ?= -s -S

.PHONY: run
run:
	$(qemu) $(qemu-memory-args) $(qemu-args) $(qemu-serial-args) $(qemu-rtc-args) $(qemu-attached-args)

.PHONY: run_dbg
run_dbg:
	$(qemu) $(qemu-memory-args) $(qemu-args) $(qemu-serial-args) $(qemu-rtc-args) $(qemu-attached-args) $(qemu-debug-args)