#!/usr/bin/env python3
"""Generate the Make fragment that assembles initrd/initrd.toml inputs."""

from __future__ import annotations

import sys
from pathlib import Path
import tomllib

from common.arguments import parse_key_value_arguments
from common.paths import REPOSITORY_ROOT
from make_support.emitter import generated_header, multiline_assignment
from metadata.registry import OwnerMeta, scan_programs, validate_global_id

ROOT = REPOSITORY_ROOT
INITRD_CONFIG = ROOT / "initrd" / "initrd.toml"


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
            raise ValueError(
                f"{config_path}: initrd.file[{index}].src must be a non-empty string"
            )
        source_path = Path(src)
        if not source_path.is_absolute():
            source_path = ROOT / source_path
        files.append(
            (
                source_path.resolve(),
                _clean_relative_path(
                    entry.get("dst"),
                    f"{config_path}: initrd.file[{index}].dst",
                ),
            )
        )

    modules: list[tuple[str, Path]] = []
    seen_modules: set[str] = set()
    for index, entry in enumerate(module_entries):
        if not isinstance(entry, dict):
            raise ValueError(f"{config_path}: initrd.module[{index}] must be a table")
        module_id = validate_global_id(
            entry.get("mod"), f"{config_path}: initrd.module[{index}].mod"
        )
        if module_id in seen_modules:
            raise ValueError(f"{config_path}: duplicate initrd module {module_id!r}")
        seen_modules.add(module_id)
        modules.append(
            (
                module_id,
                _clean_relative_path(
                    entry.get("dst"),
                    f"{config_path}: initrd.module[{index}].dst",
                ),
            )
        )

    return files, modules


def _program_index(root: Path) -> dict[str, OwnerMeta]:
    return {program.id: program for program in scan_programs(root)}


def _resolve_modules(
    config_path: Path, root: Path
) -> tuple[list[tuple[Path, Path]], list[tuple[OwnerMeta, Path]]]:
    files, modules = _read_config(config_path)
    programs = _program_index(root)
    resolved_modules: list[tuple[OwnerMeta, Path]] = []
    for module_id, destination in modules:
        program = programs.get(module_id)
        if program is None:
            raise ValueError(f"{config_path}: initrd module {module_id!r} is not registered")
        resolved_modules.append((program, destination))
    return files, resolved_modules


def _validate_make_path(path: str, field: str) -> None:
    # These characters either change Make syntax or make newline-delimited
    # prerequisites ambiguous. The build system already requires shell-safe
    # repository paths, so reject them during configure rather than emit a
    # fragment with a different meaning.
    unsafe = set("\t\r\n #$%:\\")
    if any(character in unsafe for character in path):
        raise ValueError(f"{field} contains a character unsupported by generated Make rules")


def _validate_destinations(
    config_path: Path,
    destinations: list[Path],
) -> None:
    seen: list[Path] = []
    for destination in destinations:
        if destination in seen:
            raise ValueError(f"{config_path}: duplicate initrd destination {destination!s}")
        if any(
            destination.parts[: len(previous.parts)] == previous.parts
            or previous.parts[: len(destination.parts)] == destination.parts
            for previous in seen
        ):
            raise ValueError(
                f"{config_path}: initrd destinations conflict at {destination!s}"
            )
        seen.append(destination)


def _make_source_path(source: Path) -> str:
    try:
        relative = source.relative_to(ROOT)
    except ValueError:
        return source.as_posix()
    return "$(path-e)/" + relative.as_posix()


