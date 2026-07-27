# Resolve the active build dimensions after the selected buildpath has supplied
# environment, arch, and mode. Generated fragments route matching values into
# their y-* buckets and ignore n-* buckets.
include $(path-s)/toolchain/environment.mk

known-freestanding-archs := riscv64 loongarch64
known-build-modes := debug release

selected-arch-valid := $(if $(strip $(arch)),$(if $(filter host,$(environment)),$(strip $(arch)),$(filter $(arch),$(known-freestanding-archs))),y)
$(if $(selected-arch-valid),,$(error unsupported architecture '$(arch)' for environment '$(environment)'))
$(if $(strip $(mode)),$(if $(filter $(mode),$(known-build-modes)),,$(error unsupported build mode: $(mode))))

is-riscv64 := n
is-loongarch64 := n
$(if $(strip $(arch)),$(eval is-$(arch) := y))

is-debug := n
is-release := n
$(if $(strip $(mode)),$(eval is-$(mode) := y))

is-riscv64-debug := n
is-riscv64-release := n
is-loongarch64-debug := n
is-loongarch64-release := n
is-freestanding-riscv64 := n
is-freestanding-loongarch64 := n
$(if $(strip $(arch)),$(eval is-$(arch)-debug := n))
$(if $(strip $(arch)),$(eval is-$(arch)-release := n))
$(if $(strip $(arch)),$(eval is-host-$(arch) := n))
$(if $(strip $(arch)),$(eval is-freestanding-$(arch) := n))
$(if $(and $(strip $(arch)),$(strip $(mode))),$(eval is-$(arch)-$(mode) := y))
$(if $(and $(filter y,$(is-freestanding)),$(strip $(arch))),$(eval is-freestanding-$(arch) := y))
$(if $(and $(filter y,$(is-host)),$(strip $(arch))),$(eval is-host-$(arch) := y))
