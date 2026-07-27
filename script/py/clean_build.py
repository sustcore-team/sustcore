#!/usr/bin/env python3
"""Remove one explicitly configured build root after safety validation."""

from __future__ import annotations

from pathlib import Path
import shutil
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


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
    if len(arguments) != 1:
        print("clean_build.py: expected build-root=<absolute-path>", file=sys.stderr)
        return 1
    key, separator, value = arguments[0].partition("=")
    if key != "build-root" or not separator or not value:
        print("clean_build.py: expected build-root=<absolute-path>", file=sys.stderr)
        return 1
    try:
        clean(Path(value))
    except (OSError, ValueError) as error:
        print(f"clean_build.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
