#!/usr/bin/env python3
"""Emit kernel build configuration from kernel.toml."""

from mk_emitter import MkEmitter


def emit(data: dict) -> str:
    emitter = MkEmitter(data)
    emitter.keymap("$arch.boot", "$arch-boot")
    return emitter.render("kernel.toml")
