# The freestanding interface follows the same no-exception/no-RTTI contract as
# the kernel. Host tests retain exceptions so state-restoration paths are
# exercised there.
flags-cpp += $(if $(filter y,$(is-freestanding)),-fno-exceptions -fno-rtti)
