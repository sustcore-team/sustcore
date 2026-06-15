path-e := $(shell pwd)
path-script := $(path-e)/script
path-tools := $(path-e)/tools
path-mount ?= /mnt/sustcore
path-img ?= $(path-e)/sustcore.img
path-lib := $(path-e)/libs
path-include := $(path-e)/include
path-third_party := $(path-e)/third_party
build-mode ?= release
build-arch ?= $(architecture)
path-build := $(path-e)/build/$(build-mode)/$(build-arch)
path-bin ?= $(path-build)/bin
path-objects ?= $(path-build)/objects
path-attach ?= $(path-bin)/attachment
path-initrd ?= $(path-bin)/initrd
path-kernel ?= $(path-bin)/kernel/sustcore.bin
path-kernel-phy ?= $(path-bin)/kernel/sustcore-phy.bin

#TODO
offset-kernel ?= 1048576

q ?= @
