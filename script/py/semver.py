#!/usr/bin/env python3
"""Minimal semver matcher used by the dependency resolver.

Supported in this implementation:
- exact versions, for example ``1.2.3``
- wildcards, for example ``*``, ``*.*.*``, ``1.*.*``, ``1.x.x``
- comparators, for example ``>=1.2.3`` or ``<2.0.0``
- range conjunctions separated by whitespace, for example ``>=1.2.0 <2.0.0``
- logical OR via ``||``

Not implemented yet:
- prerelease versions, for example ``1.2.3-alpha.1``
- build metadata, for example ``1.2.3+build.7``
- caret ranges, for example ``^1.2.3``
- tilde ranges, for example ``~1.2.3``
- hyphen ranges
"""

from __future__ import annotations

from dataclasses import dataclass
import re


_VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
_COMPARATOR_RE = re.compile(r"^(>=|<=|=|>|<)(.+)$")


@dataclass(frozen=True, order=True)
class Version:
    major: int
    minor: int
    patch: int


def parse_version(version: str) -> Version:
    if not _VERSION_RE.fullmatch(version):
        raise ValueError(f"invalid semver version: {version!r}")
    major, minor, patch = (int(part) for part in version.split("."))
    return Version(major, minor, patch)


def _is_wildcard_token(token: str) -> bool:
    return token in {"*", "x", "X"}


def _match_wildcard(version: Version, expression: str) -> bool:
    token = expression.strip()
    if _is_wildcard_token(token):
        return True

    parts = token.split(".")
    if len(parts) != 3:
        raise ValueError(f"invalid semver wildcard expression: {expression!r}")

    values = (version.major, version.minor, version.patch)
    for part, value in zip(parts, values):
        if _is_wildcard_token(part):
            continue
        if not part.isdigit():
            raise ValueError(f"invalid semver wildcard expression: {expression!r}")
        if int(part) != value:
            return False
    return True


def _match_comparator(version: Version, expression: str) -> bool:
    token = expression.strip()

    if any(marker in token for marker in ("^", "~")):
        raise ValueError(f"unsupported semver operator in expression: {expression!r}")

    comparator = _COMPARATOR_RE.fullmatch(token)
    if comparator is None:
        if any(_is_wildcard_token(part) for part in token.split(".")) or token == "*":
            return _match_wildcard(version, token)
        target = parse_version(token)
        return version == target

    operator, rhs = comparator.groups()
    if any(_is_wildcard_token(part) for part in rhs.split(".")) or rhs == "*":
        raise ValueError(f"wildcards are not allowed with comparators: {expression!r}")

    target = parse_version(rhs)
    if operator == "=":
        return version == target
    if operator == ">":
        return version > target
    if operator == ">=":
        return version >= target
    if operator == "<":
        return version < target
    if operator == "<=":
        return version <= target
    raise ValueError(f"invalid semver comparator: {expression!r}")


def matches(version: str, expression: str) -> bool:
    candidate = parse_version(version)
    normalized = expression.strip()
    if not normalized:
        raise ValueError("empty semver expression")

    or_clauses = [clause.strip() for clause in normalized.split("||")]
    if any(not clause for clause in or_clauses):
        raise ValueError(f"invalid semver OR expression: {expression!r}")

    for clause in or_clauses:
        comparators = clause.split()
        if not comparators:
            raise ValueError(f"invalid semver clause: {expression!r}")
        if all(_match_comparator(candidate, comparator) for comparator in comparators):
            return True
    return False


def filter_matches(versions: list[str], expression: str) -> list[str]:
    return [version for version in versions if matches(version, expression)]
