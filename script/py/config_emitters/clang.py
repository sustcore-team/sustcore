#!/usr/bin/env python3
"""Emit compiler configuration from clang.toml."""

from common.constants import ARCHITECTURES
from make_support.emitter import MkEmitter


TARGET_ARCHITECTURES = ARCHITECTURES
HOST_FIELDS = {
    "clang": str,
    "clang++": str,
    "ar": str,
    "sysroot": str,
    "cppstdlib": str,
    "cflags": list,
    "cxxflags": list,
    "ldflags": list,
}


def _validate_host(data: dict) -> None:
    host = data.get("host")
    if host is None:
        return
    if not isinstance(host, dict):
        raise ValueError("clang.toml: host must be a table")
    unknown = set(host) - set(HOST_FIELDS)
    if unknown:
        raise ValueError(
            "clang.toml: unsupported host field: " + ", ".join(sorted(unknown))
        )
    for field, expected_type in HOST_FIELDS.items():
        if field not in host:
            continue
        value = host[field]
        if not isinstance(value, expected_type):
            expected = "an array of strings" if expected_type is list else "a string"
            raise ValueError(f"clang.toml: host.{field} must be {expected}")
        if expected_type is list and not all(isinstance(item, str) for item in value):
            raise ValueError(f"clang.toml: host.{field} must be an array of strings")


def emit(data: dict) -> str:
    _validate_host(data)
    emitter = MkEmitter(data)
    for arch in TARGET_ARCHITECTURES:
        emitter.keymap(f"{arch}.clang", f"{arch}-comp-c")
        emitter.keymap(f"{arch}.clang++", f"{arch}-comp-cpp")
    emitter.keymap("flags.clang.$arch", "$arch-flags-c")
    emitter.keymap("flags.clang++.$arch", "$arch-flags-cpp")
    emitter.keymap("flags.clang", "freestanding-config-flags-c")
    emitter.keymap("flags.clang++", "freestanding-config-flags-cpp")
    emitter.keymap("host.clang", "host-config-clang")
    emitter.keymap("host.clang++", "host-config-clangxx")
    emitter.keymap("host.ar", "host-config-llvm-ar")
    emitter.keymap("host.sysroot", "host-config-sysroot")
    emitter.keymap("host.cppstdlib", "host-config-cppstdlib")
    emitter.keymap("host.cflags", "host-config-cflags")
    emitter.keymap("host.cxxflags", "host-config-cxxflags")
    emitter.keymap("host.ldflags", "host-config-ldflags")
    return emitter.render("clang.toml")
