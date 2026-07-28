from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_testbench


ROOT = Path(__file__).resolve().parents[3]


class BuildTestbenchFragmentTests(unittest.TestCase):
    def _evaluate(self, environment: str, arch: str) -> dict[str, str]:
        fragment = build_testbench.emit(ROOT)
        makefile = f"""\
path-s := {ROOT / 'script'}
path-cache := /nonexistent
environment := {environment}
arch := {arch}
mode := debug
include $(path-s)/env/selection.mk
{fragment}
.PHONY: show
show:
\t@echo programs=$(testbench-program-ids)
\t@echo tests=$(testbench-test-ids)
\t@echo benches=$(testbench-bench-ids)
\t@echo examples=$(testbench-example-ids)
\t@echo checks=$(header-check-ids)
\t@echo freestanding-checks=$(freestanding-check-ids)
\t@echo all-programs=$(testbench-program-ids-all)
\t@echo all-checks=$(header-check-ids-all)
\t@echo all-freestanding-checks=$(freestanding-check-ids-all)
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

    def test_host_selects_programs_and_host_header_checks(self) -> None:
        values = self._evaluate("host", "x86_64")

        self.assertEqual(values["programs"], values["all-programs"])
        self.assertIn("tayclib-itoa-test", values["tests"].split())
        self.assertIn("taycpplib-bench", values["benches"].split())
        self.assertIn("tayclib-itoa-example", values["examples"].split())
        self.assertIn("taycpplib-expected-example", values["examples"].split())
        self.assertIn("tayclib-bits-after-system", values["checks"].split())
        self.assertIn("taycpplib-reflection", values["checks"].split())
        self.assertEqual(values["freestanding-checks"], "")

    def test_freestanding_selects_only_freestanding_header_checks(self) -> None:
        values = self._evaluate("freestanding", "riscv64")

        self.assertEqual(values["programs"], "")
        self.assertEqual(values["tests"], "")
        self.assertEqual(values["benches"], "")
        self.assertEqual(values["examples"], "")
        self.assertIn("tayclib-rtti-cpp", values["checks"].split())
        self.assertNotIn("tayclib-bits-after-system", values["checks"].split())
        self.assertNotIn("taycpplib-reflection", values["checks"].split())
        self.assertIn("tayclib-bits-after-system", values["all-checks"].split())
        self.assertIn(
            "taycpplib-panic-with-provider",
            values["freestanding-checks"].split(),
        )
        self.assertEqual(
            values["freestanding-checks"], values["all-freestanding-checks"]
        )


if __name__ == "__main__":
    unittest.main()
