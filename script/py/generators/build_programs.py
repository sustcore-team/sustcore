#!/usr/bin/env python3
"""Generate the global program registry from program metadata."""

from __future__ import annotations

from pathlib import Path

from generators import build_ctx
from make_support.emitter import generated_header
from metadata.registry import OwnerMeta, scan_programs


def emit(root: Path) -> str:
    programs = scan_programs(root)
    lines = [
        generated_header("script/py/build_programs.py"),
        "",
        f"program-ids := {' '.join(program.id for program in programs)}",
        "",
    ]
    for program in programs:
        lines.extend(
            (
                f"program-{program.id}-makefile := {program.makefile}",
                f"program-{program.id}-target := {program.target}",
                "",
            )
        )
    return "\n".join(lines)


def emit_ctx(root: Path) -> dict[str, str]:
    """Return module build contexts keyed by their cache file names."""
    return {
        build_ctx.module_name(program.id): build_ctx.emit(
            program.id,
            program.root,
            f"$(path-obj)/module/{program.id}",
            f"$(path-bin)/{program.output}",
        )
        for program in scan_programs(root)
    }
