from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from common.arguments import parse_key_value_arguments
from common.filesystem import write_text_if_changed
from make_support.emitter import make_value, multiline_assignment


class ArgumentParsingTests(unittest.TestCase):
    def test_parses_schema_and_preserves_empty_optional_values(self) -> None:
        self.assertEqual(
            parse_key_value_arguments(
                ["root=build", "mode="],
                {"root", "mode"},
                required_keys={"root"},
            ),
            {"root": "build", "mode": ""},
        )

    def test_rejects_duplicate_and_empty_required_values(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate argument: root"):
            parse_key_value_arguments(
                ["root=a", "root=b"], {"root"}, required_keys={"root"}
            )
        with self.assertRaisesRegex(ValueError, "invalid argument: root="):
            parse_key_value_arguments(
                ["root="],
                {"root"},
                required_keys={"root"},
                non_empty_keys={"root"},
            )


class GeneratedFileTests(unittest.TestCase):
    def test_write_text_if_changed_preserves_unchanged_file(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "generated.mk"
            self.assertTrue(write_text_if_changed(output, "value := first\n"))
            original_stat = output.stat()
            self.assertFalse(write_text_if_changed(output, "value := first\n"))
            self.assertEqual(output.stat().st_mtime_ns, original_stat.st_mtime_ns)
            self.assertTrue(write_text_if_changed(output, "value := second\n"))


class MakeEmissionTests(unittest.TestCase):
    def test_escapes_literal_make_values(self) -> None:
        self.assertEqual(make_value("$value # note"), "$$value \\# note")

    def test_renders_multiline_assignment(self) -> None:
        self.assertEqual(
            multiline_assignment("items", ["one", "two"]),
            ["items := \\", "\tone \\", "\ttwo", ""],
        )


if __name__ == "__main__":
    unittest.main()
