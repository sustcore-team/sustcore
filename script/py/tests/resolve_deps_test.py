from __future__ import annotations

from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import resolve_deps
from libregistry import scan_dependency_owners


ROOT = Path(__file__).resolve().parents[3]


class DependencyEnvironmentTests(unittest.TestCase):
    def test_merges_common_environment_and_arch_and_deduplicates(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "dependencies.toml"
            path.write_text(
                '[[dependencies]]\nlib="common"\nversion="^1.0.0"\n'
                '[[host.dependencies]]\nlib="host"\nversion="~2.0"\n'
                '[[host.dependencies]]\nlib="common"\nversion="^1.0.0"\n'
                '[[x86_64.dependencies]]\nlib="arch"\nversion="*"\n',
                encoding="utf-8",
            )
            self.assertEqual(
                resolve_deps.dependency_entries(path, "host", "x86_64"),
                [("common", "^1.0.0"), ("host", "~2.0"), ("arch", "*")],
            )

    def test_rejects_conflicting_duplicate_ranges(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "dependencies.toml"
            path.write_text(
                '[[dependencies]]\nlib="same"\nversion="^1.0.0"\n'
                '[[host.dependencies]]\nlib="same"\nversion="^2.0.0"\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "conflicting version expressions"):
                resolve_deps.dependency_entries(path, "host", "x86_64")

    def test_rejects_unknown_environment(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "dependencies.toml"
            with self.assertRaisesRegex(ValueError, "unsupported environment"):
                resolve_deps.dependency_entries(path, "firmware", "riscv64")

    def test_freestanding_fragment_selects_arch_dependencies_at_make_time(self) -> None:
        owners = {owner.id: owner for owner in scan_dependency_owners(ROOT)}
        output = resolve_deps.emit(ROOT, owners["kernel"])

        self.assertIn("kernel-dep-ids-y +=", output)
        self.assertIn("kernel-dep-ids-$(is-riscv64) += sbi", output)
        self.assertIn(
            "kernel-dep-ids := $(strip $(kernel-dep-ids-y))", output
        )
        self.assertNotIn("kernel-dep-ids-riscv64 :=", output)

        with TemporaryDirectory() as temporary_directory:
            deps_path = Path(temporary_directory) / "deps.mk"
            deps_path.write_text(output, encoding="utf-8")
            makefile = Path(temporary_directory) / "Makefile"
            makefile.write_text(
                f"""\
path-s := {ROOT / 'script'}
environment := freestanding
include $(path-s)/env/selection.mk
include {deps_path}
.PHONY: show
show:
\t@echo $(kernel-dep-ids)
""",
                encoding="utf-8",
            )
            riscv = subprocess.run(
                [
                    "make",
                    "--no-print-directory",
                    "-f",
                    str(makefile),
                    "show",
                    "arch=riscv64",
                    "mode=debug",
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            loongarch = subprocess.run(
                [
                    "make",
                    "--no-print-directory",
                    "-f",
                    str(makefile),
                    "show",
                    "arch=loongarch64",
                    "mode=release",
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )

        self.assertEqual(riscv.returncode, 0, riscv.stderr)
        self.assertEqual(loongarch.returncode, 0, loongarch.stderr)
        self.assertIn("sbi", riscv.stdout.split())
        self.assertNotIn("sbi", loongarch.stdout.split())

    def test_host_fragment_uses_environment_selector(self) -> None:
        owners = {owner.id: owner for owner in scan_dependency_owners(ROOT)}
        output = resolve_deps.emit(
            ROOT, owners["taycpplib"], "x86_64", "host"
        )

        self.assertIn(
            "taycpplib-dep-ids-$(is-host) += tayclib", output
        )
        self.assertIn(
            "taycpplib-dep-ids := $(strip $(taycpplib-dep-ids-y))", output
        )


if __name__ == "__main__":
    unittest.main()
