"""Shared command-line parsing for Make-facing Python entry points."""

from __future__ import annotations

from collections.abc import Collection, Sequence


def parse_key_value_arguments(
    arguments: Sequence[str],
    allowed_keys: Collection[str],
    *,
    required_keys: Collection[str] = (),
    non_empty_keys: Collection[str] = (),
) -> dict[str, str]:
    """Parse unique ``key=value`` arguments under an explicit schema."""
    allowed = set(allowed_keys)
    required = set(required_keys)
    non_empty = set(non_empty_keys)
    if not required <= allowed or not non_empty <= allowed:
        raise ValueError("argument schema contains keys outside the allowed set")

    values: dict[str, str] = {}
    for argument in arguments:
        key, separator, value = argument.partition("=")
        if not separator or key not in allowed or (key in non_empty and not value):
            raise ValueError(f"invalid argument: {argument}")
        if key in values:
            raise ValueError(f"duplicate argument: {key}")
        values[key] = value

    missing = required - values.keys()
    if missing:
        raise ValueError("missing argument: " + ", ".join(sorted(missing)))
    return values
