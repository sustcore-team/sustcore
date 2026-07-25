#!/usr/bin/env python3
"""Build the initrd root directory and cpio archive from kernel/initrd.toml."""

from __future__ import annotations

import shutil
import stat
import sys
from pathlib import Path
import tomllib


ROOT = Path(__file__).resolve().parents[2]


def parse_arguments(arguments: list[str]) -> dict[str, str]:
    values: dict[str, str] = {}
    for argument in arguments:
        key, separator, value = argument.partition("=")
        if not separator or key not in {"config", "path-bin", "path-initrd-root", "path-initrd"} or not value:
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value

    missing = {"config", "path-bin", "path-initrd-root", "path-initrd"} - values.keys()
    if missing:
        raise ValueError("missing argument: " + ", ".join(sorted(missing)))
    return values


def parse_list_modules_arguments(arguments: list[str]) -> Path:
    values: dict[str, str] = {}
    for argument in arguments:
        key, separator, value = argument.partition("=")
        if not separator or key != "config" or not value:
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value
    if "config" not in values:
        raise ValueError("missing argument: config")
    return Path(values["config"]).resolve()


def _clean_relative_path(value: object, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a non-empty string")
    path = Path(value.lstrip("/"))
    if path.is_absolute() or ".." in path.parts or str(path) in {"", "."}:
        raise ValueError(f"{field} must be a relative path inside initrd")
    return path


def _read_config(config_path: Path) -> tuple[list[tuple[Path, Path]], list[tuple[str, Path]]]:
    if not config_path.is_file():
        raise ValueError(f"initrd config does not exist: {config_path}")
    with config_path.open("rb") as config_file:
        data = tomllib.load(config_file)

    initrd = data.get("initrd", {})
    if not isinstance(initrd, dict):
        raise ValueError(f"{config_path}: initrd must be a table")

    file_entries = initrd.get("file", [])
    module_entries = initrd.get("module", [])
    if not isinstance(file_entries, list):
        raise ValueError(f"{config_path}: initrd.file must be an array of tables")
    if not isinstance(module_entries, list):
        raise ValueError(f"{config_path}: initrd.module must be an array of tables")

    files: list[tuple[Path, Path]] = []
    for index, entry in enumerate(file_entries):
        if not isinstance(entry, dict):
            raise ValueError(f"{config_path}: initrd.file[{index}] must be a table")
        src = entry.get("src")
        if not isinstance(src, str) or not src:
            raise ValueError(f"{config_path}: initrd.file[{index}].src must be a non-empty string")
        source_path = Path(src)
        if not source_path.is_absolute():
            source_path = ROOT / source_path
        files.append((source_path, _clean_relative_path(entry.get("dst"), f"{config_path}: initrd.file[{index}].dst")))

    modules: list[tuple[str, Path]] = []
    for index, entry in enumerate(module_entries):
        if not isinstance(entry, dict):
            raise ValueError(f"{config_path}: initrd.module[{index}] must be a table")
        mod = entry.get("mod")
        if not isinstance(mod, str) or not mod:
            raise ValueError(f"{config_path}: initrd.module[{index}].mod must be a non-empty string")
        modules.append((mod, _clean_relative_path(entry.get("dst"), f"{config_path}: initrd.module[{index}].dst")))

    return files, modules


def list_modules(config_path: Path) -> list[str]:
    _, modules = _read_config(config_path)
    result: list[str] = []
    seen: set[str] = set()
    for module_id, _ in modules:
        if module_id in seen:
            continue
        seen.add(module_id)
        result.append(module_id)
    return result


def _copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise ValueError(f"initrd source file does not exist: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def _module_source(path_bin: Path, module_id: str) -> Path:
    return path_bin / "module" / f"{module_id}.mod"


def _pad4(size: int) -> bytes:
    return b"\0" * ((4 - (size % 4)) % 4)


def _write_svr4_newc_entry(
    archive,
    name: str,
    inode: int,
    mode: int,
    nlink: int,
    mtime: int,
    data: bytes,
) -> None:
    encoded_name = name.encode("utf-8") + b"\0"
    fields = (
        inode,
        mode,
        0,
        0,
        nlink,
        mtime,
        len(data),
        0,
        0,
        0,
        0,
        len(encoded_name),
        0,
    )
    header = "070701" + "".join(f"{field:08x}" for field in fields)
    archive.write(header.encode("ascii"))
    archive.write(encoded_name)
    archive.write(_pad4(110 + len(encoded_name)))
    archive.write(data)
    archive.write(_pad4(len(data)))


def _write_archive(root: Path, archive_path: Path) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with archive_path.open("wb") as archive:
        inode = 1
        for path in sorted(item for item in root.rglob("*") if item.is_dir()):
            file_stat = path.stat()
            mode = stat.S_IFDIR | stat.S_IMODE(file_stat.st_mode)
            _write_svr4_newc_entry(
                archive,
                path.relative_to(root).as_posix(),
                inode,
                mode,
                2,
                int(file_stat.st_mtime),
                b"",
            )
            inode += 1

        for path in sorted(item for item in root.rglob("*") if item.is_file()):
            file_stat = path.stat()
            mode = stat.S_IFREG | stat.S_IMODE(file_stat.st_mode)
            _write_svr4_newc_entry(
                archive,
                path.relative_to(root).as_posix(),
                inode,
                mode,
                1,
                int(file_stat.st_mtime),
                path.read_bytes(),
            )
            inode += 1

        _write_svr4_newc_entry(archive, "TRAILER!!!", inode, 0, 1, 0, b"")


def build(config_path: Path, path_bin: Path, path_initrd_root: Path, path_initrd: Path) -> None:
    files, modules = _read_config(config_path)

    if path_initrd_root.exists():
        shutil.rmtree(path_initrd_root)
    path_initrd_root.mkdir(parents=True, exist_ok=True)

    for source, destination in files:
        _copy_file(source, path_initrd_root / destination)

    for module_id, destination in modules:
        _copy_file(_module_source(path_bin, module_id), path_initrd_root / destination)

    _write_archive(path_initrd_root, path_initrd)


def main(arguments: list[str]) -> int:
    try:
        if arguments and arguments[0] == "list-modules":
            print(" ".join(list_modules(parse_list_modules_arguments(arguments[1:]))))
            return 0

        values = parse_arguments(arguments)
        build(
            Path(values["config"]).resolve(),
            Path(values["path-bin"]).resolve(),
            Path(values["path-initrd-root"]).resolve(),
            Path(values["path-initrd"]).resolve(),
        )
    except (OSError, tomllib.TOMLDecodeError, ValueError) as error:
        print(f"initrd.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
