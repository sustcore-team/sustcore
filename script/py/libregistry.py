#!/usr/bin/env python3
"""Shared metadata registry for libraries."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import tomllib

import semver

KNOWN_ARCHITECTURES = ("riscv64", "loongarch64")
VALID_ID_PATTERN = re.compile(r"^[A-Za-z0-9_-]+$")


@dataclass(frozen=True)
class LibraryMeta:
    id: str
    version: str
    root: str
    kind: str
    libname: str
    makefile: str
    target: str
    include_c: str
    include_cpp: str
    include_asm: str
    support_archs: tuple[str, ...]
    arch_ldscripts: dict[str, str]
    arch_crt0: dict[str, str]
    arch_crti: dict[str, str]
    arch_crtn: dict[str, str]
    metadata_path: str

    @property
    def archive_path(self) -> str:
        if not self.libname:
            return ""
        return f"$(path-bin)/libs/{self.libname}"

    @property
    def is_header_only(self) -> bool:
        return self.libname == ""

    @property
    def is_c_library(self) -> bool:
        return self.kind == "c-library"


@dataclass(frozen=True)
class OwnerMeta:
    id: str
    root: str
    metadata_path: str
    kind: str
    output: str = ""
    makefile: str = ""
    target: str = ""
    c_library: str = ""
    ldscript: str = ""


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


def normalize_kind(raw_value: object, field: str) -> str:
    if raw_value is None:
        return "library"
    if raw_value not in {"library", "c-library"}:
        raise ValueError(f"{field} must be 'library' or 'c-library'")
    return raw_value


def normalize_relative_path(raw_value: object, field: str, *, required: bool = False) -> str:
    if raw_value is None:
        if required:
            raise ValueError(f"{field} must be a non-empty string")
        return ""
    if not isinstance(raw_value, str) or (required and not raw_value):
        raise ValueError(f"{field} must be a non-empty string")
    if not raw_value:
        return ""
    path = Path(raw_value)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"{field} must be a relative path inside the metadata directory")
    return raw_value


def normalize_output_path(raw_value: object, field: str) -> str:
    if not isinstance(raw_value, str) or not raw_value:
        raise ValueError(f"{field} must be a non-empty string")
    path = Path(raw_value)
    if path.is_absolute() or ".." in path.parts or str(path) in {"", "."}:
        raise ValueError(f"{field} must be a relative path inside the binary directory")
    return raw_value


def normalize_arch_paths(
    metadata_path: Path,
    arch_data: object,
    field: str,
) -> dict[str, str]:
    if arch_data is None:
        return {}
    if not isinstance(arch_data, dict):
        raise ValueError(f"{metadata_path}: arch must be a table")

    result: dict[str, str] = {}
    for arch, values in arch_data.items():
        if not isinstance(arch, str) or not arch:
            raise ValueError(f"{metadata_path}: arch names must be non-empty strings")
        if not isinstance(values, dict):
            raise ValueError(f"{metadata_path}: arch.{arch} must be a table")
        result[arch] = normalize_relative_path(
            values.get(field), f"{metadata_path}: arch.{arch}.{field}"
        )
    return {arch: value for arch, value in result.items() if value}


def _arch_data_for_entry(metadata_path: Path, data: dict, entry: dict, entry_count: int) -> object:
    if "arch" in entry:
        return entry.get("arch")
    if "arch" in data:
        if entry_count != 1:
            raise ValueError(
                f"{metadata_path}: top-level arch table is only supported with one libmeta entry"
            )
        return data.get("arch")
    return None


def parse_kernel_owner(root: Path) -> OwnerMeta:
    kernel_root = (root / "kernel").resolve()
    if not kernel_root.is_dir():
        raise ValueError(f"kernel directory does not exist: {kernel_root}")
    return OwnerMeta(
        id="kernel",
        root=str(kernel_root),
        metadata_path=str(kernel_root / "dependencies.toml"),
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
            kind = normalize_kind(entry.get("kind"), f"{metadata_path}: kind")
            version = entry.get("version")
            if not isinstance(version, str) or not version:
                raise ValueError(f"{metadata_path}: version must be a non-empty string")
            try:
                semver.parse_version(version)
            except ValueError as error:
                raise ValueError(f"{metadata_path}: invalid library version: {error}") from error
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

            arch_data = _arch_data_for_entry(metadata_path, data, entry, len(entries))
            libraries.append(
                LibraryMeta(
                    id=library_id,
                    version=version,
                    root=str(metadata_path.parent.resolve()),
                    kind=kind,
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
                    arch_ldscripts=normalize_arch_paths(metadata_path, arch_data, "ldscript"),
                    arch_crt0=normalize_arch_paths(metadata_path, arch_data, "crt0"),
                    arch_crti=normalize_arch_paths(metadata_path, arch_data, "crti"),
                    arch_crtn=normalize_arch_paths(metadata_path, arch_data, "crtn"),
                    metadata_path=str(metadata_path),
                )
            )

    return libraries


def scan_program_metadata_files(root: Path) -> list[Path]:
    metadata_files = []
    for relative_root in ("module", "program"):
        scan_root = root / relative_root
        if not scan_root.is_dir():
            continue
        metadata_files.extend(sorted(scan_root.rglob("metadata.toml")))
    return metadata_files


def scan_programs(root: Path) -> list[OwnerMeta]:
    programs: list[OwnerMeta] = []
    seen_ids: dict[str, Path] = {}

    for metadata_path in scan_program_metadata_files(root):
        with metadata_path.open("rb") as metadata_file:
            data = tomllib.load(metadata_file)

        entries = data.get("progmeta")
        if not isinstance(entries, list) or not entries:
            raise ValueError(f"{metadata_path}: progmeta must be a non-empty array of tables")

        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"{metadata_path}: each progmeta entry must be a table")

            program_id = validate_global_id(entry.get("id"), f"{metadata_path}: id")
            if program_id in seen_ids:
                raise ValueError(
                    f"duplicate program id {program_id!r}: {seen_ids[program_id]} and {metadata_path}"
                )
            seen_ids[program_id] = metadata_path
            makefile = normalize_relative_path(
                entry.get("makefile"), f"{metadata_path}: makefile", required=True
            )
            makefile_path = (metadata_path.parent / makefile).resolve()
            if not makefile_path.is_file():
                raise ValueError(f"{metadata_path}: makefile does not exist: {makefile}")
            target = entry.get("target")
            if not isinstance(target, str) or not target:
                raise ValueError(f"{metadata_path}: target must be a non-empty string")
            output = normalize_output_path(entry.get("output"), f"{metadata_path}: output")
            c_library = entry.get("c-library", "")
            if not isinstance(c_library, str):
                raise ValueError(f"{metadata_path}: c-library must be a string")

            programs.append(
                OwnerMeta(
                    id=program_id,
                    root=str(metadata_path.parent.resolve()),
                    metadata_path=str(metadata_path),
                    kind="program",
                    output=output,
                    makefile=str(makefile_path),
                    target=target,
                    c_library=c_library,
                    ldscript=normalize_relative_path(entry.get("ldscript"), f"{metadata_path}: ldscript"),
                )
            )

    return programs


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
    for program in scan_programs(root):
        if program.id in seen_ids:
            raise ValueError(
                f"duplicate owner id {program.id!r}: {seen_ids[program.id]} and {program.metadata_path}"
            )
        seen_ids[program.id] = program.metadata_path
        owners.append(program)
    return owners


def libraries_for_arch(root: Path, arch: str) -> list[LibraryMeta]:
    result = []
    for library in scan_libraries(root):
        if library.support_archs and arch not in library.support_archs:
            continue
        result.append(library)
    return result
