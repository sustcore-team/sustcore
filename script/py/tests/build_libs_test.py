from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_libs


ROOT = Path(__file__).resolve().parents[3]


class BuildLibrariesFragmentTests(unittest.TestCase):
    def test_registry_and_build_targets_are_separate(self) -> None:
        registry, build_targets = build_libs.emit(ROOT)

        self.assertIn("library-ids :=", registry)
        self.assertIn("library-ids-riscv64 :=", registry)
        self.assertNotIn(".PHONY:", registry)
        self.assertNotIn("build-libs:", registry)

        self.assertIn(".PHONY:", build_targets)
        self.assertIn("build-libs: $$(build-lib-targets-$(arch))", build_targets)
        self.assertIn("build-lib-targets-riscv64 :=", build_targets)
        self.assertNotIn("library-ids :=", build_targets)

    def test_component_headers_define_library_paths(self) -> None:
        headers = build_libs.emit_headers(ROOT)

        header = headers["build-header-lib-mini-cstd.mk"]
        self.assertIn("owner-id := mini-cstd", header)
        self.assertIn("owner-root := " + str(ROOT / "libs" / "mincstd"), header)
        self.assertIn("obj-root ?= $(path-obj)/libs/mini-cstd", header)
        self.assertIn("target ?= $(path-bin)/libs/libmini-cstd.a", header)

        header_only = headers["build-header-lib-mini-cppstd.mk"]
        self.assertIn("target ?= \n", header_only)


if __name__ == "__main__":
    unittest.main()
