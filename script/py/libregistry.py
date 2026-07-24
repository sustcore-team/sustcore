#!/usr/bin/env python3
"""Shared metadata registry for libraries."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import tomllib

KNOWN_ARCHITECTURES = ("riscv64", "loongarch64")
VALID_ID_PATTERN = re.compile(r"^[A-Za-z0-9_-]+$")


@dataclass(frozen=True)
class LibraryMeta:
    id: str
    version: str
    root: str
    libname: str
    makefile: str
    target: str
    include_c: str
    include_cpp: str
    include_asm: str
    support_archs: tuple[str, ...]
    metadata_path: str

    @property
    def archive_path(self) -> str:
        if not self.libname:
            return ""
        return f"$(path-bin)/libs/$(arch)/{self.libname}"

    @property
    def is_header_only(self) -> bool:
        return self.libname == ""


@dataclass(frozen=True)
class OwnerMeta:
    id: str
    root: str
    metadata_path: str
    kind: str


def scan_metadata_files(root: Path) -> list[Path]:
    metadata_files = []
    for relative_root in ("libs", "third_party/libs"):
        scan_root = root / relative_root
        if not scan_root.is_dir():
            continue
        metadata_files.extend(sorted(scan_root.rglob("metadata.toml")))
    return metadata_files


def validate_global_id(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a non-empty string")
    if not VALID_ID_PATTERN.fullmatch(value):
        raise ValueError(
            f"{field} must match {VALID_ID_PATTERN.pattern!r}"
        )
    return value


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


def normalize_libname(raw_value: object, field: str) -> str:
    if not isinstance(raw_value, str):
        raise ValueError(f"{field} must be a string")
    if not raw_value:
        return ""
    libname = Path(raw_value)
    if libname.name != raw_value:
        raise ValueError(f"{field} must be a file name without directory components")
    if not raw_value.endswith(".a"):
        raise ValueError(f"{field} must end with '.a'")
    return raw_value


def parse_kernel_owner(root: Path) -> OwnerMeta:
    metadata_path = root / "kernel" / "metadata.toml"
    if not metadata_path.is_file():
        raise ValueError(f"kernel metadata does not exist: {metadata_path}")

    with metadata_path.open("rb") as metadata_file:
        data = tomllib.load(metadata_file)

    entries = data.get("kernelmeta")
    if not isinstance(entries, list) or not entries:
        raise ValueError(f"{metadata_path}: kernelmeta must be a non-empty array of tables")
    if len(entries) != 1 or not isinstance(entries[0], dict):
        raise ValueError(f"{metadata_path}: kernelmeta must contain exactly one table")

    kernel_id = validate_global_id(entries[0].get("id"), f"{metadata_path}: id")
    return OwnerMeta(
        id=kernel_id,
        root=str(metadata_path.parent.resolve()),
        metadata_path=str(metadata_path),
        kind="kernel",
    )


def scan_libraries(root: Path) -> list[LibraryMeta]:
    libraries: list[LibraryMeta] = []

    for metadata_path in scan_metadata_files(root):
        with metadata_path.open("rb") as metadata_file:
            data = tomllib.load(metadata_file)

        entries = data.get("libmeta")
        if not isinstance(entries, list) or not entries:
            raise ValueError(f"{metadata_path}: libmeta must be a non-empty array of tables")

        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"{metadata_path}: each libmeta entry must be a table")

            library_id = validate_global_id(entry.get("id"), f"{metadata_path}: id")
            version = entry.get("version")
            if not isinstance(version, str) or not version:
                raise ValueError(f"{metadata_path}: version must be a non-empty string")
            libname = normalize_libname(entry.get("libname"), f"{metadata_path}: libname")

            raw_makefile = entry.get("makefile", "")
            raw_target = entry.get("target", "")
            if not isinstance(raw_makefile, str):
                raise ValueError(f"{metadata_path}: makefile must be a string")
            if not isinstance(raw_target, str):
                raise ValueError(f"{metadata_path}: target must be a string")

            makefile_path = ""
            if libname:
                if not raw_makefile:
                    raise ValueError(f"{metadata_path}: makefile must be non-empty for non-header-only libraries")
                if not raw_target:
                    raise ValueError(f"{metadata_path}: target must be non-empty for non-header-only libraries")
                resolved_makefile = (metadata_path.parent / raw_makefile).resolve()
                if not resolved_makefile.is_file():
                    raise ValueError(
                        f"{metadata_path}: makefile does not exist: {raw_makefile}"
                    )
                makefile_path = str(resolved_makefile)
            elif raw_makefile:
                resolved_makefile = (metadata_path.parent / raw_makefile).resolve()
                if not resolved_makefile.is_file():
                    raise ValueError(
                        f"{metadata_path}: makefile does not exist: {raw_makefile}"
                    )
                makefile_path = str(resolved_makefile)

            libraries.append(
                LibraryMeta(
                    id=library_id,
                    version=version,
                    root=str(metadata_path.parent.resolve()),
                    libname=libname,
                    makefile=makefile_path,
                    target=raw_target,
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


def scan_dependency_owners(root: Path) -> list[OwnerMeta]:
    kernel = parse_kernel_owner(root)
    owners = [kernel]
    seen_ids: dict[str, str] = {kernel.id: kernel.metadata_path}

    for library in scan_libraries(root):
        if library.id in seen_ids:
            raise ValueError(
                f"duplicate owner id {library.id!r}: {seen_ids[library.id]} and {library.metadata_path}"
            )
        seen_ids[library.id] = library.metadata_path
        owners.append(
            OwnerMeta(
                id=library.id,
                root=library.root,
                metadata_path=library.metadata_path,
                kind="library",
            )
        )
    return owners


def libraries_for_arch(root: Path, arch: str) -> list[LibraryMeta]:
    result = []
    for library in scan_libraries(root):
        if library.support_archs and arch not in library.support_archs:
            continue
        result.append(library)
    return result
