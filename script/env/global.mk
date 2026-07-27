# 这个文件一定要在每个 Makefile 的最前面 include, 保证各个变量的正确性

path-e := $(shell pwd)
path-s := $(path-e)/script

path-tools       := $(path-e)/tools
path-lib         := $(path-e)/libs
path-include     := $(path-e)/include
path-third_party := $(path-e)/third_party
path-cache       := $(path-s)/.cache
path-deps        := $(path-cache)/deps
path-ctx         := $(path-cache)/ctx

include $(path-s)/env/q.mk
include $(path-s)/env/shell.mk
include $(path-s)/env/scripts.mk
-include $(path-cache)/config.mk
include $(path-s)/py/init.mk
