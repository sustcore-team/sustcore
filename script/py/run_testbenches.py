#!/usr/bin/env python3
"""Run every selected host testbench and report aggregate results."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Callable

from libregistry import HostProgramMeta, scan_testbenches


VALID_KINDS = {"test", "bench", "example"}
VALID_MODES = {"debug", "release"}
VALID_SANITIZERS = {"", "address", "undefined", "address,undefined"}


@dataclass(frozen=True)
class ProgramResult:
    id: str
    status: str
    detail: str = ""


def parse_arguments(arguments: list[str]) -> dict[str, str]:
    allowed = {
        "root", "kind", "mode", "sanitize", "lib", "host-features",
        "make-command", "q",
    }
    values: dict[str, str] = {}
    for argument in arguments:
        key, separator, value = argument.partition("=")
        if not separator or key not in allowed:
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value

    missing = {"root", "kind", "mode"} - values.keys()
    if missing:
        raise ValueError("missing argument: " + ", ".join(sorted(missing)))
    if values["kind"] not in VALID_KINDS:
        raise ValueError("kind must be 'test', 'bench', or 'example'")
    if values["mode"] not in VALID_MODES:
        raise ValueError("mode must be 'debug' or 'release'")
    if values.get("sanitize", "") not in VALID_SANITIZERS:
        raise ValueError(f"unsupported sanitizer: {values['sanitize']!r}")
    root = Path(values["root"])
    if not root.is_dir():
        raise ValueError(f"repository root does not exist: {root}")
    return values


def select_programs(
    programs: list[HostProgramMeta], kind: str, owner: str
) -> list[HostProgramMeta]:
    return [
        program
        for program in programs
        if program.kind == kind and (not owner or program.owner == owner)
    ]


def make_command(values: dict[str, str], program: HostProgramMeta) -> list[str]:
    command = shlex.split(values.get("make-command", "make"))
    if not command:
        raise ValueError("make-command must not be empty")
    return command + [
        "--no-print-directory",
        "MAKEOVERRIDES=",
        "host-context=1",
        "allow-target-arch=1",
        f"mode={values['mode']}",
        f"sanitize={values.get('sanitize', '')}",
        f"q={values.get('q', '@')}",
        f"run-host-program-{program.id}",
    ]


def run_selected(
    values: dict[str, str],
    programs: list[HostProgramMeta],
    *,
    executor: Callable[..., subprocess.CompletedProcess[object]] = subprocess.run,
) -> list[ProgramResult]:
    features = set(values.get("host-features", "").split())
    selected = select_programs(programs, values["kind"], values.get("lib", ""))
    results: list[ProgramResult] = []

    for program in selected:
        missing = sorted(set(program.requires_features) - features)
        if missing:
            detail = "missing feature(s): " + ", ".join(missing)
            print(f"[ SKIP ] {program.id}: {detail}", flush=True)
            results.append(ProgramResult(program.id, "SKIP", detail))
            continue

        print(f"[ RUN  ] {program.id}", flush=True)
        try:
            completed = executor(make_command(values, program), check=False)
            returncode = completed.returncode
        except OSError as error:
            returncode = 1
            detail = str(error)
        else:
            detail = ""

        if returncode == 0:
            status = "PASS" if values["kind"] == "test" else "DONE"
            print(f"[ {status:<4} ] {program.id}", flush=True)
            results.append(ProgramResult(program.id, status))
        else:
            detail = detail or f"make exited with {returncode}"
            print(f"[ FAIL ] {program.id}: {detail}", flush=True)
            results.append(ProgramResult(program.id, "FAIL", detail))
    return results


def print_summary(kind: str, results: list[ProgramResult]) -> None:
    label = {
        "test": "Functionality",
        "bench": "Performance",
        "example": "Example",
    }[kind]
    print(f"\n{label} testbench summary:")
    for result in results:
        suffix = f" ({result.detail})" if result.detail else ""
        print(f"  {result.status:<4} {result.id}{suffix}")
    passed_status = "PASS" if kind == "test" else "DONE"
    passed = sum(result.status == passed_status for result in results)
    failed = sum(result.status == "FAIL" for result in results)
    skipped = sum(result.status == "SKIP" for result in results)
    print(f"  total={len(results)} passed={passed} failed={failed} skipped={skipped}")


def main(arguments: list[str]) -> int:
    try:
        values = parse_arguments(arguments)
        programs, _, _ = scan_testbenches(Path(values["root"]))
        results = run_selected(values, programs)
        print_summary(values["kind"], results)
        return 1 if any(result.status == "FAIL" for result in results) else 0
    except (OSError, ValueError) as error:
        print(f"run_testbenches.py: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
