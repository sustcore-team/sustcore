#!/usr/bin/env python3
"""Manage architecture- and mode-specific compilation databases for clangd."""

from __future__ import annotations

import json
import os
import shutil
import sys
import tempfile
from pathlib import Path


SUPPORTED_ARCHITECTURES = {"riscv64", "loongarch64"}
SUPPORTED_MODES = {"debug", "release"}


def parse_arguments(arguments: list[str]) -> tuple[str, dict[str, str]]:
    if not arguments or arguments[0] not in {"prepare", "select"}:
        raise ValueError("expected action 'prepare' or 'select'")

    action = arguments[0]
    values: dict[str, str] = {}
    for argument in arguments[1:]:
        key, separator, value = argument.partition("=")
        if not separator or key not in {"root", "arch", "mode"}:
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value

    missing = {"root", "arch", "mode"} - values.keys()
    if missing:
        raise ValueError("missing argument: " + ", ".join(sorted(missing)))
    return action, values


def validate_selection(build_root: Path, arch: str, mode: str) -> None:
    if not str(build_root):
        raise ValueError("build root must not be empty")
    if arch not in SUPPORTED_ARCHITECTURES:
        raise ValueError(f"unsupported architecture: {arch}")
    if mode not in SUPPORTED_MODES:
        raise ValueError(f"unsupported mode: {mode}")


def database_path(
    build_root: Path, arch: str, mode: str
) -> Path:
    validate_selection(build_root, arch, mode)
    return build_root / mode / arch / "compile_commands.json"


def prepare_database(
    build_root: Path, arch: str, mode: str
) -> Path:
    path = database_path(build_root, arch, mode)
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def _remove_current_database(current_path: Path) -> None:
    if current_path.is_symlink() or current_path.is_file():
        current_path.unlink()
    elif current_path.exists():
        raise ValueError(f"refusing to replace non-file: {current_path}")


def select_database(
    build_root: Path, arch: str, mode: str
) -> tuple[Path, Path, bool]:
    target = database_path(build_root, arch, mode)
    build_root.mkdir(parents=True, exist_ok=True)
    current_path = build_root / "compile_commands.json"

    if not target.is_file():
        _remove_current_database(current_path)
        return current_path, target, False

    try:
        with target.open("r", encoding="utf-8") as database_file:
            data = json.load(database_file)
        if not isinstance(data, list):
            raise ValueError(f"compilation database must contain a JSON array: {target}")
    except (OSError, ValueError):
        _remove_current_database(current_path)
        raise

    if current_path.exists() and not current_path.is_file():
        raise ValueError(f"refusing to replace non-file: {current_path}")

    fd, temporary_name = tempfile.mkstemp(
        prefix=".compile_commands.json.", dir=build_root
    )
    try:
        with target.open("rb") as source, os.fdopen(fd, "wb") as destination:
            shutil.copyfileobj(source, destination)
            destination.flush()
            os.fsync(destination.fileno())
            os.fchmod(destination.fileno(), target.stat().st_mode & 0o777)
        os.replace(temporary_name, current_path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    return current_path, target, True


def main(arguments: list[str]) -> int:
    try:
        action, values = parse_arguments(arguments)
        root_value = values["root"]
        arch = values["arch"]
        mode = values["mode"]
        if not root_value:
            raise ValueError("build root must not be empty")
        build_root = Path(root_value)
        if action == "prepare":
            path = prepare_database(build_root, arch, mode)
            print(f"Compilation database: {path}")
        else:
            if not arch or not mode:
                print(
                    "compile_commands.py: warning: current configuration is incomplete; "
                    "run 'make switch' and 'make configure' before selecting a database",
                    file=sys.stderr,
                )
                return 0
            current_path, target, selected = select_database(
                build_root, arch, mode
            )
            print(f"Selected compilation database: {target}")
            if not selected:
                print(
                    f"compile_commands.py: warning: database has not been generated: {target}",
                    file=sys.stderr,
                )
            else:
                print(f"clangd database: {current_path}")
    except (OSError, ValueError) as error:
        print(f"compile_commands.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
