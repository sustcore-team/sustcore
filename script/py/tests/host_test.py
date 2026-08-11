from __future__ import annotations

from contextlib import redirect_stderr
from io import StringIO
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import host


ROOT = Path(__file__).resolve().parents[3]


def completed(
    command: list[str], stdout: str = "", stderr: str = ""
) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(command, 0, stdout, stderr)


class FakeHostCommands:
    def __init__(self, sysroot: Path, resource_dir: Path) -> None:
        self.sysroot = sysroot
        self.resource_dir = resource_dir
        self.c_triple = "x86_64-pc-linux-gnu"
        self.cpp_triple = "x86_64-pc-linux-gnu"
        self.machine = "x86_64"
        self.c_is_clang = True
        self.cpp_is_clang = True
        self.ar_is_llvm = True
        self.provider = "libstdc++"
        self.extra_include: Path | None = None
        self.commands: list[list[str]] = []

    def run(
        self, command: list[str] | tuple[str, ...], *, input_text: str | None = None
    ) -> subprocess.CompletedProcess[str]:
        command = list(command)
        self.commands.append(command)
        executable = Path(command[0]).name

        if command[-1:] == ["--version"]:
            if "ar" in executable:
                value = "LLVM version 18.1.3\n" if self.ar_is_llvm else "GNU ar 2.42\n"
            else:
                value = "clang version 18.1.3\n"
            return completed(command, value)

        if command[-1:] == ["-print-target-triple"]:
            triple = self.cpp_triple if "++" in executable else self.c_triple
            return completed(command, triple + "\n")

        if command[-1:] == ["-print-resource-dir"]:
            return completed(command, str(self.resource_dir) + "\n")

        if "-dM" in command:
            if input_text and "cstddef" in input_text:
                macro = "__GLIBCXX__" if self.provider == "libstdc++" else "_LIBCPP_VERSION"
                return completed(command, f"#define {macro} 1\n")
            is_clang = self.cpp_is_clang if "++" in executable else self.c_is_clang
            output = "#define __clang__ 1\n" if is_clang else "#define __GNUC__ 13\n"
            return completed(command, output)

        if "-v" in command and "-E" in command:
            paths = [self.resource_dir, self.sysroot / "usr" / "include"]
            if self.extra_include is not None:
                paths.append(self.extra_include)
            search = ["#include <...> search starts here:"]
            search.extend(f" {path}" for path in paths)
            search.append("End of search list.")
            return completed(command, stderr="\n".join(search) + "\n")

        return completed(command)


