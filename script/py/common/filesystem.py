"""Atomic publication helpers for generated text files."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile


def atomic_write_text(path: Path, content: str) -> None:
    """Atomically replace *path* with UTF-8 *content*."""
    path.parent.mkdir(parents=True, exist_ok=True)
    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent, text=True
    )
    try:
        with os.fdopen(file_descriptor, "w", encoding="utf-8") as temporary_file:
            temporary_file.write(content)
            temporary_file.flush()
            os.fsync(temporary_file.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def write_text_if_changed(path: Path, content: str) -> bool:
    """Atomically publish *content* unless the existing file is identical."""
    if path.is_file() and path.read_text(encoding="utf-8") == content:
        return False
    atomic_write_text(path, content)
    return True
