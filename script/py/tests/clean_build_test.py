from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import clean_build


class CleanBuildTests(unittest.TestCase):
    def test_clean_removes_exact_build_root(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            parent = Path(temporary_directory)
            build_root = parent / "build"
            build_root.mkdir()
            (build_root / "artifact").write_text("generated\n")

            clean_build.clean(build_root)

            self.assertFalse(build_root.exists())
            self.assertTrue(parent.exists())

    def test_clean_rejects_dangerous_roots(self) -> None:
        for path in (Path("/"), Path.home(), clean_build.REPOSITORY_ROOT):
            with self.subTest(path=path), self.assertRaisesRegex(
                ValueError, "unsafe build root"
            ):
                clean_build.clean(path)

    def test_clean_rejects_relative_path(self) -> None:
        with self.assertRaisesRegex(ValueError, "must be absolute"):
            clean_build.clean(Path("build"))


if __name__ == "__main__":
    unittest.main()
