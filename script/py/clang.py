#!/usr/bin/env python3
"""Emit compiler configuration from clang.toml."""

from mk_emitter import MkEmitter


def emit(data: dict) -> str:
    emitter = MkEmitter(data)
    emitter.keymap("$arch.clang", "$arch-comp-c")
    emitter.keymap("$arch.clang++", "$arch-comp-cpp")
    emitter.keymap("flags.clang.$arch", "$arch-flags-c")
    emitter.keymap("flags.clang++.$arch", "$arch-flags-cpp")
    emitter.keymap("flags.clang", "flags-c")
    emitter.keymap("flags.clang++", "flags-cpp")
    return emitter.render("clang.toml")
