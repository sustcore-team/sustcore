#!/usr/bin/env python3
"""Resolve owner dependencies against the generated library registry."""

from __future__ import annotations

from collections.abc import Iterable
import os
from pathlib import Path
import sys
import tempfile
import tomllib

from libregistry import (
    KNOWN_ARCHITECTURES,
    KNOWN_ENVIRONMENTS,
    LibraryMeta,
    OwnerMeta,
    libraries_for_environment,
    scan_dependency_owners,
)
import semver


DependencyEntry = tuple[str, str]


def _select_library(
    library_index: dict[str, list[LibraryMeta]],
    dep_name: str,
    dep_version: str,
    source_path: Path,
    environment: str,
    arch: str,
) -> LibraryMeta:
    candidates = library_index.get(dep_name, [])
    if not candidates:
        raise ValueError(
            f"{source_path}: dependency {dep_name!r} is not registered for "
            f"environment {environment!r}, architecture {arch!r}"
        )

    matching = [candidate for candidate in candidates if semver.matches(candidate.version, dep_version)]
    if not matching:
        raise ValueError(
            f"{source_path}: dependency {dep_name!r} version expression {dep_version!r} matched no libraries for architecture {arch!r}"
        )
    if len(matching) != 1:
        raise ValueError(
            f"{source_path}: dependency {dep_name!r} version expression {dep_version!r} matched multiple libraries"
        )
    return matching[0]


def _parse_dep_entries(entries: object, source_path: Path) -> list[DependencyEntry]:
    if entries is None:
        return []
    if not isinstance(entries, list):
        raise ValueError(f"{source_path}: dependencies must be an array of tables")

    result = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError(f"{source_path}: dependency entries must be tables")
        dep_name = entry.get("lib")
        dep_version = entry.get("version")
        if not isinstance(dep_name, str) or not dep_name:
            raise ValueError(f"{source_path}: dependency lib must be a non-empty string")
        if not isinstance(dep_version, str) or not dep_version:
            raise ValueError(f"{source_path}: dependency version must be a non-empty string")
        result.append((dep_name, dep_version))
    return result


def _read_dependency_file(target_path: Path) -> tuple[list[DependencyEntry], dict[str, list[DependencyEntry]]]:
    if not target_path.is_file():
        return [], {}

    with target_path.open("rb") as dependency_file:
        data = tomllib.load(dependency_file)

    common_deps = _parse_dep_entries(data.get("dependencies"), target_path)
    arch_entries: dict[str, list[DependencyEntry]] = {}
    for key, value in data.items():
        if key == "dependencies":
            continue
        if not isinstance(value, dict):
            raise ValueError(f"{target_path}: architecture section {key!r} must be a table")
        arch_entries[key] = _parse_dep_entries(value.get("dependencies"), target_path)
    return common_deps, arch_entries


def _merge_entries(
    source_path: Path, *entry_groups: list[DependencyEntry]
) -> list[DependencyEntry]:
    merged: list[DependencyEntry] = []
    versions: dict[str, str] = {}
    for entries in entry_groups:
        for dep_name, dep_version in entries:
            previous = versions.get(dep_name)
            if previous is not None:
                if previous != dep_version:
                    raise ValueError(
                        f"{source_path}: dependency {dep_name!r} uses conflicting "
                        f"version expressions {previous!r} and {dep_version!r}"
                    )
                continue
            versions[dep_name] = dep_version
            merged.append((dep_name, dep_version))
    return merged


def dependency_entries(
    target_path: Path, environment: str, arch: str | None
) -> list[DependencyEntry]:
    if environment not in KNOWN_ENVIRONMENTS:
        raise ValueError(f"unsupported environment: {environment!r}")
    common, sections = _read_dependency_file(target_path)
    environment_entries = sections.get(environment, [])
    arch_entries = sections.get(arch, []) if arch else []
    return _merge_entries(target_path, common, environment_entries, arch_entries)


def _library_index(
    root: Path, environment: str, arch: str
) -> dict[str, list[LibraryMeta]]:
    library_index: dict[str, list[LibraryMeta]] = {}
    for library in libraries_for_environment(root, environment, arch):
        library_index.setdefault(library.id, []).append(library)
    return library_index


def _stable_unique(items: Iterable[str]) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for item in items:
        if not item or item in seen:
            continue
        seen.add(item)
        result.append(item)
    return result


