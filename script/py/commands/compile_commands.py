#!/usr/bin/env python3
"""Manage target and host compilation databases for clangd."""

from __future__ import annotations

import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

from common.arguments import parse_key_value_arguments
from common.constants import ARCHITECTURES, BUILD_MODES, SANITIZERS


SUPPORTED_ARCHITECTURES = set(ARCHITECTURES)
SUPPORTED_MODES = set(BUILD_MODES)
SUPPORTED_SANITIZERS = set(SANITIZERS)
HOST_TRIPLE_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def parse_arguments(arguments: list[str]) -> tuple[str, dict[str, str]]:
    schemas = {
        "prepare": ({"root", "arch", "mode"}, set()),
        "select": ({"root", "arch", "mode"}, set()),
        "prepare-host": ({"root", "triple", "mode"}, {"sanitize"}),
        "select-host": ({"root", "triple", "mode"}, {"sanitize"}),
        "publish-host": ({"root", "triple", "mode", "source"}, {"sanitize"}),
    }
    if not arguments or arguments[0] not in schemas:
        raise ValueError(
            "expected action 'prepare', 'select', 'prepare-host', "
            "'select-host', or 'publish-host'"
        )

    action = arguments[0]
    required, optional = schemas[action]
    values = parse_key_value_arguments(
        arguments[1:], required | optional, required_keys=required
    )
    return action, values


def validate_selection(build_root: Path, arch: str, mode: str) -> None:
    if not str(build_root):
        raise ValueError("build root must not be empty")
    if arch not in SUPPORTED_ARCHITECTURES:
        raise ValueError(f"unsupported architecture: {arch}")
    if mode not in SUPPORTED_MODES:
        raise ValueError(f"unsupported mode: {mode}")


def validate_host_selection(
    build_root: Path, triple: str, mode: str, sanitize: str = ""
) -> None:
    if not str(build_root):
        raise ValueError("build root must not be empty")
    if not HOST_TRIPLE_PATTERN.fullmatch(triple):
        raise ValueError(f"invalid host triple: {triple!r}")
    if mode not in SUPPORTED_MODES:
        raise ValueError(f"unsupported mode: {mode}")
    if sanitize not in SUPPORTED_SANITIZERS:
        raise ValueError(f"unsupported sanitizer: {sanitize!r}")


def database_path(build_root: Path, arch: str, mode: str) -> Path:
    validate_selection(build_root, arch, mode)
    return build_root / mode / arch / "compile_commands.json"


def host_database_path(
    build_root: Path, triple: str, mode: str, sanitize: str = ""
) -> Path:
    validate_host_selection(build_root, triple, mode, sanitize)
    parent = build_root / mode / "host" / triple
    if sanitize:
        parent = parent / "sanitize" / sanitize.replace(",", "-")
    return parent / "compile_commands.json"


def prepare_database(build_root: Path, arch: str, mode: str) -> Path:
    path = database_path(build_root, arch, mode)
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def prepare_host_database(
    build_root: Path, triple: str, mode: str, sanitize: str = ""
) -> Path:
    path = host_database_path(build_root, triple, mode, sanitize)
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def _validate_database(path: Path) -> None:
    try:
        with path.open("r", encoding="utf-8") as database_file:
            data = json.load(database_file)
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid compilation database JSON: {path}: {error}") from error
    if not isinstance(data, list):
        raise ValueError(f"compilation database must contain a JSON array: {path}")


def publish_database(source: Path, target: Path) -> Path:
    if not source.is_file():
        raise ValueError(f"compilation database does not exist: {source}")
    _validate_database(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and not target.is_file():
        raise ValueError(f"refusing to replace non-file: {target}")
    os.replace(source, target)
    return target


def _remove_current_database(current_path: Path) -> None:
    if current_path.is_symlink() or current_path.is_file():
        current_path.unlink()
    elif current_path.exists():
        raise ValueError(f"refusing to replace non-file: {current_path}")


def _select_path(build_root: Path, target: Path) -> tuple[Path, Path, bool]:
    build_root.mkdir(parents=True, exist_ok=True)
    current_path = build_root / "compile_commands.json"

    if not target.is_file():
        _remove_current_database(current_path)
        return current_path, target, False

    try:
        _validate_database(target)
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


def select_database(
    build_root: Path, arch: str, mode: str
) -> tuple[Path, Path, bool]:
    return _select_path(build_root, database_path(build_root, arch, mode))


def select_host_database(
    build_root: Path, triple: str, mode: str, sanitize: str = ""
) -> tuple[Path, Path, bool]:
    return _select_path(
        build_root, host_database_path(build_root, triple, mode, sanitize)
    )


def _print_selection(current_path: Path, target: Path, selected: bool) -> None:
    print(f"Selected compilation database: {target}")
    if selected:
        print(f"clangd database: {current_path}")
    else:
        print(
            f"compile_commands.py: warning: database has not been generated: {target}",
            file=sys.stderr,
        )


def main(arguments: list[str]) -> int:
    try:
        action, values = parse_arguments(arguments)
        root_value = values["root"]
        if not root_value:
            raise ValueError("build root must not be empty")
        build_root = Path(root_value)

        if action in {"prepare", "select"}:
            arch = values["arch"]
            mode = values["mode"]
            if action == "select" and (not arch or not mode):
                print(
                    "compile_commands.py: warning: current configuration is incomplete; "
                    "run 'make switch' and 'make configure' before selecting a database",
                    file=sys.stderr,
                )
                return 0
            if action == "prepare":
                print(f"Compilation database: {prepare_database(build_root, arch, mode)}")
            else:
                _print_selection(*select_database(build_root, arch, mode))
            return 0

        triple = values["triple"]
        mode = values["mode"]
        sanitize = values.get("sanitize", "")
        if action == "prepare-host":
            print(
                "Compilation database: "
                f"{prepare_host_database(build_root, triple, mode, sanitize)}"
            )
        elif action == "select-host":
            _print_selection(
                *select_host_database(build_root, triple, mode, sanitize)
            )
        else:
            target = host_database_path(build_root, triple, mode, sanitize)
            print(f"Published compilation database: {publish_database(Path(values['source']), target)}")
    except (OSError, ValueError) as error:
        print(f"compile_commands.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
