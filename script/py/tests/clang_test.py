from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from config_emitters import clang


class ClangConfigurationTests(unittest.TestCase):
    def test_emits_target_and_host_configuration_separately(self) -> None:
        output = clang.emit(
            {
                "riscv64": {"clang": "target-clang"},
                "host": {
                    "clang": "host-clang",
                    "clang++": "host-clang++",
                    "ar": "host-ar",
                    "sysroot": "/sdk",
                    "cppstdlib": "libc++",
                    "cflags": ["-O2"],
                    "cxxflags": ["-stdlib=libc++"],
                    "ldflags": ["-fuse-ld=lld"],
                },
            }
        )

        self.assertIn("riscv64-comp-c := target-clang", output)
        self.assertIn("host-config-clang := host-clang", output)
        self.assertIn("host-config-clangxx := host-clang++", output)
        self.assertIn("host-config-sysroot := /sdk", output)
        self.assertNotIn("host-comp-c :=", output)

    def test_routes_common_target_flags_through_freestanding_selection(self) -> None:
        output = clang.emit(
            {"flags": {"clang": ["-Wall"], "clang++": ["-Wextra"]}}
        )

        self.assertIn("freestanding-config-flags-c := -Wall", output)
        self.assertIn("freestanding-config-flags-cpp := -Wextra", output)
        self.assertNotIn("\nflags-c :=", output)
        self.assertNotIn("\nflags-cpp :=", output)

    def test_rejects_unknown_host_field(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported host field"):
            clang.emit({"host": {"sys-root": "/"}})

    def test_rejects_non_string_flag_entries(self) -> None:
        with self.assertRaisesRegex(ValueError, "array of strings"):
            clang.emit({"host": {"cflags": ["-O2", 1]}})


if __name__ == "__main__":
    unittest.main()
