#!/usr/bin/env python3
"""Generate Make configuration files from a selected TOML configuration set."""

from __future__ import annotations

import sys
from pathlib import Path

import tomllib

from common.arguments import parse_key_value_arguments
from common.filesystem import write_text_if_changed
from common.paths import CACHE_ROOT, REPOSITORY_ROOT
from config_emitters import clang, kernel, path, qemu
from dependencies import resolver as resolve_deps
from generators import (
    build_ctx,
    build_host_tools,
    build_libs,
    build_programs,
    build_testbench,
    initrd,
)
from metadata.registry import scan_dependency_owners


ROOT = REPOSITORY_ROOT
CONFIG_ROOT = ROOT / "config"
CONFIG_CACHE = CACHE_ROOT / ".configure.mk"
EMITTERS = {
    "clang": clang,
    "kernel": kernel,
    "path": path,
    "qemu": qemu,
}


def _read_cached_config() -> str | None:
    if not CONFIG_CACHE.is_file():
        return None
    for line in CONFIG_CACHE.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition(":=")
        if separator and key.strip() == "cached-config":
            value = value.strip()
            return value or None
    return None


def parse_arguments(arguments: list[str]) -> tuple[str, tuple[str, ...]]:
    values = parse_key_value_arguments(arguments, {"config", "arch", "mode"})

    config_name = values.get("config") or _read_cached_config() or "default"
    ignored = tuple(key for key in ("arch", "mode") if key in values)
    return config_name, ignored


def load_emitter(name: str):
    emitter = EMITTERS.get(name)
    if emitter is None:
        raise ValueError(f"no emitter for {name}.toml")
    return emitter


def remove_stale_cache_fragments(expected_names: set[str]) -> None:
    expected_names.add(".switch.mk")
    for cache_path in CACHE_ROOT.glob("*.mk"):
        relative_name = cache_path.relative_to(CACHE_ROOT).as_posix()
        if relative_name not in expected_names:
            cache_path.unlink()

    for managed_root in (CACHE_ROOT / "deps", CACHE_ROOT / "ctx"):
        if not managed_root.is_dir():
            continue
        for cache_path in managed_root.rglob("*"):
            if cache_path.is_dir() and not cache_path.is_symlink():
                continue
            relative_name = cache_path.relative_to(CACHE_ROOT).as_posix()
            if relative_name not in expected_names:
                cache_path.unlink()
        for directory in sorted(
            (path for path in managed_root.rglob("*") if path.is_dir()),
            key=lambda path: len(path.parts),
            reverse=True,
        ):
            if not any(directory.iterdir()):
                directory.rmdir()
        if not any(managed_root.iterdir()):
            managed_root.rmdir()


def generate(config_name: str) -> None:
    config_dir = (CONFIG_ROOT / config_name).resolve()
    if CONFIG_ROOT not in config_dir.parents or not config_dir.is_dir():
        raise ValueError(f"configuration directory does not exist: {config_name}")

    toml_files = sorted(config_dir.glob("*.toml"))
    if not toml_files:
        raise ValueError(f"configuration directory is empty: {config_dir}")

    generated: list[str] = []
    contents: dict[str, str] = {}
    for toml_path in toml_files:
        with toml_path.open("rb") as config_file:
            data = tomllib.load(config_file)
        emitter = load_emitter(toml_path.stem)
        generated_name = f"{toml_path.stem}.mk"
        contents[generated_name] = emitter.emit(data)
        generated.append(generated_name)

    libraries_name = "libraries.mk"
    build_libraries_name = "build-libs.mk"
    libraries_content, build_libraries_content = build_libs.emit(ROOT)
    contents[libraries_name] = libraries_content
    contents[build_libraries_name] = build_libraries_content
    generated.extend((libraries_name, build_libraries_name))

    component_ctx = build_libs.emit_ctx(ROOT)

    programs_name = "programs.mk"
    contents[programs_name] = build_programs.emit(ROOT)
    generated.append(programs_name)
    component_ctx.update(build_programs.emit_ctx(ROOT))

    host_tools_name = "host-tools.mk"
    contents[host_tools_name] = build_host_tools.emit(ROOT)
    generated.append(host_tools_name)
    component_ctx.update(build_host_tools.emit_ctx(ROOT))

    initrd_name = "initrd.mk"
    contents[initrd_name] = initrd.emit()
    generated.append(initrd_name)

    testbench_name = "testbench.mk"
    contents[testbench_name] = build_testbench.emit(ROOT)
    generated.append(testbench_name)
    component_ctx.update(build_testbench.emit_ctx(ROOT))

    kernel_ctx_name = build_ctx.kernel_name()
    component_ctx[kernel_ctx_name] = build_ctx.emit(
        "kernel",
        str((ROOT / "kernel").resolve()),
        "$(path-obj)/kernel",
        "$(kernel-path)",
    )
    for ctx_name, ctx_content in component_ctx.items():
        contents[f"ctx/{ctx_name}"] = ctx_content

    for owner in scan_dependency_owners(ROOT):
        if owner.kind == "host-tool":
            continue
        deps_name = f"deps/{owner.id}.mk"
        contents[deps_name] = resolve_deps.emit(ROOT, owner)
        generated.append(deps_name)

    include_lines = [
        "# Generated by script/py/configure.py. Do not edit this file directly.",
        "",
    ]
    include_lines.extend(
        f"-include $(path-cache)/{name}"
        for name in generated
        if name in {"clang.mk", "kernel.mk", "path.mk", "qemu.mk"}
    )
    include_lines.append("")
    contents["config.mk"] = "\n".join(include_lines)

    contents[CONFIG_CACHE.name] = (
        "# Generated by script/py/configure.py. Do not edit this file directly.\n"
        f"cached-config := {config_name}\n"
    )

    CONFIG_CACHE.unlink(missing_ok=True)
    for name, content in contents.items():
        if name == CONFIG_CACHE.name:
            continue
        write_text_if_changed(CACHE_ROOT / name, content)
    remove_stale_cache_fragments(set(contents))
    write_text_if_changed(CONFIG_CACHE, contents[CONFIG_CACHE.name])


def main(arguments: list[str]) -> int:
    try:
        config_name, ignored = parse_arguments(arguments)
        for key in ignored:
            print(
                f"configure.py: warning: {key}= is ignored; use make switch to persist the build selection",
                file=sys.stderr,
            )
        generate(config_name)
    except (OSError, tomllib.TOMLDecodeError, TypeError, ValueError) as error:
        print(f"configure.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
