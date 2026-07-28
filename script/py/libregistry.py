#!/usr/bin/env python3
"""Shared metadata registry for libraries."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import tomllib

import semver

KNOWN_ARCHITECTURES = ("riscv64", "loongarch64")
KNOWN_ENVIRONMENTS = ("freestanding", "host")
KNOWN_FEATURES = ("cpp-static-reflection",)
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
    support_environments: tuple[str, ...] = ("freestanding",)
    testbench_test: tuple[str, ...] = ()
    testbench_headercheck: tuple[str, ...] = ()
    testbench_bench: tuple[str, ...] = ()
    testbench_freestanding: tuple[str, ...] = ()
    testbench_example: tuple[str, ...] = ()
    host_libname: str | None = None
    host_makefile: str | None = None
    host_target: str | None = None

    def libname_for(self, environment: str) -> str:
        if environment == "host" and self.host_libname is not None:
            return self.host_libname
        return self.libname

    def makefile_for(self, environment: str) -> str:
        if environment == "host" and self.host_makefile is not None:
            return self.host_makefile
        return self.makefile

    def target_for(self, environment: str) -> str:
        if environment == "host" and self.host_target is not None:
            return self.host_target
        return self.target

    def archive_path_for(self, environment: str) -> str:
        libname = self.libname_for(environment)
        if not libname:
            return ""
        return f"$(path-bin)/libs/{libname}"

    def is_header_only_for(self, environment: str) -> bool:
        return self.libname_for(environment) == ""

    @property
    def archive_path(self) -> str:
        return self.archive_path_for("freestanding")

    @property
    def is_header_only(self) -> bool:
        return self.is_header_only_for("freestanding")

    @property
    def is_c_library(self) -> bool:
        return self.kind == "c-library"

    def supports(self, environment: str, arch: str | None = None) -> bool:
        if environment not in self.support_environments:
            return False
        return not (
            environment == "freestanding"
            and self.support_archs
            and arch not in self.support_archs
        )


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


@dataclass(frozen=True)
class HostProgramMeta:
    id: str
    owner: str
    root: str
    metadata_path: str
    kind: str
    makefile: str
    target: str
    output: str
    requires_features: tuple[str, ...] = ()
    expect: str = "success"
    stderr_contains: str = ""
    stderr_equals: str = ""


@dataclass(frozen=True)
class HeaderCheckMeta:
    id: str
    owner: str
    root: str
    metadata_path: str
    header: str
    language: str
    environments: tuple[str, ...]
    requires_features: tuple[str, ...] = ()
    before_headers: tuple[str, ...] = ()
    after_headers: tuple[str, ...] = ()


@dataclass(frozen=True)
class FreestandingCheckMeta:
    id: str
    owner: str
    root: str
    library_root: str
    metadata_path: str
    kind: str
    language: str
    sources: tuple[str, ...]
    expect: str = "success"


@dataclass(frozen=True)
class TestbenchMetadataSource:
    path: Path
    category: str
    owner: str
    library_root: Path


def scan_metadata_files(root: Path) -> list[Path]:
    metadata_files = []
    for relative_root in ("libs", "third_party/libs"):
        scan_root = root / relative_root
        if not scan_root.is_dir():
            continue
        metadata_files.extend(
            path
            for path in sorted(scan_root.rglob("metadata.toml"))
            if "testbench" not in path.relative_to(scan_root).parts
        )
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


def normalize_choice_list(
    raw_value: object,
    field: str,
    choices: tuple[str, ...],
    *,
    default: tuple[str, ...] = (),
) -> tuple[str, ...]:
    if raw_value is None:
        return default
    if not isinstance(raw_value, list) or not raw_value or not all(
        isinstance(item, str) and item for item in raw_value
    ):
        raise ValueError(f"{field} must be a non-empty array of strings")
    unknown = sorted(set(raw_value) - set(choices))
    if unknown:
        raise ValueError(f"{field} contains unsupported values: {', '.join(unknown)}")
    return tuple(dict.fromkeys(raw_value))


def normalize_string_list(raw_value: object, field: str) -> tuple[str, ...]:
    if raw_value is None:
        return ()
    if not isinstance(raw_value, list) or not all(
        isinstance(item, str) and item for item in raw_value
    ):
        raise ValueError(f"{field} must be an array of non-empty strings")
    return tuple(dict.fromkeys(raw_value))


def normalize_testbench_paths(
    metadata_path: Path, raw_value: object
) -> dict[str, tuple[str, ...]]:
    categories = ("test", "headercheck", "bench", "freestanding", "example")
    if raw_value is None:
        return {category: () for category in categories}
    if not isinstance(raw_value, dict):
        raise ValueError(f"{metadata_path}: testbench must be a table")

    unknown = sorted(set(raw_value) - set(categories))
    if unknown:
        raise ValueError(
            f"{metadata_path}: testbench contains unsupported fields: {', '.join(unknown)}"
        )

    result: dict[str, tuple[str, ...]] = {}
    seen_paths: dict[str, str] = {}
    for category in categories:
        field = f"{metadata_path}: testbench.{category}"
        paths = raw_value.get(category, [])
        if not isinstance(paths, list) or not all(
            isinstance(path, str) and path for path in paths
        ):
            raise ValueError(f"{field} must be an array of non-empty strings")

        resolved_paths: list[str] = []
        for raw_path in paths:
            relative_path = normalize_relative_path(raw_path, field, required=True)
            if Path(relative_path).suffix != ".toml":
                raise ValueError(f"{field} entries must name TOML files")
            resolved_path = (metadata_path.parent / relative_path).resolve()
            if not resolved_path.is_file():
                raise ValueError(
                    f"{metadata_path}: testbench.{category} file does not exist: {raw_path}"
                )
            path_key = str(resolved_path)
            previous_category = seen_paths.get(path_key)
            if previous_category is not None:
                raise ValueError(
                    f"{metadata_path}: testbench file {raw_path!r} is registered in "
                    f"both {previous_category!r} and {category!r}"
                )
            seen_paths[path_key] = category
            resolved_paths.append(path_key)
        result[category] = tuple(resolved_paths)
    return result


def normalize_features(raw_value: object, field: str) -> tuple[str, ...]:
    features = normalize_string_list(raw_value, field)
    unknown = sorted(set(features) - set(KNOWN_FEATURES))
    if unknown:
        raise ValueError(f"{field} contains unknown features: {', '.join(unknown)}")
    return features


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


def normalize_library_build(
    metadata_path: Path,
    raw_value: object,
    *,
    field: str,
    inherited_libname: str,
    inherited_makefile: str,
    inherited_target: str,
) -> tuple[str | None, str | None, str | None]:
    if raw_value is None:
        return None, None, None
    if not isinstance(raw_value, dict):
        raise ValueError(f"{metadata_path}: {field} must be a table")
    unknown = sorted(set(raw_value) - {"libname", "makefile", "target"})
    if unknown:
        raise ValueError(
            f"{metadata_path}: {field} contains unsupported fields: {', '.join(unknown)}"
        )

    libname = normalize_libname(
        raw_value.get("libname", inherited_libname),
        f"{metadata_path}: {field}.libname",
    )
    raw_makefile = raw_value.get("makefile", inherited_makefile)
    target = raw_value.get("target", inherited_target)
    if not isinstance(raw_makefile, str):
        raise ValueError(f"{metadata_path}: {field}.makefile must be a string")
    if not isinstance(target, str):
        raise ValueError(f"{metadata_path}: {field}.target must be a string")
    if libname and (not raw_makefile or not target):
        raise ValueError(
            f"{metadata_path}: {field}.makefile and {field}.target must be non-empty "
            "for non-header-only variants"
        )

    makefile = ""
    if raw_makefile:
        resolved_makefile = (metadata_path.parent / raw_makefile).resolve()
        if not resolved_makefile.is_file():
            raise ValueError(
                f"{metadata_path}: {field}.makefile does not exist: {raw_makefile}"
            )
        makefile = str(resolved_makefile)
    return libname, makefile, target


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
            testbench_paths = normalize_testbench_paths(
                metadata_path, entry.get("testbench")
            )
            host_libname, host_makefile, host_target = normalize_library_build(
                metadata_path,
                entry.get("host"),
                field="host",
                inherited_libname=libname,
                inherited_makefile=makefile_path,
                inherited_target=raw_target,
            )
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
                    support_environments=normalize_choice_list(
                        entry.get("support-environments"),
                        f"{metadata_path}: support-environments",
                        KNOWN_ENVIRONMENTS,
                        default=("freestanding",),
                    ),
                    testbench_test=testbench_paths["test"],
                    testbench_headercheck=testbench_paths["headercheck"],
                    testbench_bench=testbench_paths["bench"],
                    testbench_freestanding=testbench_paths["freestanding"],
                    testbench_example=testbench_paths["example"],
                    host_libname=host_libname,
                    host_makefile=host_makefile,
                    host_target=host_target,
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
    return libraries_for_environment(root, "freestanding", arch)


def libraries_for_environment(
    root: Path, environment: str, arch: str | None = None
) -> list[LibraryMeta]:
    if environment not in KNOWN_ENVIRONMENTS:
        raise ValueError(f"unsupported environment: {environment!r}")
    return [
        library
        for library in scan_libraries(root)
        if library.supports(environment, arch)
    ]


def _testbench_metadata_sources(root: Path) -> list[TestbenchMetadataSource]:
    result: list[TestbenchMetadataSource] = []
    for library in scan_libraries(root):
        paths_by_category = {
            "test": library.testbench_test,
            "headercheck": library.testbench_headercheck,
            "bench": library.testbench_bench,
            "freestanding": library.testbench_freestanding,
            "example": library.testbench_example,
        }
        for category, paths in paths_by_category.items():
            result.extend(
                TestbenchMetadataSource(
                    path=Path(path),
                    category=category,
                    owner=library.id,
                    library_root=Path(library.root),
                )
                for path in paths
            )
    return result


def scan_testbenches(
    root: Path,
) -> tuple[
    list[HostProgramMeta],
    list[HeaderCheckMeta],
    list[FreestandingCheckMeta],
]:
    programs: list[HostProgramMeta] = []
    header_checks: list[HeaderCheckMeta] = []
    freestanding_checks: list[FreestandingCheckMeta] = []
    seen_program_ids: dict[str, Path] = {}
    seen_check_ids: dict[str, Path] = {}
    seen_freestanding_ids: dict[str, Path] = {}

    for source in _testbench_metadata_sources(root):
        metadata_path = source.path
        owner = source.owner
        library_root = source.library_root
        category = source.category
        with metadata_path.open("rb") as metadata_file:
            data = tomllib.load(metadata_file)

        raw_programs = data.get("hostprog", [])
        if not isinstance(raw_programs, list):
            raise ValueError(f"{metadata_path}: hostprog must be an array of tables")
        if category in {"headercheck", "freestanding"} and raw_programs:
            raise ValueError(
                f"{metadata_path}: hostprog entries belong in testbench.test, "
                "testbench.bench, or testbench.example files"
            )
        for entry in raw_programs:
            if not isinstance(entry, dict):
                raise ValueError(f"{metadata_path}: hostprog entries must be tables")
            program_id = validate_global_id(entry.get("id"), f"{metadata_path}: hostprog.id")
            if program_id in seen_program_ids:
                raise ValueError(
                    f"duplicate host program id {program_id!r}: "
                    f"{seen_program_ids[program_id]} and {metadata_path}"
                )
            seen_program_ids[program_id] = metadata_path
            kind = entry.get("kind")
            if kind not in {"test", "bench", "example"}:
                raise ValueError(
                    f"{metadata_path}: hostprog.kind must be 'test', 'bench', "
                    "or 'example'"
                )
            if kind != category:
                raise ValueError(
                    f"{metadata_path}: hostprog.kind {kind!r} does not match "
                    f"testbench.{category} registration"
                )
            makefile = normalize_relative_path(
                entry.get("makefile"), f"{metadata_path}: hostprog.makefile", required=True
            )
            makefile_path = (metadata_path.parent / makefile).resolve()
            if not makefile_path.is_file():
                raise ValueError(f"{metadata_path}: makefile does not exist: {makefile}")
            target = entry.get("target")
            if not isinstance(target, str) or not target:
                raise ValueError(f"{metadata_path}: hostprog.target must be a non-empty string")
            expect = entry.get("expect", "success")
            if expect not in {"success", "abort"}:
                raise ValueError(f"{metadata_path}: hostprog.expect must be 'success' or 'abort'")
            stderr_contains = entry.get("stderr-contains", "")
            stderr_equals = entry.get("stderr-equals", "")
            if not isinstance(stderr_contains, str) or not isinstance(stderr_equals, str):
                raise ValueError(f"{metadata_path}: stderr assertions must be strings")
            programs.append(
                HostProgramMeta(
                    id=program_id,
                    owner=owner,
                    root=str(metadata_path.parent.resolve()),
                    metadata_path=str(metadata_path),
                    kind=kind,
                    makefile=str(makefile_path),
                    target=target,
                    output=normalize_output_path(
                        entry.get("output"), f"{metadata_path}: hostprog.output"
                    ),
                    requires_features=normalize_features(
                        entry.get("requires-features"),
                        f"{metadata_path}: hostprog.requires-features",
                    ),
                    expect=expect,
                    stderr_contains=stderr_contains,
                    stderr_equals=stderr_equals,
                )
            )

        raw_checks = data.get("headercheck", [])
        if not isinstance(raw_checks, list):
            raise ValueError(f"{metadata_path}: headercheck must be an array of tables")
        if category != "headercheck" and raw_checks:
            raise ValueError(
                f"{metadata_path}: headercheck entries belong in testbench.headercheck files"
            )
        for index, entry in enumerate(raw_checks):
            if not isinstance(entry, dict):
                raise ValueError(f"{metadata_path}: headercheck entries must be tables")
            header = normalize_relative_path(
                entry.get("header"), f"{metadata_path}: headercheck.header", required=True
            )
            language = entry.get("language")
            if language not in {"c", "c++"}:
                raise ValueError(f"{metadata_path}: headercheck.language must be 'c' or 'c++'")
            check_id = entry.get("id", f"{owner}-header-{index}")
            check_id = validate_global_id(check_id, f"{metadata_path}: headercheck.id")
            if check_id in seen_check_ids:
                raise ValueError(
                    f"duplicate header check id {check_id!r}: "
                    f"{seen_check_ids[check_id]} and {metadata_path}"
                )
            seen_check_ids[check_id] = metadata_path
            header_checks.append(
                HeaderCheckMeta(
                    id=check_id,
                    owner=owner,
                    root=str(library_root.resolve()),
                    metadata_path=str(metadata_path),
                    header=header,
                    language=language,
                    environments=normalize_choice_list(
                        entry.get("environments"),
                        f"{metadata_path}: headercheck.environments",
                        KNOWN_ENVIRONMENTS,
                        default=("freestanding", "host"),
                    ),
                    requires_features=normalize_features(
                        entry.get("requires-features"),
                        f"{metadata_path}: headercheck.requires-features",
                    ),
                    before_headers=normalize_string_list(
                        entry.get("before-headers"),
                        f"{metadata_path}: headercheck.before-headers",
                    ),
                    after_headers=normalize_string_list(
                        entry.get("after-headers"),
                        f"{metadata_path}: headercheck.after-headers",
                    ),
                )
            )

        raw_freestanding = data.get("freestanding-check", [])
        if not isinstance(raw_freestanding, list):
            raise ValueError(
                f"{metadata_path}: freestanding-check must be an array of tables"
            )
        if category != "freestanding" and raw_freestanding:
            raise ValueError(
                f"{metadata_path}: freestanding-check entries belong in "
                "testbench.freestanding files"
            )
        for entry in raw_freestanding:
            if not isinstance(entry, dict):
                raise ValueError(
                    f"{metadata_path}: freestanding-check entries must be tables"
                )
            check_id = validate_global_id(
                entry.get("id"), f"{metadata_path}: freestanding-check.id"
            )
            if check_id in seen_freestanding_ids:
                raise ValueError(
                    f"duplicate freestanding check id {check_id!r}: "
                    f"{seen_freestanding_ids[check_id]} and {metadata_path}"
                )
            seen_freestanding_ids[check_id] = metadata_path

            kind = entry.get("kind")
            if kind not in {"compile", "link"}:
                raise ValueError(
                    f"{metadata_path}: freestanding-check.kind must be "
                    "'compile' or 'link'"
                )
            language = entry.get("language")
            if language not in {"c", "c++"}:
                raise ValueError(
                    f"{metadata_path}: freestanding-check.language must be 'c' or 'c++'"
                )
            expect = entry.get("expect", "success")
            if expect not in {"success", "failure"}:
                raise ValueError(
                    f"{metadata_path}: freestanding-check.expect must be "
                    "'success' or 'failure'"
                )
            raw_sources = entry.get("sources")
            if not isinstance(raw_sources, list) or not raw_sources or not all(
                isinstance(item, str) and item for item in raw_sources
            ):
                raise ValueError(
                    f"{metadata_path}: freestanding-check.sources must be a "
                    "non-empty array of strings"
                )
            allowed_suffixes = {"c": {".c"}, "c++": {".cpp"}}
            sources: list[str] = []
            for raw_source in raw_sources:
                relative_source = normalize_relative_path(
                    raw_source,
                    f"{metadata_path}: freestanding-check.sources",
                    required=True,
                )
                source_path = (metadata_path.parent / relative_source).resolve()
                if not source_path.is_file():
                    raise ValueError(
                        f"{metadata_path}: freestanding source does not exist: "
                        f"{raw_source}"
                    )
                if source_path.suffix not in allowed_suffixes[language]:
                    raise ValueError(
                        f"{metadata_path}: freestanding source {raw_source!r} "
                        f"does not match language {language!r}"
                    )
                sources.append(relative_source)

            freestanding_checks.append(
                FreestandingCheckMeta(
                    id=check_id,
                    owner=owner,
                    root=str(metadata_path.parent.resolve()),
                    library_root=str(library_root.resolve()),
                    metadata_path=str(metadata_path),
                    kind=kind,
                    language=language,
                    sources=tuple(sources),
                    expect=expect,
                )
            )

    return programs, header_checks, freestanding_checks
