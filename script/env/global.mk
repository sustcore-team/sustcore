path-e := $(shell pwd)
path-script := $(path-e)/script
path-tools := $(path-e)/tools
path-mount ?= /mnt/sustcore
path-img ?= $(path-e)/sustcore.img
path-lib := $(path-e)/libs
path-include := $(path-e)/include
path-attach := $(path-e)/build/bin/attachment
path-kernel := $(path-e)/build/bin/kernel/sustcore.bin
path-kernel-phy := $(path-e)/build/bin/kernel/sustcore-phy.bin
path-third_party := $(path-e)/third_party

#TODO
offset-kernel ?= 1048576

q ?= @