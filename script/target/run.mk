-include $(path-s)/toolchain/qemu.mk

.PHONY: runonly dbgonly

qemu := $($(arch)-qemu)
qemu-bios-args :=
qemu-machine-args := -machine virt
qemu-kernel-args := -kernel $(kernel-path)
qemu-serial-args := -nographic
qemu-debug-args := -s -S

ifeq ($(arch),riscv64)
qemu-bios-args := -bios default
endif

qemu-run-command := $(qemu) $(qemu-bios-args) $(qemu-machine-args) $(qemu-kernel-args) $(qemu-generated-args) $(qemu-attached-args) $(qemu-serial-args)
qemu-dbg-command := $(qemu-run-command) $(qemu-debug-args)

runonly:
	$(q)$(qemu-run-command)

dbgonly:
	$(q)$(qemu-dbg-command)
