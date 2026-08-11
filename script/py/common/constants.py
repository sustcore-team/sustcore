"""Canonical build dimensions shared by configuration and runners."""

ARCHITECTURES = ("riscv64", "loongarch64")
ENVIRONMENTS = ("freestanding", "host")
BUILD_MODES = ("debug", "release")
SANITIZERS = ("", "address", "undefined", "address,undefined")