def _merge_required(
    destination: dict[str, str], source: dict[str, str], source_path: Path
) -> None:
    for dep_name, dep_version in source.items():
        previous = destination.get(dep_name)
        if previous is not None and previous != dep_version:
            raise ValueError(
                f"{source_path}: transitive dependency {dep_name!r} uses conflicting "
                f"version expressions {previous!r} and {dep_version!r}"
            )
        destination.setdefault(dep_name, dep_version)


def _collect_required_ids(
    root: Path,
    library_index: dict[str, list[LibraryMeta]],
    libraries: list[LibraryMeta],
    environment: str,
    arch: str,
    cache: dict[tuple[str, str, str], dict[str, str]],
    active_stack: set[tuple[str, str, str]],
) -> dict[str, str]:
    required: dict[str, str] = {}

    for library in libraries:
        cache_key = (library.id, environment, arch)
        if cache_key in cache:
            child_required = cache[cache_key]
        else:
            if cache_key in active_stack:
                raise ValueError(
                    f"cyclic library dependency detected while resolving {library.id!r} "
                    f"for environment {environment!r}, architecture {arch!r}"
                )
            active_stack.add(cache_key)
            dep_path = Path(library.root) / "dependencies.toml"
            child_entries = dependency_entries(dep_path, environment, arch)
            child_required = {dep_name: dep_version for dep_name, dep_version in child_entries}
            resolved_children = [
                _select_library(
                    library_index, dep_name, dep_version, dep_path, environment, arch
                )
                for dep_name, dep_version in child_entries
            ]
            _merge_required(
                child_required,
                _collect_required_ids(
                    root, library_index, resolved_children, environment, arch, cache, active_stack
                ),
                dep_path,
            )
            cache[cache_key] = child_required
            active_stack.remove(cache_key)

        _merge_required(required, child_required, Path(library.root) / "dependencies.toml")

    return required


def _resolve_scope(
    root: Path,
    owner: OwnerMeta,
    dep_entries: list[DependencyEntry],
    environment: str,
    arch: str,
    scope_name: str,
) -> list[LibraryMeta]:
    library_index = _library_index(root, environment, arch)
    dependency_path = Path(owner.root) / "dependencies.toml"
    resolved = [
        _select_library(
            library_index, dep_name, dep_version, dependency_path, environment, arch
        )
        for dep_name, dep_version in dep_entries
    ]

    declared_ids = {dep_name for dep_name, _ in dep_entries}
    cache: dict[tuple[str, str, str], dict[str, str]] = {}
    required_ids = _collect_required_ids(
        root, library_index, resolved, environment, arch, cache, set()
    )

    missing = {dep_name: dep_version for dep_name, dep_version in required_ids.items() if dep_name not in declared_ids}
    if missing:
        suggestions = ", ".join(
            f'{{ lib = "{dep_name}", version = "{dep_version}" }}'
            for dep_name, dep_version in sorted(missing.items())
        )
        raise ValueError(
            f"{dependency_path}: owner {owner.id!r} is missing transitive dependencies in scope {scope_name!r}: {suggestions}"
        )
    return resolved


def _archive_path_for_arch(library: LibraryMeta, arch: str) -> str:
    if not library.libname:
        return ""
    return f"$(path-bin)/libs/{library.libname}"


def _object_path_for_crt(library: LibraryMeta, relative_object: str) -> str:
    if not relative_object:
        return ""
    return f"$(path-obj)/libs/{library.id}/{relative_object}"


SCOPE_FIELDS = (
    "dep-ids",
    "dep-archives",
    "includes-c",
    "includes-cpp",
    "includes-asm",
)

C_LIBRARY_FIELDS = (
    "c-library-id",
    "c-library-archive",
    "c-library-includes-c",
    "c-library-includes-cpp",
    "c-library-includes-asm",
    "c-library-ldscript",
    "c-library-crt0",
    "c-library-crti",
    "c-library-crtn",
)


def _scope_values(libraries: list[LibraryMeta], arch: str) -> dict[str, str]:
    return {
        "dep-ids": " ".join(_stable_unique(library.id for library in libraries)),
        "dep-archives": " ".join(
            _stable_unique(
                _archive_path_for_arch(library, arch)
                for library in libraries
                if library.libname
            )
        ),
        "includes-c": " ".join(
            _stable_unique(library.include_c for library in libraries)
        ),
        "includes-cpp": " ".join(
            _stable_unique(library.include_cpp for library in libraries)
        ),
        "includes-asm": " ".join(
            _stable_unique(library.include_asm for library in libraries)
        ),
    }


