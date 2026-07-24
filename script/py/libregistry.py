#!/usr/bin/env python3
"""Shared metadata registry for libraries."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import tomllib

KNOWN_ARCHITECTURES = ("riscv64", "loongarch64")


@dataclass(frozen=True)
class LibraryMeta:
    id: str
    version: str
    makefile: str
    target: str
    include_c: str
    include_cpp: str
    include_asm: str
    support_archs: tuple[str, ...]
    metadata_path: str


def scan_metadata_files(root: Path) -> list[Path]:
    metadata_files = []
    for relative_root in ("libs", "third_party/libs"):
        scan_root = root / relative_root
        if not scan_root.is_dir():
            continue
        metadata_files.extend(sorted(scan_root.rglob("metadata.toml")))
    return metadata_files


def normalize_include_flags(base_dir: Path, raw_value: object, field: str) -> str:
    if raw_value is None:
        return ""
    if not isinstance(raw_value, list) or not all(isinstance(item, str) for item in raw_value):
        raise ValueError(f"{field} must be an array of strings")
    flags = [f"-I{(base_dir / item).resolve()}" for item in raw_value]
    return " ".join(flags)


def normalize_support_archs(raw_value: object, field: str) -> tuple[str, ...]:
    if raw_value is None:
        return ()
    if not isinstance(raw_value, list) or not all(isinstance(item, str) and item for item in raw_value):
        raise ValueError(f"{field} must be an array of non-empty strings")
    return tuple(raw_value)


def scan_libraries(root: Path) -> list[LibraryMeta]:
    libraries: list[LibraryMeta] = []
    seen_ids: dict[str, Path] = {}

    for metadata_path in scan_metadata_files(root):
        with metadata_path.open("rb") as metadata_file:
            data = tomllib.load(metadata_file)

        entries = data.get("libmeta")
        if not isinstance(entries, list) or not entries:
            raise ValueError(f"{metadata_path}: libmeta must be a non-empty array of tables")

        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"{metadata_path}: each libmeta entry must be a table")

            for field in ("id", "makefile", "target", "version"):
                value = entry.get(field)
                if not isinstance(value, str) or not value:
                    raise ValueError(f"{metadata_path}: {field} must be a non-empty string")

            library_id = entry["id"]
            if library_id in seen_ids:
                raise ValueError(
                    f"duplicate library id {library_id!r}: "
                    f"{seen_ids[library_id]} and {metadata_path}"
                )
            seen_ids[library_id] = metadata_path

            makefile_path = (metadata_path.parent / entry["makefile"]).resolve()
            if not makefile_path.is_file():
                raise ValueError(
                    f"{metadata_path}: makefile does not exist: {entry['makefile']}"
                )

            libraries.append(
                LibraryMeta(
                    id=library_id,
                    version=entry["version"],
                    makefile=str(makefile_path),
                    target=entry["target"],
                    include_c=normalize_include_flags(
                        metadata_path.parent, entry.get("include-c"), f"{metadata_path}: include-c"
                    ),
                    include_cpp=normalize_include_flags(
                        metadata_path.parent, entry.get("include-cpp"), f"{metadata_path}: include-cpp"
                    ),
                    include_asm=normalize_include_flags(
                        metadata_path.parent, entry.get("include-asm"), f"{metadata_path}: include-asm"
                    ),
                    support_archs=normalize_support_archs(
                        entry.get("support-archs"), f"{metadata_path}: support-archs"
                    ),
                    metadata_path=str(metadata_path),
                )
            )

    return libraries


def libraries_for_arch(root: Path, arch: str) -> list[LibraryMeta]:
    result = []
    for library in scan_libraries(root):
        if library.support_archs and arch not in library.support_archs:
            continue
        result.append(library)
    return result
