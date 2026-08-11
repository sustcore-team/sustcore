from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]


class LinkTargetRuleTests(unittest.TestCase):
    def test_linker_rule_supports_an_intermediate_target(self) -> None:
        rule = (ROOT / "script" / "rules" / "ld.mk").read_text(encoding="utf-8")

        self.assertIn("ld-target ?= $(target)", rule)
        self.assertIn("$(ld-target):", rule)
        self.assertIn("$(archives)", rule)

    def test_archive_rule_supports_an_intermediate_target(self) -> None:
        rule = (ROOT / "script" / "rules" / "ar.mk").read_text(encoding="utf-8")
        layer = (ROOT / "script" / "build" / "static-library.mk").read_text(
            encoding="utf-8"
        )

        self.assertIn("ar-target ?= $(target)", rule)
        self.assertIn("$(ar-target):", rule)
        self.assertIn("build-static: $(ar-target)", layer)


if __name__ == "__main__":
    unittest.main()
