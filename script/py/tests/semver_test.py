from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from semver import filter_matches, matches, parse_version


class ParseVersionTests(unittest.TestCase):
    def test_parses_complete_versions(self) -> None:
        cases = {
            "0.0.0": (0, 0, 0, (), ()),
            "1.2.3": (1, 2, 3, (), ()),
            "1.2.3-alpha.1": (1, 2, 3, ("alpha", "1"), ()),
            "1.2.3-alpha.1+build.007": (
                1,
                2,
                3,
                ("alpha", "1"),
                ("build", "007"),
            ),
        }

        for source, expected in cases.items():
            with self.subTest(source=source):
                version = parse_version(source)
                self.assertEqual(
                    (version.major, version.minor, version.patch, version.prerelease, version.build),
                    expected,
                )

    def test_rejects_invalid_complete_versions(self) -> None:
        cases = (
            "",
            "1",
            "1.2",
            "1.2.3.4",
            "01.2.3",
            "1.02.3",
            "1.2.03",
            "1.2.3-",
            "1.2.3-alpha..1",
            "1.2.3-01",
            "1.2.3+",
            "1.2.3+build+again",
            "1.2.3+build..1",
            "1.2.3-alpha_1",
        )

        for source in cases:
            with self.subTest(source=source):
                with self.assertRaises(ValueError):
                    parse_version(source)


class VersionPrecedenceTests(unittest.TestCase):
    def test_semver_prerelease_precedence(self) -> None:
        ordered_versions = [
            parse_version(source)
            for source in (
                "1.0.0-alpha",
                "1.0.0-alpha.1",
                "1.0.0-alpha.beta",
                "1.0.0-beta",
                "1.0.0-beta.2",
                "1.0.0-beta.11",
                "1.0.0-rc.1",
                "1.0.0",
            )
        ]

        self.assertEqual(ordered_versions, sorted(reversed(ordered_versions)))
        for lower, higher in zip(ordered_versions, ordered_versions[1:]):
            with self.subTest(lower=lower, higher=higher):
                self.assertLess(lower, higher)

    def test_build_metadata_does_not_affect_equality_or_precedence(self) -> None:
        plain = parse_version("1.2.3")
        with_build = parse_version("1.2.3+build.5")
        other_build = parse_version("1.2.3+build.6")

        self.assertEqual(plain, with_build)
        self.assertEqual(with_build, other_build)
        self.assertEqual(hash(plain), hash(with_build))
        self.assertFalse(plain < with_build)
        self.assertFalse(plain > with_build)


