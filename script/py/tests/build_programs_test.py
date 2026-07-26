from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_programs


ROOT = Path(__file__).resolve().parents[3]


class BuildProgramsFragmentTests(unittest.TestCase):
    def test_registry_keeps_makefile_and_build_target(self) -> None:
        registry = build_programs.emit(ROOT)

        self.assertIn("program-init-makefile := " + str(ROOT / "module" / "init" / "Makefile"), registry)
        self.assertIn("program-init-target := build", registry)
        self.assertNotIn("program-init-output", registry)

    def test_component_header_defines_module_paths(self) -> None:
        headers = build_programs.emit_headers(ROOT)

        header = headers["build-header-module-init.mk"]
        self.assertIn("owner-id := init", header)
        self.assertIn("owner-root := " + str(ROOT / "module" / "init"), header)
        self.assertIn("obj-root ?= $(path-obj)/module/init", header)
        self.assertIn("target ?= $(path-bin)/module/init.mod", header)


if __name__ == "__main__":
    unittest.main()
