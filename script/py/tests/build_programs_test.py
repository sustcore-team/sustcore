from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generators import build_programs
from generators import attachments
from metadata.registry import parse_kernel_owner


ROOT = Path(__file__).resolve().parents[3]


class BuildProgramsFragmentTests(unittest.TestCase):
    def test_kernel_metadata_registers_attachment(self) -> None:
        kernel = parse_kernel_owner(ROOT)

        self.assertEqual(kernel.id, "kernel")
        self.assertEqual(kernel.output, "sustcore.bin")
        self.assertEqual(kernel.target, "build")
        self.assertEqual(
            [(item.module_id, item.segment) for item in kernel.attachments],
            [("usrboot", ".rodata.usrboot")],
        )

    def test_attachment_fragment_connects_module_to_kernel_object(self) -> None:
        fragment = attachments.emit(ROOT)

        self.assertIn(
            "kernel-attachment-objects := "
            "$(path-obj)/kernel/attachment/usrboot.attachment.o",
            fragment,
        )
        self.assertIn(
            "$(path-obj)/kernel/attachment/usrboot.attachment.o: "
            "$(path-bin)/module/usrboot",
            fragment,
        )
        self.assertIn("attachment-module-targets := build-module-usrboot", fragment)
        self.assertIn(
            "kernel-attachment-usrboot-section := .rodata.usrboot",
            fragment,
        )

    def test_registry_keeps_makefile_and_build_target(self) -> None:
        registry = build_programs.emit(ROOT)

        self.assertIn(
            "program-usrboot-makefile := "
            + str(ROOT / "module" / "usrboot" / "Makefile"),
            registry,
        )
        self.assertIn("program-usrboot-target := build", registry)
        self.assertNotIn("program-usrboot-output", registry)

    def test_component_context_defines_module_paths(self) -> None:
        contexts = build_programs.emit_ctx(ROOT)

        context = contexts["module-usrboot.mk"]
        self.assertIn("owner-id := usrboot", context)
        self.assertIn("owner-root := " + str(ROOT / "module" / "usrboot"), context)
        self.assertIn("obj-root ?= $(path-obj)/module/usrboot", context)
        self.assertIn("target ?= $(path-bin)/module/usrboot", context)


if __name__ == "__main__":
    unittest.main()
