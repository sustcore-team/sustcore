from __future__ import annotations

from contextlib import redirect_stderr
from io import StringIO
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import compile_commands


class CompileCommandsTests(unittest.TestCase):
    def test_prepare_creates_architecture_mode_directory(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            path = compile_commands.prepare_database(
                build_root, "riscv64", "debug"
            )

            self.assertEqual(
                path,
                build_root / "debug" / "riscv64" / "compile_commands.json",
            )
            self.assertTrue(path.parent.is_dir())

    def test_host_database_path_includes_triple_and_sanitizer_profile(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)

            plain = compile_commands.prepare_host_database(
                build_root, "x86_64-pc-linux-gnu", "debug"
            )
            sanitized = compile_commands.prepare_host_database(
                build_root, "x86_64-pc-linux-gnu", "release", "address,undefined"
            )

            self.assertEqual(
                plain,
                build_root / "debug" / "host" / "x86_64-pc-linux-gnu"
                / "compile_commands.json",
            )
            self.assertEqual(
                sanitized,
                build_root / "release" / "host" / "x86_64-pc-linux-gnu"
                / "sanitize" / "address-undefined" / "compile_commands.json",
            )

    def test_publish_host_database_replaces_only_after_validation(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            target = compile_commands.prepare_host_database(
                build_root, "x86_64-pc-linux-gnu", "debug"
            )
            target.write_text('[{"file": "old.cpp"}]\n', encoding="utf-8")
            invalid = target.parent / ".invalid.json"
            invalid.write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "JSON array"):
                compile_commands.publish_database(invalid, target)

            self.assertEqual(json.loads(target.read_text(encoding="utf-8")), [
                {"file": "old.cpp"}
            ])

            valid = target.parent / ".valid.json"
            valid.write_text('[{"file": "host.cpp"}]\n', encoding="utf-8")
            compile_commands.publish_database(valid, target)
            self.assertFalse(valid.exists())
            self.assertEqual(json.loads(target.read_text(encoding="utf-8")), [
                {"file": "host.cpp"}
            ])

    def test_select_host_database_copies_to_stable_entry(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            target = compile_commands.prepare_host_database(
                build_root, "x86_64-pc-linux-gnu", "debug"
            )
            target.write_text('[{"file": "host.cpp"}]\n', encoding="utf-8")

            current, selected_target, selected = (
                compile_commands.select_host_database(
                    build_root, "x86_64-pc-linux-gnu", "debug"
                )
            )

            self.assertTrue(selected)
            self.assertEqual(selected_target, target)
            self.assertEqual(json.loads(current.read_text(encoding="utf-8")), [
                {"file": "host.cpp"}
            ])

    def test_rejects_unsafe_host_triple(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            with self.assertRaisesRegex(ValueError, "invalid host triple"):
                compile_commands.host_database_path(
                    Path(temporary_directory), "../host", "debug"
                )

    def test_select_copies_database_to_build_root(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            target = compile_commands.prepare_database(
                build_root, "riscv64", "debug"
            )
            target.write_text('[{"file": "kernel.cpp"}]\n', encoding="utf-8")

            current_path, selected_target, selected = (
                compile_commands.select_database(
                    build_root, "riscv64", "debug"
                )
            )

            self.assertTrue(selected)
            self.assertEqual(selected_target, target)
            self.assertEqual(current_path, build_root / "compile_commands.json")
            self.assertFalse(current_path.is_symlink())
            self.assertEqual(
                current_path.read_text(encoding="utf-8"),
                target.read_text(encoding="utf-8"),
            )

    def test_select_replaces_current_database(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            current_path = build_root / "compile_commands.json"
            current_path.write_text("[]\n", encoding="utf-8")
            target = compile_commands.prepare_database(
                build_root, "loongarch64", "release"
            )
            target.write_text('[{"file": "laboot.cpp"}]\n', encoding="utf-8")

            compile_commands.select_database(
                build_root, "loongarch64", "release"
            )

            self.assertEqual(
                json.loads(current_path.read_text(encoding="utf-8")),
                [{"file": "laboot.cpp"}],
            )

    def test_missing_selection_removes_stale_current_database(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            current_path = build_root / "compile_commands.json"
            current_path.write_text("[]\n", encoding="utf-8")

            returned_path, _, selected = compile_commands.select_database(
                build_root, "riscv64", "debug"
            )

            self.assertFalse(selected)
            self.assertEqual(returned_path, current_path)
            self.assertFalse(current_path.exists())

    def test_invalid_database_is_not_published(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            target = compile_commands.prepare_database(
                build_root, "riscv64", "debug"
            )
            target.write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "JSON array"):
                compile_commands.select_database(
                    build_root, "riscv64", "debug"
                )

            self.assertFalse((build_root / "compile_commands.json").exists())

    def test_rejects_unsupported_selection(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            with self.assertRaisesRegex(ValueError, "unsupported architecture"):
                compile_commands.prepare_database(
                    Path(temporary_directory), "x86_64", "debug"
                )

    def test_prepare_action_rejects_empty_build_root(self) -> None:
        with redirect_stderr(StringIO()):
            result = compile_commands.main(
                ["prepare", "root=", "arch=riscv64", "mode=debug"]
            )

        self.assertEqual(result, 1)

    def test_select_action_allows_incomplete_cached_selection(self) -> None:
        with redirect_stderr(StringIO()):
            result = compile_commands.main(
                ["select", "root=build", "arch=", "mode="]
            )

        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
