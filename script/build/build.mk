# 目标模式(内核/loader k 模块m 静态库s 动态库l)
mode ?=

# obj文件
objects ?=

# 目标
target ?=

# 库列表
libraries ?=

libraries-ld := $(foreach lib, $(libraries), -l$(lib))

# 源文件目录
dir-src ?=

# 目标文件目录
dir-obj ?=

# 构建所需文件
build-objects := $(foreach obj, $(objects), $(dir-obj)/$(obj)) \
				 $(foreach attachment, $(attachments), $(dir-obj)/attachment/$(attachment))

# libc特殊对象
obj-crt0-libc ?= $(dir-obj)/$(obj-crt0)
obj-crti-libc ?= $(dir-obj)/$(obj-crti)
obj-crtn-libc ?= $(dir-obj)/$(obj-crtn)

# module链接时, 若默认链接kmod, 则将kmod的CRT对象显式注入最终链接
module-use-kmod-crt ?= $(if $(filter kmod,$(libraries)),true,false)
ifeq ($(module-use-kmod-crt),true)
module-crt-head-objs ?= $(path-objects)/kmod/$(architecture)/crt0.o \
						$(path-objects)/kmod/$(architecture)/crti.o
module-crt-tail-objs ?= $(path-objects)/kmod/$(architecture)/crtn.o
else
module-crt-head-objs ?=
module-crt-tail-objs ?=
endif

# 架构(x86 x86_64 riscv loongrach)
architecture ?= riscv