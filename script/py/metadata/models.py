"""Typed metadata records shared by scanners, generators, and runners."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


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
class HostToolMeta:
    id: str
    root: str
    metadata_path: str
    makefile: str
    target: str
    output: str


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
