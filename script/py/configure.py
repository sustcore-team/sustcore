#!/usr/bin/env python3
"""Generate Make configuration files from a selected TOML configuration set."""

from __future__ import annotations

import importlib.util
import os
import sys
import tempfile
from pathlib import Path

import tomllib

import build_ctx
import build_libs
import build_programs
import build_testbench
from libregistry import scan_dependency_owners
import resolve_deps


ROOT = Path(__file__).resolve().parents[2]
CONFIG_ROOT = ROOT / "config"
CACHE_ROOT = ROOT / "script" / ".cache"
CONFIG_CACHE = CACHE_ROOT / ".configure.mk"


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
    values = {}
    for argument in arguments:
        key, separator, value = argument.partition("=")
        if not separator or key not in {"config", "arch", "mode"}:
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value

    config_name = values.get("config") or _read_cached_config() or "default"
    ignored = tuple(key for key in ("arch", "mode") if key in values)
    return config_name, ignored


def load_emitter(name: str):
    path = Path(__file__).resolve().parent / f"{name}.py"
    if not path.is_file():
        raise ValueError(f"no emitter for {name}.toml")

    spec = importlib.util.spec_from_file_location(f"config_emitter_{name}", path)
    if spec is None or spec.loader is None:
        raise ValueError(f"cannot load emitter for {name}.toml")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    sys.path.insert(0, str(path.parent))
    try:
        spec.loader.exec_module(module)
    finally:
        sys.path.pop(0)
    return module


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent, text=True
    )
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as temporary_file:
            temporary_file.write(content)
            temporary_file.flush()
            os.fsync(temporary_file.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


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
