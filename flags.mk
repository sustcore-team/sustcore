# Common build flags shared by kernel, modules, and libraries.

flags-common-c := -std=gnu18 -nostdlib -fno-builtin -ffreestanding \
	-Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
	-fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
	-fno-stack-protector -Wno-int-to-pointer-cast \
	-fno-toplevel-reorder -fno-tree-scev-cprop

flags-common-cpp := -std=gnu++23 -nostdlib -fno-builtin -ffreestanding \
	-Wall -Wno-sign-compare -Wno-literal-suffix -Wno-int-to-pointer-cast \
	-fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
	-fno-stack-protector -fno-toplevel-reorder -fno-tree-scev-cprop \
	

ifeq ($(architecture),riscv64)
flags-common-c += -mcmodel=medany
flags-common-cpp += -mcmodel=medany
endif

ifeq ($(architecture),loongarch64)
flags-common-c += -mcmodel=normal
flags-common-cpp += -mcmodel=normal
endif

flags-use-sustcore-features := -DUSE_SUSTCORE_FEATURES
flags-no-rtti-cpp := -fno-rtti
flags-no-exceptions-cpp := -fno-exceptions

flags-common-ld := -nostdlib
flags-kernel-ld := -z max-page-size=0x100
flags-module-ld := -z max-page-size=0x1000 -e_start

# 通过 JSON 文件临时引入一些因为不同工具链需要的 flag
# 比如说，评测机不能开 -mno-lsx，但本地测试要开
config-additional-flags-c ?=
config-additional-flags-cpp ?=
config-additional-flags-asm ?=
config-additional-flags-ld ?=

flags-common-c += $(config-additional-flags-c)
flags-common-cpp += $(config-additional-flags-cpp)
flags-common-ld += $(config-additional-flags-ld)
flags-asm += $(config-additional-flags-asm)

flags-mode-c :=
flags-mode-cpp :=
flags-mode-ld :=
defs-mode-c :=
defs-mode-cpp :=

ifeq ($(build-mode), debug)
flags-mode-c += -O0 -g
flags-mode-cpp += -O0 -g
else
flags-mode-c += -Ofast
flags-mode-cpp += -Ofast
flags-mode-ld += --strip-all
defs-mode-c += -DNDEBUG
defs-mode-cpp += -DNDEBUG
endif
