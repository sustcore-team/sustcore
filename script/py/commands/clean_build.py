#!/usr/bin/env python3
"""Remove one explicitly configured build root after safety validation."""

from __future__ import annotations

from pathlib import Path
import shutil
import sys

from common.arguments import parse_key_value_arguments
from common.paths import REPOSITORY_ROOT



def clean(build_root: Path) -> None:
    resolved = build_root.resolve()
    forbidden = {Path("/"), Path.home().resolve(), REPOSITORY_ROOT.resolve()}
    if not build_root.is_absolute():
        raise ValueError(f"build root must be absolute: {build_root}")
    if resolved in forbidden:
        raise ValueError(f"refusing to remove unsafe build root: {resolved}")
    if resolved.exists() and not resolved.is_dir():
        raise ValueError(f"build root is not a directory: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)


def main(arguments: list[str]) -> int:
    try:
        values = parse_key_value_arguments(
            arguments,
            {"build-root"},
            required_keys={"build-root"},
            non_empty_keys={"build-root"},
        )
        clean(Path(values["build-root"]))
    except (OSError, ValueError) as error:
        print(f"clean_build.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
