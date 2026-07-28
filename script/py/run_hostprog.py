#!/usr/bin/env python3
"""Run one host testbench program and enforce its metadata expectation."""

from __future__ import annotations

from pathlib import Path
import os
import signal
import subprocess
import sys


def main(arguments: list[str]) -> int:
    try:
        values: dict[str, str] = {}
        for argument in arguments:
            key, separator, value = argument.partition("=")
            if not separator or key not in {
                "program", "expect", "stderr-contains", "stderr-equals"
            }:
                raise ValueError(f"invalid argument: {argument}")
            values[key] = value
        program = Path(values.get("program", ""))
        if not program.is_file():
            raise ValueError(f"program does not exist: {program}")
        expect = values.get("expect", "success")
        if expect not in {"success", "abort"}:
            raise ValueError(f"unsupported expectation: {expect!r}")
        environment = os.environ.copy()
        environment.setdefault("ASAN_OPTIONS", "detect_leaks=0")
        result = subprocess.run(
            [str(program)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=environment,
        )
        if result.stdout:
            print(result.stdout, end="")
        expected_code = 0 if expect == "success" else -signal.SIGABRT
        if result.returncode != expected_code:
            raise ValueError(
                f"{program.name} exited with {result.returncode}, expected {expected_code}: "
                f"{result.stderr.strip()}"
            )
        contains = values.get("stderr-contains", "")
        equals = values.get("stderr-equals", "")
        if contains and contains not in result.stderr:
            raise ValueError(f"{program.name} stderr does not contain {contains!r}")
        if equals and result.stderr != equals:
            raise ValueError(f"{program.name} stderr did not match the expected text")
        if result.stderr and expect == "success":
            print(result.stderr, end="", file=sys.stderr)
    except (OSError, ValueError) as error:
        print(f"run_hostprog.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
