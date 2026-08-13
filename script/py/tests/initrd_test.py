from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generators import initrd


ROOT = Path(__file__).resolve().parents[3]


class InitrdEmitterTests(unittest.TestCase):
    def test_emit_declares_module_outputs_and_cpio_recipe(self) -> None:
        fragment = initrd.emit(ROOT / "initrd" / "initrd.toml")

        self.assertIn("initrd-module-ids := usrboot", fragment)
        self.assertIn(
            "build-module-usrboot: build-hosttool | build-libs", fragment
        )
        self.assertIn(
            "build-modules: build-hosttool build-libs $(initrd-module-targets)",
            fragment,
        )
        self.assertIn("build-initrd: build-hosttool $(path-initrd)", fragment)
        self.assertIn("$(path-initrd): $(path-cache)/initrd.mk $(initrd-inputs)", fragment)
        self.assertIn("--null --create --format=newc", fragment)
        self.assertNotIn("initrd.py) \\", fragment)

    def test_emit_rejects_duplicate_destinations(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            config_path = Path(temporary_directory) / "initrd.toml"
            config_path.write_text(
                "[[initrd.file]]\n"
                "src=\"LICENSE\"\n"
                "dst=\"same\"\n\n"
                "[[initrd.file]]\n"
                "src=\"README.md\"\n"
                "dst=\"same\"\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "duplicate initrd destination"):
                initrd.emit(config_path)

    def test_emit_rejects_make_unsafe_destinations(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            config_path = Path(temporary_directory) / "initrd.toml"
            config_path.write_text(
                "[[initrd.file]]\n"
                "src=\"LICENSE\"\n"
                "dst=\"bad path\"\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "unsupported by generated Make rules"):
                initrd.emit(config_path)


if __name__ == "__main__":
    unittest.main()
