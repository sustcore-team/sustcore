from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generators import build_programs


ROOT = Path(__file__).resolve().parents[3]


class BuildProgramsFragmentTests(unittest.TestCase):
    def test_registry_keeps_makefile_and_build_target(self) -> None:
        registry = build_programs.emit(ROOT)

        self.assertIn(
            "program-usrboot-image-makefile := "
            + str(ROOT / "module" / "usrboot" / "Makefile"),
            registry,
        )
        self.assertIn("program-usrboot-image-target := build", registry)
        self.assertNotIn("program-usrboot-image-output", registry)

    def test_component_context_defines_module_paths(self) -> None:
        contexts = build_programs.emit_ctx(ROOT)

        context = contexts["module-usrboot-image.mk"]
        self.assertIn("owner-id := usrboot-image", context)
        self.assertIn("owner-root := " + str(ROOT / "module" / "usrboot"), context)
        self.assertIn("obj-root ?= $(path-obj)/module/usrboot-image", context)
        self.assertIn("target ?= $(path-bin)/module/usrboot", context)


if __name__ == "__main__":
    unittest.main()
