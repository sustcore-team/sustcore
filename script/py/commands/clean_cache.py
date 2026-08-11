#!/usr/bin/env python3
"""Remove generated build state while preserving the persisted selection."""

from __future__ import annotations

from pathlib import Path
import shutil
import sys

from common.paths import CACHE_ROOT

PRESERVED_NAMES = {".switch.mk"}


def clean(cache_root: Path = CACHE_ROOT) -> None:
    if not cache_root.exists():
        return
    if not cache_root.is_dir():
        raise ValueError(f"cache path is not a directory: {cache_root}")

    for path in cache_root.iterdir():
        if path.name in PRESERVED_NAMES:
            continue
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()


def main(arguments: list[str]) -> int:
    if arguments:
        print("clean_cache.py: no arguments are accepted", file=sys.stderr)
        return 1
    try:
        clean()
    except (OSError, ValueError) as error:
        print(f"clean_cache.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
