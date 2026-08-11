#!/usr/bin/env python3
"""Validate the native Clang toolchain and emit its Make configuration."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Mapping, Sequence

from common.arguments import parse_key_value_arguments
from common.filesystem import write_text_if_changed
from make_support.emitter import generated_header, make_value


ARCH_ALIASES = {
    "amd64": "x86_64",
    "x86-64": "x86_64",
    "x64": "x86_64",
    "arm64": "aarch64",
}
CPP_STDLIBS = {"auto", "libstdc++", "libc++"}
SAFE_TRIPLE = re.compile(r"^[A-Za-z0-9_.+-]+$")

C_PROBE = """\
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
int main(void) {
    void *value = malloc(1);
    free(value);
    return sizeof(uintptr_t) == sizeof(void *) ? 0 : 1;
}
"""

CPP_PROBE = """\
#include <atomic>
#include <compare>
#include <concepts>
#include <cstddef>
#include <type_traits>

template<class T>
concept Scalar = std::is_scalar_v<T>;

int main() {
    std::atomic<int> value{0};
    static_assert(Scalar<int>);
    auto order = (value.load() <=> 0);
    return order == 0 ? 0 : 1;
}
"""

REFLECTION_PROBE = """\
#include <meta>
#ifndef __cpp_impl_reflection
#error static reflection is unavailable
#endif
int main() { return 0; }
"""


@dataclass(frozen=True)
class HostConfig:
    clang: str
    clangxx: str
    llvm_ar: str
    sysroot: Path
    cppstdlib: str
    cflags: tuple[str, ...]
    cxxflags: tuple[str, ...]
    ldflags: tuple[str, ...]
    requested_arch: str | None = None


@dataclass(frozen=True)
class HostValidation:
    clang: str
    clangxx: str
    llvm_ar: str
    sysroot: Path
    arch: str
    triple: str
    clang_version: str
    clangxx_version: str
    llvm_ar_version: str
    cppstdlib: str
    cflags: tuple[str, ...]
    cxxflags: tuple[str, ...]
    ldflags: tuple[str, ...]
    fingerprint: str
    features: tuple[str, ...] = ()


def normalize_arch(value: str) -> str:
    normalized = value.strip().lower()
    if not normalized:
        raise ValueError("host architecture must not be empty")
    return ARCH_ALIASES.get(normalized, normalized)


def parse_arguments(arguments: list[str]) -> tuple[Path, str | None]:
    if not arguments or arguments[0] != "validate":
        raise ValueError("expected action 'validate'")

    values = parse_key_value_arguments(
        arguments[1:],
        {"output", "host-arch"},
        required_keys={"output"},
        non_empty_keys={"output", "host-arch"},
    )
    return Path(values["output"]), values.get("host-arch")


def _parse_flags(value: str, variable: str) -> tuple[str, ...]:
    try:
        return tuple(shlex.split(value))
    except ValueError as error:
        raise ValueError(f"{variable} contains invalid shell quoting: {error}") from error


def _reject_managed_flags(flags: Sequence[str], variable: str) -> None:
    for flag in flags:
        if flag == "--sysroot" or flag.startswith("--sysroot="):
            raise ValueError(f"{variable} must not override HOST_SYSROOT")
        if flag == "-isysroot" or flag.startswith("-isysroot"):
            raise ValueError(f"{variable} must not override HOST_SYSROOT")


def config_from_environment(
    environment: Mapping[str, str], requested_arch: str | None
) -> HostConfig:
    sysroot_value = environment.get("HOST_SYSROOT", "")
    if not sysroot_value:
        raise ValueError(
            "HOST_SYSROOT is required; configure clang.toml [host].sysroot or set it explicitly"
        )
    sysroot = Path(sysroot_value)
    if not sysroot.is_absolute():
        raise ValueError(f"HOST_SYSROOT must be an absolute directory: {sysroot}")
    if not sysroot.is_dir():
        raise ValueError(f"HOST_SYSROOT does not exist or is not a directory: {sysroot}")

    cppstdlib = environment.get("HOST_CPPSTDLIB", "auto") or "auto"
    if cppstdlib not in CPP_STDLIBS:
        raise ValueError(
            f"HOST_CPPSTDLIB must be one of {', '.join(sorted(CPP_STDLIBS))}: {cppstdlib!r}"
        )

    cflags = _parse_flags(environment.get("HOST_CFLAGS", ""), "HOST_CFLAGS")
    cxxflags = _parse_flags(environment.get("HOST_CXXFLAGS", ""), "HOST_CXXFLAGS")
    ldflags = _parse_flags(environment.get("HOST_LDFLAGS", ""), "HOST_LDFLAGS")
    for flags, variable in (
        (cflags, "HOST_CFLAGS"),
        (cxxflags, "HOST_CXXFLAGS"),
        (ldflags, "HOST_LDFLAGS"),
    ):
        _reject_managed_flags(flags, variable)
    if any(flag == "-stdlib" or flag.startswith("-stdlib=") for flag in cxxflags):
        raise ValueError("HOST_CXXFLAGS must not override HOST_CPPSTDLIB")

    return HostConfig(
        clang=environment.get("HOST_CLANG", "clang") or "clang",
        clangxx=environment.get("HOST_CLANGXX", "clang++") or "clang++",
        llvm_ar=environment.get("HOST_LLVM_AR", "llvm-ar") or "llvm-ar",
        sysroot=sysroot.resolve(),
        cppstdlib=cppstdlib,
        cflags=cflags,
        cxxflags=cxxflags,
        ldflags=ldflags,
        requested_arch=normalize_arch(requested_arch) if requested_arch else None,
    )


def _run(
    command: Sequence[str],
    *,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            list(command),
            input=input_text,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
    except FileNotFoundError as error:
        raise ValueError(f"command not found: {command[0]}") from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or "").strip()
        rendered = shlex.join(command)
        raise ValueError(f"command failed: {rendered}\n{detail}") from error


def _resolve_tool(value: str, label: str) -> str:
    resolved = shutil.which(value)
    if resolved is None:
        raise ValueError(f"{label} executable was not found: {value}")
    # Driver behavior depends on argv[0] (notably clang versus clang++), so do
    # not collapse compiler symlinks to their shared binary.
    return str(Path(resolved).absolute())


def _first_line(value: str, label: str) -> str:
    lines = value.strip().splitlines()
    if not lines:
        raise ValueError(f"{label} returned an empty version")
    return lines[0]


def _predefined_macros(command: Sequence[str], language: str) -> set[str]:
    result = _run([*command, "-dM", "-E", "-x", language, "-"], input_text="")
    macros = set()
    for line in result.stdout.splitlines():
        match = re.match(r"#define\s+([A-Za-z_][A-Za-z0-9_]*)", line)
        if match:
            macros.add(match.group(1))
    return macros


def _target_triple(command: Sequence[str], label: str) -> str:
    triple = _run([*command, "-print-target-triple"]).stdout.strip().lower()
    if not triple or not SAFE_TRIPLE.fullmatch(triple) or "-" not in triple:
        raise ValueError(f"{label} returned an invalid target triple: {triple!r}")
    return triple


def _resource_dir(command: Sequence[str], label: str) -> Path:
    value = _run([*command, "-print-resource-dir"]).stdout.strip()
    path = Path(value)
    if not path.is_absolute() or not path.is_dir():
        raise ValueError(f"{label} returned an invalid resource directory: {value!r}")
    return path.resolve()


def _extract_gcc_install(flags: Sequence[str]) -> Path | None:
    for index, flag in enumerate(flags):
        if flag.startswith("--gcc-install-dir="):
            value = flag.partition("=")[2]
        elif flag == "--gcc-install-dir" and index + 1 < len(flags):
            value = flags[index + 1]
        else:
            continue
        path = Path(value)
        if not path.is_absolute() or not path.is_dir():
            raise ValueError(f"invalid --gcc-install-dir: {value!r}")
        return path.resolve()
    return None


def _is_below(path: Path, root: Path) -> bool:
    return path == root or root in path.parents


def _gcc_include_roots(gcc_install: Path) -> tuple[Path, ...]:
    # GCC installs libraries under <prefix>/lib/gcc/<triple>/<version> and
    # libstdc++ headers under <prefix>/include[/<triple>]/c++/<version>.
    if len(gcc_install.parents) < 4:
        return (gcc_install,)
    prefix = gcc_install.parents[3]
    triple = gcc_install.parent.name
    version = gcc_install.name
    return (
        gcc_install,
        prefix / "include" / "c++" / version,
        prefix / "include" / triple / "c++" / version,
    )


def parse_include_search(output: str) -> list[Path]:
    paths: list[Path] = []
    active = False
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if line == "#include <...> search starts here:":
            active = True
            continue
        if active and line == "End of search list.":
            break
        if not active or not line or line.startswith("ignoring "):
            continue
        suffix = " (framework directory)"
        if line.endswith(suffix):
            line = line[: -len(suffix)]
        path = Path(line)
        if path.is_absolute():
            paths.append(path.resolve())
    if not paths:
        raise ValueError("compiler verbose output did not contain an include search list")
    return paths


def _validate_include_search(
    command: Sequence[str],
    language: str,
    sysroot: Path,
    resource_dir: Path,
    gcc_install: Path | None,
) -> None:
    result = _run([*command, "-E", "-v", "-x", language, "-"], input_text="")
    roots = [sysroot, resource_dir]
    if gcc_install is not None:
        roots.extend(_gcc_include_roots(gcc_install))
    unexpected = [
        path
        for path in parse_include_search(result.stderr + "\n" + result.stdout)
        if not any(_is_below(path, root.resolve()) for root in roots)
    ]
    if unexpected:
        raise ValueError(
            "compiler include search escapes HOST_SYSROOT: "
            + ", ".join(str(path) for path in unexpected)
        )


def _detect_cppstdlib(command: Sequence[str]) -> str:
    result = _run(
        [*command, "-dM", "-E", "-x", "c++", "-"],
        input_text="#include <cstddef>\n",
    )
    has_libstdcpp = "__GLIBCXX__" in result.stdout
    has_libcpp = "_LIBCPP_VERSION" in result.stdout
    if has_libstdcpp == has_libcpp:
        raise ValueError("could not identify exactly one C++ standard library provider")
    return "libstdc++" if has_libstdcpp else "libc++"


def _fingerprint(values: Mapping[str, object]) -> str:
    encoded = json.dumps(values, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def validate(config: HostConfig) -> HostValidation:
    clang = _resolve_tool(config.clang, "HOST_CLANG")
    clangxx = _resolve_tool(config.clangxx, "HOST_CLANGXX")
    llvm_ar = _resolve_tool(config.llvm_ar, "HOST_LLVM_AR")
    sysroot_flag = f"--sysroot={config.sysroot}"

    cflags = config.cflags
    provider_flags = () if config.cppstdlib == "auto" else (f"-stdlib={config.cppstdlib}",)
    cxxflags = ("-std=c++23", *config.cxxflags, *provider_flags)
    c_command = (clang, sysroot_flag, *cflags)
    cpp_command = (clangxx, sysroot_flag, *cxxflags)

    c_macros = _predefined_macros(c_command, "c")
    cpp_macros = _predefined_macros(cpp_command, "c++")
    if "__clang__" not in c_macros:
        raise ValueError("HOST_CLANG is not a Clang compiler")
    if "__clang__" not in cpp_macros:
        raise ValueError("HOST_CLANGXX is not a Clang compiler")

    c_triple = _target_triple(c_command, "HOST_CLANG")
    cpp_triple = _target_triple(cpp_command, "HOST_CLANGXX")
    if c_triple != cpp_triple:
        raise ValueError(
            f"host compiler target mismatch: HOST_CLANG={c_triple}, HOST_CLANGXX={cpp_triple}"
        )
    target_arch = normalize_arch(c_triple.split("-", 1)[0])
    machine_arch = normalize_arch(platform.machine())
    if target_arch != machine_arch:
        raise ValueError(
            f"host architecture mismatch: compiler target is {target_arch}, uname is {machine_arch}"
        )
    if config.requested_arch and config.requested_arch != target_arch:
        raise ValueError(
            f"host architecture mismatch: requested {config.requested_arch}, compiler target is {target_arch}"
        )

    clang_version = _first_line(_run([clang, "--version"]).stdout, "HOST_CLANG")
    clangxx_version = _first_line(_run([clangxx, "--version"]).stdout, "HOST_CLANGXX")
    llvm_ar_version = _first_line(_run([llvm_ar, "--version"]).stdout, "HOST_LLVM_AR")
    if "LLVM" not in llvm_ar_version:
        raise ValueError("HOST_LLVM_AR is not an LLVM archiver")

    gcc_install = _extract_gcc_install(cxxflags)
    _validate_include_search(
        c_command,
        "c",
        config.sysroot,
        _resource_dir(c_command, "HOST_CLANG"),
        None,
    )
    _validate_include_search(
        cpp_command,
        "c++",
        config.sysroot,
        _resource_dir(cpp_command, "HOST_CLANGXX"),
        gcc_install,
    )

    detected_cppstdlib = _detect_cppstdlib(cpp_command)
    if config.cppstdlib != "auto" and detected_cppstdlib != config.cppstdlib:
        raise ValueError(
            f"C++ standard library mismatch: requested {config.cppstdlib}, detected {detected_cppstdlib}"
        )

    with tempfile.TemporaryDirectory(prefix="sustcore-host-probe-") as temporary_directory:
        temporary_root = Path(temporary_directory)
        c_program = temporary_root / "c-probe"
        cpp_program = temporary_root / "cpp-probe"
        _run(
            [*c_command, "-x", "c", "-", "-o", str(c_program), *config.ldflags],
            input_text=C_PROBE,
        )
        _run([str(c_program)])
        _run(
            [*cpp_command, "-x", "c++", "-", "-o", str(cpp_program), *config.ldflags],
            input_text=CPP_PROBE,
        )
        _run([str(cpp_program)])

        features: list[str] = []
        reflection_program = temporary_root / "reflection-probe"
        try:
            _run(
                [
                    *cpp_command,
                    "-freflection-latest",
                    "-x",
                    "c++",
                    "-",
                    "-o",
                    str(reflection_program),
                    *config.ldflags,
                ],
                input_text=REFLECTION_PROBE,
            )
        except ValueError:
            pass
        else:
            features.append("cpp-static-reflection")

    fingerprint_values = {
        "clang": clang,
        "clangxx": clangxx,
        "llvm_ar": llvm_ar,
        "sysroot": str(config.sysroot),
        "arch": target_arch,
        "triple": c_triple,
        "clang_version": clang_version,
        "clangxx_version": clangxx_version,
        "llvm_ar_version": llvm_ar_version,
        "cppstdlib": detected_cppstdlib,
        "cflags": cflags,
        "cxxflags": cxxflags,
        "ldflags": config.ldflags,
        "features": features,
    }
    return HostValidation(
        clang=clang,
        clangxx=clangxx,
        llvm_ar=llvm_ar,
        sysroot=config.sysroot,
        arch=target_arch,
        triple=c_triple,
        clang_version=clang_version,
        clangxx_version=clangxx_version,
        llvm_ar_version=llvm_ar_version,
        cppstdlib=detected_cppstdlib,
        cflags=cflags,
        cxxflags=cxxflags,
        ldflags=config.ldflags,
        fingerprint=_fingerprint(fingerprint_values),
        features=tuple(features),
    )


def _shell_value(value: str) -> str:
    return make_value(shlex.quote(value))


def _shell_flags(flags: Sequence[str]) -> str:
    return make_value(shlex.join(flags))


def emit_make(validation: HostValidation) -> str:
    return "\n".join(
        (
            generated_header("script/py/host.py"),
            "",
            "environment := host",
            f"host-arch := {make_value(validation.arch)}",
            f"host-triple := {make_value(validation.triple)}",
            f"host-comp-c := {_shell_value(validation.clang)}",
            f"host-comp-cpp := {_shell_value(validation.clangxx)}",
            f"host-comp-ar := {_shell_value(validation.llvm_ar)}",
            f"host-clang-version := {make_value(validation.clang_version)}",
            f"host-clangxx-version := {make_value(validation.clangxx_version)}",
            f"host-llvm-ar-version := {make_value(validation.llvm_ar_version)}",
            f"host-sysroot := {_shell_value(str(validation.sysroot))}",
            "host-sysroot-flag := --sysroot=$(host-sysroot)",
            f"host-cppstdlib := {make_value(validation.cppstdlib)}",
            f"host-cflags := {_shell_flags(validation.cflags)}",
            f"host-cxxflags := {_shell_flags(validation.cxxflags)}",
            f"host-ldflags := {_shell_flags(validation.ldflags)}",
            f"host-toolchain-fingerprint := {validation.fingerprint}",
            f"host-features := {' '.join(validation.features)}",
            "host-feature-cxxflags := $(if $(filter cpp-static-reflection,$(host-features)),-freflection-latest)",
            "",
        )
    )


def main(arguments: list[str]) -> int:
    try:
        output, requested_arch = parse_arguments(arguments)
        config = config_from_environment(os.environ, requested_arch)
        validation = validate(config)
        write_text_if_changed(output, emit_make(validation))
        print(
            f"Validated host toolchain: {validation.triple}, "
            f"{validation.cppstdlib}, sysroot={validation.sysroot}"
        )
    except (OSError, ValueError) as error:
        print(f"host.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
