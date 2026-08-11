#!/usr/bin/env python3
"""Emit path configuration from path.toml."""

from make_support.emitter import MkEmitter


def emit(data: dict) -> str:
    emitter = MkEmitter(data)
    emitter.keymap("build-root", "path-build-root")
    return emitter.render("path.toml")
