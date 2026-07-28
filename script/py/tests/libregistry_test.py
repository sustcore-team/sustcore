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

    def test_host_build_variant_can_add_an_archive(self) -> None:
        libraries = {library.id: library for library in libregistry.scan_libraries(ROOT)}
        taycpplib = libraries["taycpplib"]

        self.assertTrue(taycpplib.is_header_only_for("freestanding"))
        self.assertFalse(taycpplib.is_header_only_for("host"))
        self.assertEqual(taycpplib.libname_for("host"), "libtaycpplib.a")
        self.assertEqual(Path(taycpplib.makefile_for("host")).name, "Makefile")
        self.assertEqual(taycpplib.target_for("host"), "build-static")


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

        libraries = {library.id: library for library in libregistry.scan_libraries(ROOT)}
        self.assertEqual(
            [Path(path).parent.name for path in libraries["tayclib"].testbench_headercheck],
            ["headercheck"],
        )

    def test_unregistered_testbench_metadata_is_ignored(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench" / "test"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n',
                encoding="utf-8",
            )
            (testbench / "metadata.toml").write_text(
                '[[hostprog]]\nid="ignored-test"\nkind="test"\n',
                encoding="utf-8",
            )

            programs, checks = libregistry.scan_testbenches(root)

            self.assertEqual(programs, [])
            self.assertEqual(checks, [])

    def test_explicit_testbench_metadata_registration(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=["testbench/metadata.toml"]\n'
                'testbench.headercheck=[]\ntestbench.bench=[]\n',
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

    def test_rejects_kind_that_does_not_match_registered_category(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench" / "test"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=["testbench/test/metadata.toml"]\n'
                'testbench.headercheck=[]\ntestbench.bench=[]\n',
                encoding="utf-8",
            )
            (testbench / "Makefile").write_text("build:\n", encoding="utf-8")
            (testbench / "metadata.toml").write_text(
                '[[hostprog]]\nid="sample-bench"\nkind="bench"\n'
                'makefile="Makefile"\ntarget="build"\noutput="sample-bench"\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "does not match testbench.test"):
                libregistry.scan_testbenches(root)

    def test_multi_library_metadata_binds_each_registered_testbench(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "multi"
            testbench = library / "testbench"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="one"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=["testbench/one.toml"]\n'
                'testbench.headercheck=[]\ntestbench.bench=[]\n'
                '[[libmeta]]\nid="two"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=["testbench/two.toml"]\n'
                'testbench.headercheck=[]\ntestbench.bench=[]\n',
                encoding="utf-8",
            )
            (testbench / "Makefile").write_text("build:\n", encoding="utf-8")
            (testbench / "one.toml").write_text(
                '[[hostprog]]\nid="one-test"\nkind="test"\n'
                'makefile="Makefile"\ntarget="build"\noutput="one-test"\n',
                encoding="utf-8",
            )
            (testbench / "two.toml").write_text(
                '[[hostprog]]\nid="two-test"\nkind="test"\n'
                'makefile="Makefile"\ntarget="build"\noutput="two-test"\n',
                encoding="utf-8",
            )

            programs, _ = libregistry.scan_testbenches(root)

            self.assertEqual(
                [(program.id, program.owner) for program in programs],
                [("one-test", "one"), ("two-test", "two")],
            )

    def test_rejects_header_checks_registered_as_tests(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            testbench = library / "testbench"
            testbench.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=["testbench/header.toml"]\n'
                'testbench.headercheck=[]\ntestbench.bench=[]\n',
                encoding="utf-8",
            )
            (testbench / "header.toml").write_text(
                '[[headercheck]]\nid="sample-header"\nheader="sample.h"\n'
                'language="c"\nenvironments=["host"]\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "testbench.headercheck"):
                libregistry.scan_testbenches(root)

    def test_testbench_table_requires_all_category_lists(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            library = root / "libs" / "sample"
            library.mkdir(parents=True)
            (library / "metadata.toml").write_text(
                '[[libmeta]]\nid="sample"\nlibname=""\nversion="1.0.0"\n'
                'testbench.test=[]\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "missing fields: headercheck, bench"):
                libregistry.scan_libraries(root)

    def test_rejects_unknown_required_feature(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown features"):
            libregistry.normalize_features(["future-cpp"], "requires-features")


if __name__ == "__main__":
    unittest.main()
