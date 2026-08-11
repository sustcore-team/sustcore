"""Locations derived from the checked-out repository layout."""

from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
SCRIPT_ROOT = REPOSITORY_ROOT / "script"
CACHE_ROOT = SCRIPT_ROOT / ".cache"
