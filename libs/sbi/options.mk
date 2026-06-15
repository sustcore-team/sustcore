component-kind := static-library
component-name := sbi
component-target := $(path-bin)/libs/$(architecture)/libkersbi.a
component-objdir := $(path-objects)/sbi/$(architecture)
variant.riscv64.libraries := libsbi

flags-c := $(flags-common-c) -nostdinc $(flags-mode-c)
include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
defs-c := $(defs-mode-c)
