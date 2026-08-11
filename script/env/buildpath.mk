# 存储了 make switch arch=... mode=... 时的 arch 和 mode 的变量, 供其他 Makefile 使用
# build-mode 与 build-arch 变量的值可以通过调用 make 时使用 arch=... mode=... 来指定, 如果不指定则使用默认值
-include $(path-cache)/.switch.mk

environment := freestanding
arch ?= $(cached-arch)
mode ?= $(cached-mode)

# 使用的 build 根目录可以通过配置 path.toml 的 build-root 字段指定
path-build-root ?= $(path-e)/build

# 具体的 build 目录
path-build := $(path-build-root)/$(mode)/$(arch)
path-bin ?= $(path-build)/bin
path-obj ?= $(path-build)/obj
path-initrd-root ?= $(path-build)/initrd
path-initrd ?= $(path-build)/bin/initrd.cpio

include $(path-s)/env/selection.mk
include $(path-s)/env/q.mk