class RangeMatchingTests(unittest.TestCase):
    def assert_matches(self, expression: str, *versions: str) -> None:
        for version in versions:
            with self.subTest(expression=expression, version=version):
                self.assertTrue(matches(version, expression))

    def assert_excludes(self, expression: str, *versions: str) -> None:
        for version in versions:
            with self.subTest(expression=expression, version=version):
                self.assertFalse(matches(version, expression))

    def test_exact_versions_ignore_build_metadata(self) -> None:
        self.assert_matches("1.2.3", "1.2.3", "1.2.3+build.1")
        self.assert_excludes("1.2.3", "1.2.2", "1.2.4", "1.2.3-alpha")

    def test_wildcard_and_partial_ranges(self) -> None:
        self.assert_matches("*", "0.0.0", "2.4.6")
        self.assert_matches("1", "1.0.0", "1.99.99")
        self.assert_excludes("1", "0.99.99", "2.0.0")
        self.assert_matches("1.2", "1.2.0", "1.2.99")
        self.assert_excludes("1.2", "1.1.99", "1.3.0")
        self.assert_matches("1.x", "1.0.0", "1.8.5")
        self.assert_excludes("1.x", "2.0.0")
        self.assert_matches("1.2.x", "1.2.0", "1.2.5")
        self.assert_excludes("1.2.x", "1.3.0")

    def test_comparator_ranges_and_conjunctions(self) -> None:
        self.assert_matches(">=1.2.3 <2.0.0", "1.2.3", "1.9.9")
        self.assert_excludes(">=1.2.3 <2.0.0", "1.2.2", "2.0.0")
        self.assert_matches(">1.2", "1.3.0", "2.0.0")
        self.assert_excludes(">1.2", "1.2.0", "1.2.99")
        self.assert_matches("<=1.2", "0.9.9", "1.2.99")
        self.assert_excludes("<=1.2", "1.3.0")
        self.assert_matches("<1.2", "0.9.9", "1.1.99")
        self.assert_excludes("<1.2", "1.2.0")

    def test_logical_or_ranges(self) -> None:
        self.assert_matches("1.2.x || >=2.0.0 <2.1.0", "1.2.4", "2.0.0", "2.0.9")
        self.assert_excludes("1.2.x || >=2.0.0 <2.1.0", "1.3.0", "2.1.0")

    def test_caret_ranges(self) -> None:
        self.assert_matches("^1.2.3", "1.2.3", "1.9.9")
        self.assert_excludes("^1.2.3", "1.2.2", "2.0.0")
        self.assert_matches("^0.2.3", "0.2.3", "0.2.99")
        self.assert_excludes("^0.2.3", "0.3.0")
        self.assert_matches("^0.0.3", "0.0.3")
        self.assert_excludes("^0.0.3", "0.0.4")
        self.assert_matches("^0.0", "0.0.0", "0.0.99")
        self.assert_excludes("^0.0", "0.1.0")

    def test_tilde_ranges(self) -> None:
        self.assert_matches("~1", "1.0.0", "1.99.99")
        self.assert_excludes("~1", "2.0.0")
        self.assert_matches("~1.2", "1.2.0", "1.2.99")
        self.assert_excludes("~1.2", "1.3.0")
        self.assert_matches("~1.2.3", "1.2.3", "1.2.99")
        self.assert_excludes("~1.2.3", "1.3.0")

    def test_hyphen_ranges(self) -> None:
        self.assert_matches("1.2.3 - 2.3.4", "1.2.3", "2.3.4")
        self.assert_excludes("1.2.3 - 2.3.4", "1.2.2", "2.3.5")
        self.assert_matches("1.2 - 2.3", "1.2.0", "2.3.99")
        self.assert_excludes("1.2 - 2.3", "1.1.99", "2.4.0")
        self.assert_matches("1 - 2", "1.0.0", "2.99.99")
        self.assert_excludes("1 - 2", "0.99.99", "3.0.0")

    def test_prereleases_require_an_explicit_prerelease_comparator(self) -> None:
        self.assert_excludes(">=1.2.3 <2.0.0", "1.3.0-alpha", "1.2.4-beta.1")
        self.assert_matches(">=1.2.3-beta.2 <2.0.0", "1.2.3-beta.2", "1.2.3-beta.9")
        self.assert_excludes(">=1.2.3-beta.2 <2.0.0", "1.2.3-beta.1", "1.2.4-beta.1")

    def test_filter_matches_preserves_input_order(self) -> None:
        versions = ["2.0.0", "1.2.4", "1.3.0", "1.2.3", "1.2.3-alpha"]

        self.assertEqual(filter_matches(versions, "1.2.x"), ["1.2.4", "1.2.3"])


class InvalidRangeTests(unittest.TestCase):
    def test_rejects_invalid_range_expressions(self) -> None:
        expressions = (
            "",
            "   ",
            "1.2.3 ||",
            "|| 1.2.3",
            "^",
            "~",
            ">*",
            "1.*.3",
            "1.2.3.4",
            "1.2.3-",
            "1.2 -",
            "1.2.3 - 2.3.4 - 3.4.5",
            ">= 1.2.3",
        )

        for expression in expressions:
            with self.subTest(expression=expression):
                with self.assertRaises(ValueError):
                    matches("1.2.3", expression)

    def test_rejects_invalid_candidate_versions(self) -> None:
        with self.assertRaises(ValueError):
            matches("1.2", "*")


if __name__ == "__main__":
    unittest.main()
