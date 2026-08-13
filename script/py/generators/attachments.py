#!/usr/bin/env python3
"""Generate owner attachment objects and their module prerequisites."""

from __future__ import annotations

from pathlib import Path

from make_support.emitter import generated_header
from metadata.registry import OwnerMeta, scan_dependency_owners, scan_programs


def emit(root: Path) -> str:
    owners = [owner for owner in scan_dependency_owners(root) if owner.attachments]
    modules = {program.id: program for program in scan_programs(root)}
    lines = [generated_header("script/py/generators/attachments.py"), ""]
    module_targets = sorted({attachment.module_id for owner in owners for attachment in owner.attachments})
    lines.append(
        "attachment-module-targets := "
        + " ".join(f"build-module-{module_id}" for module_id in module_targets)
    )
    lines.append("")
    for owner in owners:
        object_paths = []
        for attachment in owner.attachments:
            module = modules[attachment.module_id]
            object_root = "$(path-obj)/kernel" if owner.kind == "kernel" else f"$(path-obj)/module/{owner.id}"
            object_path = f"{object_root}/attachment/{attachment.module_id}.attachment.o"
            input_path = f"$(path-bin)/{module.output}"
            object_paths.append(object_path)
            lines.extend(
                (
                    f"{owner.id}-attachment-{attachment.module_id}-input := {input_path}",
                    f"{owner.id}-attachment-{attachment.module_id}-section := {attachment.segment}",
                    f"{owner.id}-attachment-{attachment.module_id}-object := {object_path}",
                    f"{object_path}: {input_path}",
                    "",
                )
            )
        lines.append(f"{owner.id}-attachment-objects := {' '.join(object_paths)}")
        lines.append("")
    return "\n".join(lines)
