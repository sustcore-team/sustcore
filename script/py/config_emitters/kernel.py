#!/usr/bin/env python3
"""Emit kernel build configuration from kernel.toml."""

from make_support.emitter import MkEmitter


def emit(data: dict) -> str:
    emitter = MkEmitter(data)
    emitter.keymap("$arch.boot", "$arch-boot")
    emitter.keymap("$mode.selftests", "$mode-kernel-selftests")
    return emitter.render("kernel.toml")
