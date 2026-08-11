"""SemVer 2.0 range matching used by the dependency resolver.

Supported dependency expressions include exact versions, wildcard and partial
versions, comparators, conjunctions, logical OR, caret ranges, tilde ranges,
and hyphen ranges. Range expansion follows npm SemVer conventions.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import total_ordering
import re


_NUMERIC_RE = re.compile(r"^(0|[1-9]\d*)$")
_IDENTIFIER_RE = re.compile(r"^[0-9A-Za-z-]+$")
_COMPARATOR_RE = re.compile(r"^(>=|<=|>|<|=)?(.+)$")
_HYPHEN_RANGE_RE = re.compile(r"^(\S+)\s+-\s+(\S+)$")


@total_ordering
@dataclass(frozen=True, eq=False)
class Version:
    major: int
    minor: int
    patch: int
    prerelease: tuple[str, ...] = ()
    build: tuple[str, ...] = ()

    @property
    def core(self) -> tuple[int, int, int]:
        return self.major, self.minor, self.patch

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self.core == other.core and self.prerelease == other.prerelease

    def __hash__(self) -> int:
        return hash((self.core, self.prerelease))

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        if self.core != other.core:
            return self.core < other.core
        if not self.prerelease or not other.prerelease:
            return bool(self.prerelease) and not other.prerelease

        for left, right in zip(self.prerelease, other.prerelease):
            if left == right:
                continue
            left_numeric = left.isdigit()
            right_numeric = right.isdigit()
            if left_numeric and right_numeric:
                return int(left) < int(right)
            if left_numeric != right_numeric:
                return left_numeric
            return left < right
        return len(self.prerelease) < len(other.prerelease)


@dataclass(frozen=True)
class _PartialVersion:
    major: int | None
    minor: int | None
    patch: int | None
    specified_components: int
    prerelease: tuple[str, ...]
    build: tuple[str, ...]

    @property
    def is_full(self) -> bool:
        return self.specified_components == 3


@dataclass(frozen=True)
class _Comparator:
    operator: str
    target: Version

    def matches(self, candidate: Version) -> bool:
        if self.operator == "=":
            return candidate == self.target
        if self.operator == ">":
            return candidate > self.target
        if self.operator == ">=":
            return candidate >= self.target
        if self.operator == "<":
            return candidate < self.target
        if self.operator == "<=":
            return candidate <= self.target
        raise ValueError(f"invalid semver comparator operator: {self.operator!r}")


@dataclass(frozen=True)
class _RangeClause:
    comparators: tuple[_Comparator, ...]
    prerelease_cores: frozenset[tuple[int, int, int]]

    def matches(self, candidate: Version) -> bool:
        if not all(comparator.matches(candidate) for comparator in self.comparators):
            return False
        if candidate.prerelease and candidate.core not in self.prerelease_cores:
            return False
        return True


def _parse_identifiers(value: str, *, prerelease: bool, label: str) -> tuple[str, ...]:
    if not value:
        raise ValueError(f"invalid semver {label}")
    identifiers = tuple(value.split("."))
    for identifier in identifiers:
        if not _IDENTIFIER_RE.fullmatch(identifier):
            raise ValueError(f"invalid semver {label}: {value!r}")
        if prerelease and identifier.isdigit() and not _NUMERIC_RE.fullmatch(identifier):
            raise ValueError(f"invalid semver {label}: {value!r}")
    return identifiers


def _split_suffix(value: str, *, context: str) -> tuple[str, tuple[str, ...], tuple[str, ...]]:
    if not isinstance(value, str) or not value:
        raise ValueError(f"invalid semver {context}: {value!r}")

    core_and_prerelease, build_separator, build_value = value.partition("+")
    if build_separator and (not build_value or "+" in build_value):
        raise ValueError(f"invalid semver {context}: {value!r}")
    build = _parse_identifiers(build_value, prerelease=False, label="build metadata") if build_separator else ()

    core, prerelease_separator, prerelease_value = core_and_prerelease.partition("-")
    if prerelease_separator and not prerelease_value:
        raise ValueError(f"invalid semver {context}: {value!r}")
    prerelease = (
        _parse_identifiers(prerelease_value, prerelease=True, label="prerelease identifier")
        if prerelease_separator
        else ()
    )
    return core, prerelease, build


def _parse_numeric_component(value: str, *, context: str) -> int:
    if not _NUMERIC_RE.fullmatch(value):
        raise ValueError(f"invalid semver {context}: {value!r}")
    return int(value)


def parse_version(version: str) -> Version:
    core, prerelease, build = _split_suffix(version, context="version")
    parts = core.split(".")
    if len(parts) != 3:
        raise ValueError(f"invalid semver version: {version!r}")
    major, minor, patch = (
        _parse_numeric_component(part, context="version")
        for part in parts
    )
    return Version(major, minor, patch, prerelease, build)


def _is_wildcard_token(token: str) -> bool:
    return token in {"*", "x", "X"}


def _parse_partial_version(value: str) -> _PartialVersion:
    core, prerelease, build = _split_suffix(value, context="range version")
    parts = core.split(".")
    if not 1 <= len(parts) <= 3:
        raise ValueError(f"invalid semver range version: {value!r}")

    values: list[int | None] = []
    wildcard_seen = False
    for part in parts:
        if _is_wildcard_token(part):
            wildcard_seen = True
            values.append(None)
            continue
        if wildcard_seen:
            raise ValueError(f"invalid semver range version: {value!r}")
        values.append(_parse_numeric_component(part, context="range version"))

    if prerelease or build:
        if len(parts) != 3 or wildcard_seen:
            raise ValueError(f"invalid semver range version: {value!r}")

    specified_components = sum(part is not None for part in values)
    values.extend([None] * (3 - len(values)))
    return _PartialVersion(
        values[0],
        values[1],
        values[2],
        specified_components,
        prerelease,
        build,
    )


def _version_from_partial(partial: _PartialVersion) -> Version:
    return Version(
        partial.major or 0,
        partial.minor or 0,
        partial.patch or 0,
        partial.prerelease,
        partial.build,
    )


def _boundary_version(major: int, minor: int, patch: int) -> Version:
    return Version(major, minor, patch, ("0",))


def _xrange_upper(partial: _PartialVersion) -> Version | None:
    if partial.specified_components == 0:
        return None
    if partial.specified_components == 1:
        return _boundary_version((partial.major or 0) + 1, 0, 0)
    if partial.specified_components == 2:
        return _boundary_version(partial.major or 0, (partial.minor or 0) + 1, 0)
    return None


def _xrange_comparators(partial: _PartialVersion) -> tuple[_Comparator, ...]:
    if partial.specified_components == 0:
        return ()
    lower = _version_from_partial(partial)
    if partial.is_full:
        return (_Comparator("=", lower),)
    upper = _xrange_upper(partial)
    if upper is None:
        raise ValueError("invalid semver wildcard range")
    return _Comparator(">=", lower), _Comparator("<", upper)


def _partial_comparator(operator: str, partial: _PartialVersion) -> tuple[_Comparator, ...]:
    if partial.is_full:
        return (_Comparator(operator or "=", _version_from_partial(partial)),)
    if operator in {"", "="}:
        return _xrange_comparators(partial)
    if partial.specified_components == 0:
        if operator == ">=":
            return ()
        raise ValueError("a semver comparator cannot use only a wildcard")

    lower = _version_from_partial(partial)
    upper = _xrange_upper(partial)
    if upper is None:
        raise ValueError("invalid semver comparator range")
    if operator == ">=":
        return (_Comparator(">=", lower),)
    if operator == ">":
        return (_Comparator(">=", upper),)
    if operator == "<":
        return (_Comparator("<", _boundary_version(lower.major, lower.minor, lower.patch)),)
    if operator == "<=":
        return (_Comparator("<", upper),)
    raise ValueError(f"invalid semver comparator operator: {operator!r}")


def _caret_comparators(partial: _PartialVersion) -> tuple[_Comparator, ...]:
    if partial.specified_components == 0:
        return ()
    lower = _version_from_partial(partial)
    major = lower.major
    minor = lower.minor
    patch = lower.patch
    if major > 0 or partial.specified_components == 1:
        upper = _boundary_version(major + 1, 0, 0)
    elif minor > 0 or partial.specified_components == 2:
        upper = _boundary_version(0, minor + 1, 0)
    else:
        upper = _boundary_version(0, 0, patch + 1)
    return _Comparator(">=", lower), _Comparator("<", upper)


def _tilde_comparators(partial: _PartialVersion) -> tuple[_Comparator, ...]:
    if partial.specified_components == 0:
        return ()
    lower = _version_from_partial(partial)
    if partial.specified_components == 1:
        upper = _boundary_version(lower.major + 1, 0, 0)
    else:
        upper = _boundary_version(lower.major, lower.minor + 1, 0)
    return _Comparator(">=", lower), _Comparator("<", upper)


def _explicit_prerelease_cores(partial: _PartialVersion) -> frozenset[tuple[int, int, int]]:
    if not partial.prerelease:
        return frozenset()
    return frozenset({_version_from_partial(partial).core})


def _parse_token(token: str) -> tuple[tuple[_Comparator, ...], frozenset[tuple[int, int, int]]]:
    if token.startswith("^") or token.startswith("~"):
        operator = token[0]
        value = token[1:]
        if not value:
            raise ValueError(f"invalid semver range expression: {token!r}")
        partial = _parse_partial_version(value)
        comparators = _caret_comparators(partial) if operator == "^" else _tilde_comparators(partial)
        return comparators, _explicit_prerelease_cores(partial)

    match = _COMPARATOR_RE.fullmatch(token)
    if match is None:
        raise ValueError(f"invalid semver range expression: {token!r}")
    operator, value = match.groups()
    partial = _parse_partial_version(value)
    return _partial_comparator(operator or "", partial), _explicit_prerelease_cores(partial)


def _parse_hyphen_range(left_value: str, right_value: str) -> _RangeClause:
    left = _parse_partial_version(left_value)
    right = _parse_partial_version(right_value)
    comparators: list[_Comparator] = []
    if left.specified_components:
        comparators.append(_Comparator(">=", _version_from_partial(left)))
    if right.specified_components:
        if right.is_full:
            comparators.append(_Comparator("<=", _version_from_partial(right)))
        else:
            upper = _xrange_upper(right)
            if upper is None:
                raise ValueError("invalid semver hyphen range")
            comparators.append(_Comparator("<", upper))
    prerelease_cores = _explicit_prerelease_cores(left) | _explicit_prerelease_cores(right)
    return _RangeClause(tuple(comparators), prerelease_cores)


def _parse_clause(clause: str) -> _RangeClause:
    hyphen_range = _HYPHEN_RANGE_RE.fullmatch(clause)
    if hyphen_range is not None:
        return _parse_hyphen_range(*hyphen_range.groups())
    if " - " in clause:
        raise ValueError(f"invalid semver hyphen range: {clause!r}")

    comparators: list[_Comparator] = []
    prerelease_cores: set[tuple[int, int, int]] = set()
    for token in clause.split():
        token_comparators, token_prerelease_cores = _parse_token(token)
        comparators.extend(token_comparators)
        prerelease_cores.update(token_prerelease_cores)
    if not clause.split():
        raise ValueError(f"invalid semver clause: {clause!r}")
    return _RangeClause(tuple(comparators), frozenset(prerelease_cores))


def matches(version: str, expression: str) -> bool:
    candidate = parse_version(version)
    normalized = expression.strip()
    if not normalized:
        raise ValueError("empty semver expression")

    clauses = [clause.strip() for clause in normalized.split("||")]
    if any(not clause for clause in clauses):
        raise ValueError(f"invalid semver OR expression: {expression!r}")
    return any(_parse_clause(clause).matches(candidate) for clause in clauses)


def filter_matches(versions: list[str], expression: str) -> list[str]:
    return [version for version in versions if matches(version, expression)]
