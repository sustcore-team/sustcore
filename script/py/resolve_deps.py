#!/usr/bin/env python3
"""Resolve owner dependencies against the generated library registry."""

from __future__ import annotations

from collections.abc import Iterable
from pathlib import Path
import tomllib

from libregistry import KNOWN_ARCHITECTURES, LibraryMeta, OwnerMeta, libraries_for_arch
import semver


DependencyEntry = tuple[str, str]


def _select_library(
    library_index: dict[str, list[LibraryMeta]],
    dep_name: str,
    dep_version: str,
    source_path: Path,
    arch: str,
) -> LibraryMeta:
    candidates = library_index.get(dep_name, [])
    if not candidates:
        raise ValueError(
            f"{source_path}: dependency {dep_name!r} is not registered for architecture {arch!r}"
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


def _library_index(root: Path, arch: str) -> dict[str, list[LibraryMeta]]:
    library_index: dict[str, list[LibraryMeta]] = {}
    for library in libraries_for_arch(root, arch):
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


def _collect_required_ids(
    root: Path,
    library_index: dict[str, list[LibraryMeta]],
    libraries: list[LibraryMeta],
    arch: str,
    cache: dict[tuple[str, str], dict[str, str]],
    active_stack: set[tuple[str, str]],
) -> dict[str, str]:
    required: dict[str, str] = {}

    for library in libraries:
        cache_key = (library.id, arch)
        if cache_key in cache:
            child_required = cache[cache_key]
        else:
            if cache_key in active_stack:
                raise ValueError(
                    f"cyclic library dependency detected while resolving {library.id!r} for architecture {arch!r}"
                )
            active_stack.add(cache_key)
            dep_path = Path(library.root) / "dependencies.toml"
            common_deps, arch_entries = _read_dependency_file(dep_path)
            child_entries = common_deps + arch_entries.get(arch, [])
            child_required = {dep_name: dep_version for dep_name, dep_version in child_entries}
            resolved_children = [
                _select_library(library_index, dep_name, dep_version, dep_path, arch)
                for dep_name, dep_version in child_entries
            ]
            child_required.update(
                _collect_required_ids(
                    root,
                    library_index,
                    resolved_children,
                    arch,
                    cache,
                    active_stack,
                )
            )
            cache[cache_key] = child_required
            active_stack.remove(cache_key)

        for dep_name, dep_version in child_required.items():
            required.setdefault(dep_name, dep_version)

    return required


def _resolve_scope(
    root: Path,
    owner: OwnerMeta,
    dep_entries: list[DependencyEntry],
    arch: str,
    scope_name: str,
) -> list[LibraryMeta]:
    library_index = _library_index(root, arch)
    dependency_path = Path(owner.root) / "dependencies.toml"
    resolved = [
        _select_library(library_index, dep_name, dep_version, dependency_path, arch)
        for dep_name, dep_version in dep_entries
    ]

    declared_ids = {dep_name for dep_name, _ in dep_entries}
    cache: dict[tuple[str, str], dict[str, str]] = {}
    required_ids = _collect_required_ids(root, library_index, resolved, arch, cache, set())

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
    return f"$(path-bin)/libs/{arch}/{library.libname}"


def _emit_scope_lines(owner_id: str, suffix: str, libraries: list[LibraryMeta], arch: str) -> list[str]:
    dep_ids = " ".join(_stable_unique(library.id for library in libraries))
    archives = " ".join(
        _stable_unique(_archive_path_for_arch(library, arch) for library in libraries if library.libname)
    )
    includes_c = " ".join(_stable_unique(library.include_c for library in libraries))
    includes_cpp = " ".join(_stable_unique(library.include_cpp for library in libraries))
    includes_asm = " ".join(_stable_unique(library.include_asm for library in libraries))

    return [
        f"{owner_id}-dep-ids{suffix} := {dep_ids}",
        f"{owner_id}-dep-archives{suffix} := {archives}",
        f"{owner_id}-includes-c{suffix} := {includes_c}",
        f"{owner_id}-includes-cpp{suffix} := {includes_cpp}",
        f"{owner_id}-includes-asm{suffix} := {includes_asm}",
        "",
    ]


def _resolve_default_scope(root: Path, owner: OwnerMeta, dep_entries: list[DependencyEntry]) -> list[LibraryMeta]:
    if not dep_entries:
        return []

    resolved_by_arch = [
        _resolve_scope(root, owner, dep_entries, arch, f"default/{arch}")
        for arch in KNOWN_ARCHITECTURES
    ]
    return resolved_by_arch[0]


def emit(root: Path, owner: OwnerMeta, current_arch: str | None) -> str:
    dependency_path = Path(owner.root) / "dependencies.toml"
    common_deps, arch_entries = _read_dependency_file(dependency_path)

    default_resolved = _resolve_default_scope(root, owner, common_deps)

    lines = [
        f"# Generated by script/py/resolve_deps.py for {owner.id}. Do not edit this file directly.",
        "",
    ]
    lines.extend(_emit_scope_lines(owner.id, "", default_resolved, "$(arch)"))

    for arch in sorted(arch_entries):
        resolved_arch = _resolve_scope(
            root,
            owner,
            common_deps + arch_entries[arch],
            arch,
            arch,
        )
        lines.extend(_emit_scope_lines(owner.id, f"-{arch}", resolved_arch, arch))

    return "\n".join(lines)
