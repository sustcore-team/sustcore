from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import libregistry


ROOT = Path(__file__).resolve().parents[3]


class LibraryEnvironmentTests(unittest.TestCase):
    def test_missing_environment_field_defaults_to_freestanding(self) -> None:
        libraries = {library.id: library for library in libregistry.scan_libraries(ROOT)}
        self.assertEqual(libraries["mini-cstd"].support_environments, ("freestanding",))
        self.assertEqual(
            libraries["tayclib"].support_environments, ("freestanding", "host")
        )

    def test_support_archs_only_restricts_freestanding(self) -> None:
        library = libregistry.LibraryMeta(
            id="sample", version="1.0.0", root="/sample", kind="library",
            libname="", makefile="", target="", include_c="", include_cpp="",
            include_asm="", support_archs=("riscv64",), arch_ldscripts={},
            arch_crt0={}, arch_crti={}, arch_crtn={}, metadata_path="metadata.toml",
            support_environments=("freestanding", "host"),
        )
        self.assertFalse(library.supports("freestanding", "loongarch64"))
        self.assertTrue(library.supports("host", "x86_64"))


class TestbenchSchemaTests(unittest.TestCase):
    def test_scans_programs_and_header_checks_with_derived_owners(self) -> None:
        programs, checks = libregistry.scan_testbenches(ROOT)
        self.assertIn("tayclib-itoa-test", {program.id for program in programs})
        panic = next(program for program in programs if program.id == "taycpplib-panic-test")
        self.assertEqual((panic.owner, panic.expect), ("taycpplib", "abort"))
        self.assertEqual(Path(panic.root).name, "test")
        benchmark = next(
            program for program in programs if program.id == "taycpplib-bench"
        )
        self.assertEqual((benchmark.kind, Path(benchmark.root).name), ("bench", "bench"))
        reflection = next(check for check in checks if check.id == "taycpplib-reflection")
        self.assertEqual(reflection.requires_features, ("cpp-static-reflection",))

    def test_root_level_testbench_metadata_remains_supported(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n',
                encoding="utf-8",
            )
            (testbench / "Makefile").write_text("build:\n", encoding="utf-8")
            (testbench / "metadata.toml").write_text(
                '[[hostprog]]\nid="sample-test"\nkind="test"\n'
                'makefile="Makefile"\ntarget="build"\noutput="sample-test"\n',
                encoding="utf-8",
            )

            programs, _ = libregistry.scan_testbenches(root)

            self.assertEqual([(program.id, program.kind) for program in programs], [
                ("sample-test", "test")
            ])

    def test_rejects_kind_that_does_not_match_split_directory(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench" / "test"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n',
                encoding="utf-8",
            )
            (testbench / "Makefile").write_text("build:\n", encoding="utf-8")
            (testbench / "metadata.toml").write_text(
                '[[hostprog]]\nid="sample-bench"\nkind="bench"\n'
                'makefile="Makefile"\ntarget="build"\noutput="sample-bench"\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "does not match testbench/test"):
                libregistry.scan_testbenches(root)

    def test_rejects_ambiguous_multi_library_testbench_owner(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "multi"
            testbench = library / "testbench"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="one"\nlibname=""\nversion="1.0.0"\n'
                '[[libmeta]]\nid="two"\nlibname=""\nversion="1.0.0"\n',
                encoding="utf-8",
            )
            (testbench / "metadata.toml").write_text("hostprog=[]\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exactly one libmeta"):
                libregistry.scan_testbenches(root)

    def test_rejects_unknown_required_feature(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown features"):
            libregistry.normalize_features(["future-cpp"], "requires-features")


if __name__ == "__main__":
    unittest.main()