def emit(
    config_path: Path = INITRD_CONFIG,
    root: Path = ROOT,
) -> str:
    """Return the generated Make fragment for the initrd configuration."""
    files, modules = _resolve_modules(config_path.resolve(), root.resolve())
    destinations = [destination for _, destination in files]
    destinations.extend(destination for _, destination in modules)
    _validate_destinations(config_path, destinations)

    for source, _ in files:
        if not source.is_file():
            raise ValueError(f"{config_path}: initrd source file does not exist: {source}")
        _validate_make_path(source.as_posix(), f"{config_path}: initrd source")
    for destination in destinations:
        _validate_make_path(
            destination.as_posix(), f"{config_path}: initrd destination"
        )
    for program, _ in modules:
        _validate_make_path(program.output, f"{program.metadata_path}: output")
        _validate_make_path(program.makefile, f"{program.metadata_path}: makefile")

    module_ids = [program.id for program, _ in modules]
    module_outputs = [f"$(path-bin)/{program.output}" for program, _ in modules]
    input_paths = [_make_source_path(source) for source, _ in files]
    input_paths.extend(module_outputs)

    lines = [
        generated_header("script/py/initrd.py"),
        f"# Source: {config_path}",
        "",
        f"initrd-module-ids := {' '.join(module_ids)}",
        "initrd-module-targets := "
        + " ".join(f"build-module-{module_id}" for module_id in module_ids),
        "",
    ]
    lines.extend(multiline_assignment("initrd-inputs", input_paths))
    lines.append(
        ".PHONY: build-modules build-initrd "
        + " ".join(f"build-module-{module_id}" for module_id in module_ids)
    )
    lines.append("")
    lines.append("build-modules: build-hosttool build-libs $(initrd-module-targets)")
    lines.append("")

    for program, _ in modules:
        output = f"$(path-bin)/{program.output}"
        lines.extend(
            (
                f"build-module-{program.id}: build-hosttool | build-libs",
                f"\t$(q)$(MAKE) -f $(program-{program.id}-makefile) \\",
                "\t\tglobal-env=$(global-env) \\",
                "\t\tarch=$(arch) \\",
                "\t\tq=$(q) \\",
                f"\t\tctx=$(path-ctx)/module-{program.id}.mk \\",
                f"\t\t$(program-{program.id}-target)",
                "",
                f"{output}: | build-module-{program.id}",
                "",
            )
        )

    lines.extend(
        (
            "$(path-initrd): $(path-cache)/initrd.mk $(initrd-inputs) | build-modules",
            "\t$(q)$(rmdir) $(call shq,$(path-initrd-root))",
            "\t$(q)$(mkdir) $(call shq,$(path-initrd-root))",
        )
    )
    for source, destination in files:
        destination_path = f"$(path-initrd-root)/{destination.as_posix()}"
        lines.extend(
            (
                f"\t$(q)$(mkdir) $(call shq,$(dir {destination_path}))",
                "\t$(q)$(cp) "
                f"$(call shq,{_make_source_path(source)}) $(call shq,{destination_path})",
            )
        )
    for program, destination in modules:
        source = f"$(path-bin)/{program.output}"
        destination_path = f"$(path-initrd-root)/{destination.as_posix()}"
        lines.extend(
            (
                f"\t$(q)$(mkdir) $(call shq,$(dir {destination_path}))",
                f"\t$(q)$(cp) $(call shq,{source}) $(call shq,{destination_path})",
            )
        )
    lines.extend(
        (
            "\t$(q)$(mkdir) $(call shq,$(@D))",
            "\t$(q)$(cd) $(call shq,$(path-initrd-root)) && { \\",
            "\t\tfind . -mindepth 1 -type d -printf '%P\\0' | sort -z; \\",
            "\t\tfind . -mindepth 1 -type f -printf '%P\\0' | sort -z; \\",
            "\t} | $(initrd-cpio) --null --create --format=newc > $(call shq,$(path-initrd).tmp)",
            "\t$(q)$(mv) $(call shq,$(path-initrd).tmp) $(call shq,$@)",
            "",
            "build-initrd: build-hosttool $(path-initrd)",
            "",
        )
    )
    return "\n".join(lines)


def _parse_arguments(arguments: list[str]) -> dict[str, str]:
    return parse_key_value_arguments(
        arguments,
        {"config", "root"},
        non_empty_keys={"config", "root"},
    )


def main(arguments: list[str]) -> int:
    try:
        values = _parse_arguments(arguments)
        print(
            emit(
                Path(values.get("config", str(INITRD_CONFIG))),
                Path(values.get("root", str(ROOT))),
            ),
            end="",
        )
    except (OSError, tomllib.TOMLDecodeError, ValueError) as error:
        print(f"initrd.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
