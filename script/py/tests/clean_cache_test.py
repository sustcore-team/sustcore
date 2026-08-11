from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import clean_cache


class CleanCacheTests(unittest.TestCase):
    def test_clean_preserves_only_switch_selection(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory)
            (cache_root / ".switch.mk").write_text("cached-arch := riscv64\n")
            (cache_root / ".configure.mk").write_text("cached-config := default\n")
            deps_root = cache_root / "deps"
            deps_root.mkdir()
            (deps_root / "kernel.mk").write_text("generated\n")
            ctx_root = cache_root / "ctx"
            ctx_root.mkdir()
            (ctx_root / "kernel.mk").write_text("generated\n")
            nested = cache_root / "temporary"
            nested.mkdir()
            (nested / "state").write_text("generated\n")

            clean_cache.clean(cache_root)

            self.assertEqual(
                [path.name for path in cache_root.iterdir()], [".switch.mk"]
            )


if __name__ == "__main__":
    unittest.main()
