"""Emit per-component Make build contexts."""

from __future__ import annotations

from make_support.emitter import generated_header


def library_name(owner_id: str) -> str:
    return f"lib-{owner_id}.mk"


def module_name(owner_id: str) -> str:
    return f"module-{owner_id}.mk"


def hostprog_name(owner_id: str) -> str:
    return f"hostprog-{owner_id}.mk"


def host_tool_name(owner_id: str) -> str:
    return f"host-tool-{owner_id}.mk"


def kernel_name() -> str:
    return "kernel.mk"


def emit(owner_id: str, owner_root: str, obj_root: str, target: str) -> str:
    return "\n".join(
        (
            generated_header("script/py/configure.py"),
            "",
            f"owner-id := {owner_id}",
            f"owner-root := {owner_root}",
            f"obj-root ?= {obj_root}",
            f"target ?= {target}",
            "",
        )
    )