def _emit_scope_condition(
    owner_id: str, selector: str, libraries: list[LibraryMeta], arch: str
) -> list[str]:
    values = _scope_values(libraries, arch)
    return [
        f"{owner_id}-{field}-{selector} += {values[field]}"
        for field in SCOPE_FIELDS
        if values[field]
    ] + [""]


def _emit_scope_final(owner_id: str) -> list[str]:
    return [
        f"{owner_id}-{field} := $(strip $({owner_id}-{field}-y))"
        for field in SCOPE_FIELDS
    ] + [""]


def _libraries_for_entries(
    entries: list[DependencyEntry], resolved: list[LibraryMeta]
) -> list[LibraryMeta]:
    by_id = {library.id: library for library in resolved}
    return [by_id[dep_name] for dep_name, _ in entries]


def _without_claimed(
    entries: list[DependencyEntry], claimed: set[str]
) -> list[DependencyEntry]:
    selected = []
    for entry in entries:
        if entry[0] in claimed:
            continue
        claimed.add(entry[0])
        selected.append(entry)
    return selected


def _require_arch_value(values: dict[str, str], library: LibraryMeta, arch: str, field: str) -> str:
    value = values.get(arch, "")
    if not value:
        raise ValueError(
            f"{library.metadata_path}: c-library {library.id!r} is missing arch.{arch}.{field}"
        )
    return value


def _select_c_library(
    owner: OwnerMeta,
    libraries: list[LibraryMeta],
    arch: str,
    scope_name: str,
) -> LibraryMeta | None:
    if owner.kind != "program" or not owner.c_library:
        return None

    selected = [library for library in libraries if library.id == owner.c_library]
    if not selected:
        dependency_path = Path(owner.root) / "dependencies.toml"
        raise ValueError(
            f"{dependency_path}: program {owner.id!r} selects c-library {owner.c_library!r} "
            f"but does not declare it as a direct dependency in scope {scope_name!r}"
        )
    if len(selected) != 1:
        raise ValueError(
            f"{owner.metadata_path}: c-library {owner.c_library!r} resolved ambiguously in scope {scope_name!r}"
        )

    library = selected[0]
    if not library.is_c_library:
        raise ValueError(
            f"{owner.metadata_path}: program {owner.id!r} selects {owner.c_library!r} as c-library, "
            f"but {library.metadata_path} has kind {library.kind!r}"
        )
    if not owner.ldscript:
        _require_arch_value(library.arch_ldscripts, library, arch, "ldscript")
    _require_arch_value(library.arch_crt0, library, arch, "crt0")
    _require_arch_value(library.arch_crti, library, arch, "crti")
    _require_arch_value(library.arch_crtn, library, arch, "crtn")
    return library


def _c_library_values(
    owner: OwnerMeta, library: LibraryMeta | None, arch: str
) -> dict[str, str]:
    if library is None:
        return {field: "" for field in C_LIBRARY_FIELDS}

    if owner.ldscript:
        ldscript = str((Path(owner.root) / owner.ldscript).resolve())
    else:
        ldscript = str((Path(library.root) / _require_arch_value(library.arch_ldscripts, library, arch, "ldscript")).resolve())

    return {
        "c-library-id": library.id,
        "c-library-archive": _archive_path_for_arch(library, arch),
        "c-library-includes-c": library.include_c,
        "c-library-includes-cpp": library.include_cpp,
        "c-library-includes-asm": library.include_asm,
        "c-library-ldscript": ldscript,
        "c-library-crt0": _object_path_for_crt(
            library, _require_arch_value(library.arch_crt0, library, arch, "crt0")
        ),
        "c-library-crti": _object_path_for_crt(
            library, _require_arch_value(library.arch_crti, library, arch, "crti")
        ),
        "c-library-crtn": _object_path_for_crt(
            library, _require_arch_value(library.arch_crtn, library, arch, "crtn")
        ),
    }


def _emit_c_library_condition(
    owner: OwnerMeta, selector: str, library: LibraryMeta | None, arch: str
) -> list[str]:
    values = _c_library_values(owner, library, arch)
    return [
        f"{owner.id}-{field}-{selector} := {values[field]}"
        for field in C_LIBRARY_FIELDS
    ] + [""]


