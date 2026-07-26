from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import configure


class ConfigureComponentHeaderTests(unittest.TestCase):
    def test_generate_writes_headers_and_removes_stale_ones(self) -> None:
        original_cache_root = configure.CACHE_ROOT
        original_config_cache = configure.CONFIG_CACHE

        with TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory)
            configure.CACHE_ROOT = cache_root
            configure.CONFIG_CACHE = cache_root / ".configure.mk"
            try:
                configure.generate("default", "riscv64")
                self.assertTrue((cache_root / "build-header-lib-mini-cstd.mk").is_file())
                self.assertTrue((cache_root / "build-header-module-init.mk").is_file())

                kernel_header = (cache_root / "build-header-kernel.mk").read_text(
                    encoding="utf-8"
                )
                self.assertIn("owner-id := kernel", kernel_header)
                self.assertIn("obj-root ?= $(path-obj)/kernel", kernel_header)
                self.assertIn("target ?= $(kernel-path)", kernel_header)

                stale_header = cache_root / "build-header-lib-obsolete.mk"
                stale_header.write_text("stale\n", encoding="utf-8")
                configure.generate("default", "riscv64")
                self.assertFalse(stale_header.exists())
            finally:
                configure.CACHE_ROOT = original_cache_root
                configure.CONFIG_CACHE = original_config_cache


if __name__ == "__main__":
    unittest.main()
