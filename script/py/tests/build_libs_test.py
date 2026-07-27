from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_libs


ROOT = Path(__file__).resolve().parents[3]


class BuildLibrariesFragmentTests(unittest.TestCase):
    def test_registry_and_build_targets_are_separate(self) -> None:
        registry, build_targets = build_libs.emit(ROOT)

        self.assertIn("library-ids-all :=", registry)
        self.assertIn("library-ids-$(is-freestanding-riscv64) +=", registry)
        self.assertIn("library-ids-$(is-host) += tayclib taycpplib", registry)
        self.assertIn(
            "library-tayclib-support-environments-$(is-host) += host", registry
        )
        self.assertIn("library-kmod-crt0-$(is-riscv64) :=", registry)
        self.assertNotIn("library-kmod-crt0-riscv64", registry)
        self.assertNotIn(".PHONY:", registry)
        self.assertNotIn("build-libs:", registry)

        self.assertIn(".PHONY:", build_targets)
        self.assertIn("build-libs: $$(build-lib-targets)", build_targets)
        self.assertIn(
            "build-lib-targets-$(is-freestanding-riscv64) +=", build_targets
        )
        self.assertNotIn("build-lib-targets-riscv64", build_targets)
        self.assertIn("host-build-lib-tayclib:", build_targets)
        self.assertIn("host-build-lib-targets-$(is-host) +=", build_targets)
        self.assertIn("_build-host-libs: $(host-build-lib-targets)", build_targets)
        self.assertNotIn("library-ids-all :=", build_targets)

    def _evaluate(
        self, environment: str, arch: str, mode: str = "debug"
    ) -> dict[str, str]:
        registry, build_targets = build_libs.emit(ROOT)
        makefile = f"""\
path-s := {ROOT / 'script'}
path-cache := /nonexistent
environment := {environment}
arch := {arch}
mode := {mode}
include $(path-s)/env/selection.mk
{registry}
{build_targets}
.PHONY: show
show:
\t@echo ids=$(library-ids)
\t@echo crt0=$(library-kmod-crt0)
\t@echo build-targets=$(build-lib-targets)
\t@echo host-build-targets=$(host-build-lib-targets)
\t@echo tayclib-environments=$(library-tayclib-support-environments)
"""
        result = subprocess.run(
            ["make", "--no-print-directory", "-f", "-", "show"],
            input=makefile,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return dict(line.split("=", 1) for line in result.stdout.splitlines())

    def test_riscv64_registry_is_selected_by_make_selectors(self) -> None:
        values = self._evaluate("freestanding", "riscv64")

        self.assertIn("sbi", values["ids"].split())
        self.assertEqual(values["crt0"], "arch/riscv64/crt0.o")
        self.assertIn("build-lib-sbi", values["build-targets"].split())
        self.assertEqual(values["host-build-targets"], "")
        self.assertEqual(values["tayclib-environments"], "freestanding")

    def test_loongarch64_registry_is_selected_by_make_selectors(self) -> None:
        values = self._evaluate("freestanding", "loongarch64", "release")

        self.assertNotIn("sbi", values["ids"].split())
        self.assertEqual(values["crt0"], "arch/loongarch64/crt0.o")
        self.assertNotIn("build-lib-sbi", values["build-targets"].split())
        self.assertEqual(values["tayclib-environments"], "freestanding")

    def test_host_registry_is_selected_by_environment_selector(self) -> None:
        values = self._evaluate("host", "x86_64")

        self.assertEqual(values["ids"].split(), ["tayclib", "taycpplib"])
        self.assertEqual(values["crt0"], "")
        self.assertEqual(values["build-targets"], "")
        self.assertEqual(
            values["host-build-targets"].split(), ["host-build-lib-tayclib"]
        )
        self.assertEqual(values["tayclib-environments"], "host")

    def test_component_contexts_define_library_paths(self) -> None:
        contexts = build_libs.emit_ctx(ROOT)

        context = contexts["lib-mini-cstd.mk"]
        self.assertIn("owner-id := mini-cstd", context)
        self.assertIn("owner-root := " + str(ROOT / "libs" / "mincstd"), context)
        self.assertIn("obj-root ?= $(path-obj)/libs/mini-cstd", context)
        self.assertIn("target ?= $(path-bin)/libs/libmini-cstd.a", context)

        header_only = contexts["lib-mini-cppstd.mk"]
        self.assertIn("target ?= \n", header_only)


if __name__ == "__main__":
    unittest.main()