def _emit_c_library_final(owner_id: str) -> list[str]:
    return [
        f"{owner_id}-{field} := $(strip $({owner_id}-{field}-y))"
        for field in C_LIBRARY_FIELDS
    ] + [""]


def _emit_program_lines(owner: OwnerMeta) -> list[str]:
    if owner.kind != "program":
        return []
    ldscript = str((Path(owner.root) / owner.ldscript).resolve()) if owner.ldscript else ""
    return [
        f"{owner.id}-ldscript := {ldscript}",
        "",
    ]


def emit(
    root: Path,
    owner: OwnerMeta,
    current_arch: str | None = None,
    environment: str = "freestanding",
) -> str:
    if environment not in KNOWN_ENVIRONMENTS:
        raise ValueError(f"unsupported environment: {environment!r}")
    dependency_path = Path(owner.root) / "dependencies.toml"
    common_deps, arch_entries = _read_dependency_file(dependency_path)

    lines = [
        f"# Generated by script/py/resolve_deps.py for {owner.id} ({environment}). Do not edit this file directly.",
        "",
    ]
    lines.extend(_emit_program_lines(owner))

    if environment == "host":
        if not current_arch:
            raise ValueError(
                "host dependency resolution requires the validated host architecture"
            )
        arches = (current_arch,)
    else:
        arches = KNOWN_ARCHITECTURES

    resolved_by_arch: dict[str, list[LibraryMeta]] = {}
    for arch in arches:
        entries = dependency_entries(dependency_path, environment, arch)
        resolved_by_arch[arch] = _resolve_scope(
            root, owner, entries, environment, arch, f"{environment}/{arch}"
        )

    representative_arch = arches[0]
    claimed = set()
    common_entries = _without_claimed(common_deps, claimed)
    environment_entries = _without_claimed(
        arch_entries.get(environment, []), claimed
    )
    lines.extend(
        _emit_scope_condition(
            owner.id,
            "y",
            _libraries_for_entries(
                common_entries, resolved_by_arch[representative_arch]
            ),
            representative_arch,
        )
    )
    lines.extend(
        _emit_scope_condition(
            owner.id,
            f"$(is-{environment})",
            _libraries_for_entries(
                environment_entries, resolved_by_arch[representative_arch]
            ),
            representative_arch,
        )
    )

    base_claimed = set(claimed)
    for arch in arches:
        arch_claimed = set(base_claimed)
        selected_entries = _without_claimed(
            arch_entries.get(arch, []), arch_claimed
        )
        lines.extend(
            _emit_scope_condition(
                owner.id,
                f"$(is-{arch})",
                _libraries_for_entries(selected_entries, resolved_by_arch[arch]),
                arch,
            )
        )
    lines.extend(_emit_scope_final(owner.id))

    if owner.kind == "program":
        for arch in arches:
            c_library = _select_c_library(
                owner,
                resolved_by_arch[arch],
                arch,
                f"{environment}/{arch}",
            )
            lines.extend(
                _emit_c_library_condition(
                    owner, f"$(is-{arch})", c_library, arch
                )
            )
        lines.extend(_emit_c_library_final(owner.id))

    return "\n".join(lines)


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent, text=True)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output_file:
            output_file.write(content)
            output_file.flush()
            os.fsync(output_file.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def main(arguments: list[str]) -> int:
    try:
        values: dict[str, str] = {}
        for argument in arguments:
            key, separator, value = argument.partition("=")
            if not separator or key not in {"root", "owner", "environment", "arch", "output"} or not value:
                raise ValueError(f"invalid argument: {argument}")
            values[key] = value
        missing = {"root", "owner", "environment", "output"} - set(values)
        if missing:
            raise ValueError("missing arguments: " + ", ".join(sorted(missing)))
        root = Path(values["root"]).resolve()
        owners = {owner.id: owner for owner in scan_dependency_owners(root)}
        owner_id = values["owner"]
        if owner_id not in owners:
            raise ValueError(f"unknown dependency owner: {owner_id!r}")
        content = emit(
            root,
            owners[owner_id],
            values.get("arch"),
            values["environment"],
        )
        write_text_if_changed(Path(values["output"]), content)
    except (OSError, tomllib.TOMLDecodeError, ValueError) as error:
        print(f"resolve_deps.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
