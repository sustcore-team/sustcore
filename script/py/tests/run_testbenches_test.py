from __future__ import annotations

from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import subprocess
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from libregistry import HostProgramMeta
import run_testbenches


ROOT = Path(__file__).resolve().parents[3]


def program(
    program_id: str,
    *,
    kind: str = "test",
    owner: str = "sample",
    features: tuple[str, ...] = (),
) -> HostProgramMeta:
    return HostProgramMeta(
        id=program_id,
        owner=owner,
        root=str(ROOT),
        metadata_path=str(ROOT / "metadata.toml"),
        kind=kind,
        makefile=str(ROOT / "Makefile"),
        target="build",
        output=program_id,
        requires_features=features,
    )


def values(**overrides: str) -> dict[str, str]:
    result = {
        "root": str(ROOT),
        "kind": "test",
        "mode": "debug",
        "sanitize": "",
        "lib": "",
        "host-features": "",
        "make-command": "make",
        "q": "@",
    }
    result.update(overrides)
    return result


class RunTestbenchesTests(unittest.TestCase):
    def test_continues_after_failure_and_reports_every_test(self) -> None:
        calls: list[list[str]] = []
        returncodes = iter((0, 2, 0))

        def execute(command: list[str], *, check: bool) -> subprocess.CompletedProcess[object]:
            self.assertFalse(check)
            calls.append(command)
            return subprocess.CompletedProcess(command, next(returncodes))

        with redirect_stdout(StringIO()):
            results = run_testbenches.run_selected(
                values(), [program("one"), program("two"), program("three")],
                executor=execute,
            )

        self.assertEqual([result.status for result in results], ["PASS", "FAIL", "PASS"])
        self.assertEqual(len(calls), 3)
        self.assertTrue(calls[1][-1].endswith("run-host-program-two"))

    def test_missing_feature_is_skipped_without_invoking_make(self) -> None:
        def unexpected(*_args: object, **_kwargs: object) -> subprocess.CompletedProcess[object]:
            self.fail("executor should not be called for a skipped program")

        with redirect_stdout(StringIO()):
            results = run_testbenches.run_selected(
                values(),
                [program("reflection", features=("cpp-static-reflection",))],
                executor=unexpected,
            )

        self.assertEqual(results[0].status, "SKIP")
        self.assertIn("cpp-static-reflection", results[0].detail)

    def test_filters_by_kind_and_owner(self) -> None:
        calls: list[list[str]] = []

        def execute(command: list[str], *, check: bool) -> subprocess.CompletedProcess[object]:
            calls.append(command)
            return subprocess.CompletedProcess(command, 0)

        with redirect_stdout(StringIO()):
            results = run_testbenches.run_selected(
                values(kind="bench", lib="selected", mode="release"),
                [
                    program("test", owner="selected"),
                    program("other-bench", kind="bench", owner="other"),
                    program("selected-bench", kind="bench", owner="selected"),
                ],
                executor=execute,
            )

        self.assertEqual([(result.id, result.status) for result in results], [
            ("selected-bench", "DONE")
        ])
        self.assertEqual(len(calls), 1)

    def test_runs_examples_as_demonstrations(self) -> None:
        calls: list[list[str]] = []

        def execute(command: list[str], *, check: bool) -> subprocess.CompletedProcess[object]:
            calls.append(command)
            return subprocess.CompletedProcess(command, 0)

        with redirect_stdout(StringIO()):
            results = run_testbenches.run_selected(
                values(kind="example"),
                [program("demo", kind="example")],
                executor=execute,
            )

        self.assertEqual(results, [run_testbenches.ProgramResult("demo", "DONE")])
        self.assertEqual(len(calls), 1)

    def test_functionality_summary_lists_statuses_and_counts(self) -> None:
        output = StringIO()
        with redirect_stdout(output):
            run_testbenches.print_summary(
                "test",
                [
                    run_testbenches.ProgramResult("ok", "PASS"),
                    run_testbenches.ProgramResult("bad", "FAIL", "failed"),
                    run_testbenches.ProgramResult("future", "SKIP", "missing feature"),
                ],
            )

        summary = output.getvalue()
        self.assertIn("Functionality testbench summary", summary)
        self.assertIn("PASS ok", summary)
        self.assertIn("FAIL bad", summary)
        self.assertIn("total=3 passed=1 failed=1 skipped=1", summary)


if __name__ == "__main__":
    unittest.main()