class HostConfigurationTests(unittest.TestCase):
    def test_normalizes_common_architecture_aliases(self) -> None:
        self.assertEqual(host.normalize_arch("amd64"), "x86_64")
        self.assertEqual(host.normalize_arch("ARM64"), "aarch64")
        self.assertEqual(host.normalize_arch("riscv64"), "riscv64")

    def test_environment_requires_absolute_existing_sysroot(self) -> None:
        with self.assertRaisesRegex(ValueError, "absolute directory"):
            host.config_from_environment({"HOST_SYSROOT": "relative"}, None)

    def test_environment_rejects_managed_flag_overrides(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            environment = {
                "HOST_SYSROOT": temporary_directory,
                "HOST_CXXFLAGS": "--sysroot=/other",
            }
            with self.assertRaisesRegex(ValueError, "must not override HOST_SYSROOT"):
                host.config_from_environment(environment, None)

    def test_environment_parses_flags_and_provider(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            config = host.config_from_environment(
                {
                    "HOST_SYSROOT": temporary_directory,
                    "HOST_CPPSTDLIB": "libstdc++",
                    "HOST_CXXFLAGS": "--gcc-install-dir='/gcc 13' -O2",
                },
                "amd64",
            )
            self.assertEqual(config.cppstdlib, "libstdc++")
            self.assertEqual(config.cxxflags, ("--gcc-install-dir=/gcc 13", "-O2"))
            self.assertEqual(config.requested_arch, "x86_64")

    def test_parses_verbose_include_search(self) -> None:
        output = """\
#include <...> search starts here:
 /sdk/usr/include
 /clang/include (framework directory)
End of search list.
"""
        self.assertEqual(
            host.parse_include_search(output),
            [Path("/sdk/usr/include"), Path("/clang/include")],
        )


class HostValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.sysroot = self.root / "sysroot"
        self.resource_dir = self.root / "resource"
        (self.sysroot / "usr" / "include").mkdir(parents=True)
        self.resource_dir.mkdir()
        self.fake = FakeHostCommands(self.sysroot, self.resource_dir)
        self.config = host.HostConfig(
            clang="clang-18",
            clangxx="clang++-18",
            llvm_ar="llvm-ar",
            sysroot=self.sysroot,
            cppstdlib="libstdc++",
            cflags=("-O2",),
            cxxflags=("--gcc-install-dir=" + str(self.sysroot / "gcc"),),
            ldflags=("-Wl,--as-needed",),
            requested_arch="x86_64",
        )
        (self.sysroot / "gcc").mkdir()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def validate(self) -> host.HostValidation:
        def resolve(value: str, _label: str) -> str:
            return "/tools/" + value

        with (
            patch.object(host, "_run", self.fake.run),
            patch.object(host, "_resolve_tool", resolve),
            patch.object(host.platform, "machine", lambda: self.fake.machine),
        ):
            return host.validate(self.config)

    def test_validates_and_emits_stable_make_configuration(self) -> None:
        validation = self.validate()
        make_fragment = host.emit_make(validation)

        self.assertEqual(validation.arch, "x86_64")
        self.assertEqual(validation.cppstdlib, "libstdc++")
        self.assertIn("environment := host", make_fragment)
        self.assertIn("host-triple := x86_64-pc-linux-gnu", make_fragment)
        self.assertIn("host-sysroot-flag := --sysroot=$(host-sysroot)", make_fragment)
        self.assertIn("-std=c++23", make_fragment)
        self.assertEqual(len(validation.fingerprint), 64)

        compiler_probes = [
            command
            for command in self.fake.commands
            if command[0].startswith("/tools/clang") and command[-1] != "--version"
        ]
        self.assertTrue(compiler_probes)
        expected_sysroot = f"--sysroot={self.sysroot}"
        self.assertTrue(all(expected_sysroot in command for command in compiler_probes))

    def test_rejects_c_and_cpp_target_mismatch(self) -> None:
        self.fake.cpp_triple = "aarch64-unknown-linux-gnu"
        with self.assertRaisesRegex(ValueError, "compiler target mismatch"):
            self.validate()

    def test_rejects_native_architecture_mismatch(self) -> None:
        self.fake.machine = "aarch64"
        with self.assertRaisesRegex(ValueError, "uname is aarch64"):
            self.validate()

    def test_rejects_requested_architecture_mismatch(self) -> None:
        self.config = host.HostConfig(
            **{**self.config.__dict__, "requested_arch": "riscv64"}
        )
        with self.assertRaisesRegex(ValueError, "requested riscv64"):
            self.validate()

    def test_rejects_non_clang_compiler(self) -> None:
        self.fake.c_is_clang = False
        with self.assertRaisesRegex(ValueError, "HOST_CLANG is not a Clang"):
            self.validate()

    def test_rejects_non_llvm_archiver(self) -> None:
        self.fake.ar_is_llvm = False
        with self.assertRaisesRegex(ValueError, "not an LLVM archiver"):
            self.validate()

    def test_rejects_standard_library_mismatch(self) -> None:
        self.fake.provider = "libc++"
        with self.assertRaisesRegex(ValueError, "standard library mismatch"):
            self.validate()

    def test_rejects_include_search_outside_configured_roots(self) -> None:
        self.fake.extra_include = self.root / "other-sdk" / "include"
        self.fake.extra_include.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "escapes HOST_SYSROOT"):
            self.validate()

    def test_main_does_not_replace_output_when_validation_fails(self) -> None:
        output = self.root / "host.mk"
        output.write_text("previous\n", encoding="utf-8")
        environment = {"HOST_SYSROOT": str(self.sysroot)}
        with (
            patch.dict(os.environ, environment, clear=True),
            patch.object(host, "validate", side_effect=ValueError("probe failed")),
            redirect_stderr(StringIO()),
        ):
            result = host.main(["validate", f"output={output}"])

        self.assertEqual(result, 1)
        self.assertEqual(output.read_text(encoding="utf-8"), "previous\n")


class HostMakeFragmentTests(unittest.TestCase):
    def _run_make(self, cache_root: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
        makefile = f"""\
path-e := {ROOT}
path-s := $(path-e)/script
path-cache := {cache_root}
path-build-root := /tmp/sustcore-build
include $(path-s)/env/host-buildpath.mk
include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk
include $(path-s)/toolchain/ld.mk
.PHONY: show
show:
\t@echo environment=$(environment)
\t@echo is-host=$(is-host)
\t@echo is-freestanding=$(is-freestanding)
\t@echo arch=$(arch)
\t@echo path-build=$(path-build)
\t@echo comp-cpp=$(comp-cpp)
\t@echo flags-cpp=$(flags-cpp)
\t@echo environment-macros-cpp=$(environment-macros-cpp)
"""
        return subprocess.run(
            ["make", "--no-print-directory", "-f", "-", "show", *arguments],
            input=makefile,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def test_host_buildpath_and_toolchains_use_validated_values(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory)
            (cache_root / "host.mk").write_text(
                """\
environment := host
host-arch := x86_64
host-triple := x86_64-pc-linux-gnu
host-comp-c := clang
host-comp-cpp := clang++
host-comp-ar := llvm-ar
host-sysroot := /
host-sysroot-flag := --sysroot=$(host-sysroot)
host-cflags := -O2
host-cxxflags := -std=c++23
host-ldflags := -fuse-ld=lld
""",
                encoding="utf-8",
            )
            result = self._run_make(cache_root)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("environment=host", result.stdout)
        self.assertIn("is-host=y", result.stdout)
        self.assertIn("is-freestanding=n", result.stdout)
        self.assertIn("arch=x86_64", result.stdout)
        self.assertIn(
            "path-build=/tmp/sustcore-build/debug/host/x86_64-pc-linux-gnu",
            result.stdout,
        )
        self.assertIn("comp-cpp=clang++", result.stdout)
        self.assertIn("flags-cpp=--sysroot=/ -std=c++23", result.stdout)
        self.assertIn("-DTAY_ENV_HOST=1", result.stdout)

    def test_host_buildpath_rejects_arch_command_line_variable(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory)
            (cache_root / "host.mk").write_text(
                "environment := host\nhost-arch := x86_64\nhost-triple := native\n",
                encoding="utf-8",
            )
            result = self._run_make(cache_root, "arch=riscv64")

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("host builds do not accept arch=", result.stderr)

    def test_freestanding_toolchain_selects_only_freestanding_candidates(self) -> None:
        makefile = f"""\
path-e := {ROOT}
path-s := $(path-e)/script
environment := freestanding
arch := riscv64
mode := debug
riscv64-comp-c := target-clang
riscv64-comp-cpp := target-clang++
riscv64-comp-ld := target-ld
riscv64-flags-c := -mcpu=test-cpu
riscv64-flags-cpp := -mcpu=test-cpu
include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk
include $(path-s)/toolchain/ld.mk
.PHONY: show
show:
\t@echo is-host=$(is-host)
\t@echo is-freestanding=$(is-freestanding)
\t@echo comp-c=$(comp-c)
\t@echo comp-cpp=$(comp-cpp)
\t@echo comp-ld=$(comp-ld)
\t@echo flags-c=$(flags-c)
\t@echo flags-cpp=$(flags-cpp)
"""
        result = subprocess.run(
            ["make", "--no-print-directory", "-f", "-", "show"],
            input=makefile,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("is-host=n", result.stdout)
        self.assertIn("is-freestanding=y", result.stdout)
        self.assertIn("comp-c=target-clang", result.stdout)
        self.assertIn("comp-cpp=target-clang++", result.stdout)
        self.assertIn("comp-ld=target-ld", result.stdout)
        self.assertIn(
            "-ffreestanding -mcpu=test-cpu -target riscv64-unknown-elf",
            result.stdout,
        )
        self.assertIn("-std=c++23 -ffreestanding -nostdlib++", result.stdout)
        self.assertNotIn("--sysroot", result.stdout)


if __name__ == "__main__":
    unittest.main()
