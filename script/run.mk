# 权益之计
qemu-args ?= -m size=256m,maxmem=256m \
			 -bios default \
			 -serial stdio \
 			 -rtc base=localtime \
 			 -name "Sustcore" \
			 -machine virt -d int \
			 -kernel ./build/bin/kernel/sustcore.bin

qemu-debug ?= -s -S

.PHONY: run
run:
	$(qemu) $(qemu-args)

.PHONY: run_dbg
run_dbg:
	$(qemu) $(qemu-args) $(qemu-debug)