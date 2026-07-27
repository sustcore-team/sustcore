from __future__ import annotations

from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[3]


class BuildSelectionTests(unittest.TestCase):
    def _run_make(
        self, environment: str, arch: str, mode: str
    ) -> subprocess.CompletedProcess[str]:
        makefile = f"""\
path-s := {ROOT / 'script'}
environment := {environment}
arch := {arch}
mode := {mode}
include $(path-s)/env/selection.mk
.PHONY: show
show:
\t@echo env=$(is-host)/$(is-freestanding)
\t@echo arch=$(is-riscv64)/$(is-loongarch64)/$(is-$(arch))
\t@echo mode=$(is-debug)/$(is-release)
\t@echo pair=$(is-$(arch)-debug)/$(is-$(arch)-release)
\t@echo envarch=$(is-host-$(arch))/$(is-freestanding-$(arch))
"""
        return subprocess.run(
            ["make", "--no-print-directory", "-f", "-", "show"],
            input=makefile,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def test_riscv64_debug_selectors(self) -> None:
        result = self._run_make("freestanding", "riscv64", "debug")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("env=n/y", result.stdout)
        self.assertIn("arch=y/n/y", result.stdout)
        self.assertIn("mode=y/n", result.stdout)
        self.assertIn("pair=y/n", result.stdout)
        self.assertIn("envarch=n/y", result.stdout)

    def test_loongarch64_release_selectors(self) -> None:
        result = self._run_make("freestanding", "loongarch64", "release")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("arch=n/y/y", result.stdout)
        self.assertIn("mode=n/y", result.stdout)
        self.assertIn("pair=n/y", result.stdout)
        self.assertIn("envarch=n/y", result.stdout)

    def test_host_native_arch_selector(self) -> None:
        result = self._run_make("host", "x86_64", "debug")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("env=y/n", result.stdout)
        self.assertIn("arch=n/n/y", result.stdout)
        self.assertIn("pair=y/n", result.stdout)
        self.assertIn("envarch=y/n", result.stdout)

    def test_invalid_freestanding_architecture_is_rejected(self) -> None:
        result = self._run_make("freestanding", "x86_64", "debug")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported architecture", result.stderr)

    def test_unselected_dimensions_allow_initialization_context(self) -> None:
        result = self._run_make("freestanding", "", "")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("arch=n/n/", result.stdout)
        self.assertIn("mode=n/n", result.stdout)


if __name__ == "__main__":
    unittest.main()
