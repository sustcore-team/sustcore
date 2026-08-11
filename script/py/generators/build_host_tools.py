#!/usr/bin/env python3
"""Generate Host build-tool registry, rules, and component contexts."""

from __future__ import annotations

from pathlib import Path

from generators import build_ctx
from make_support.emitter import generated_header
from metadata.registry import scan_host_tools


def emit(root: Path) -> str:
    tools = scan_host_tools(root)
    lines = [
        generated_header("script/py/generators/build_host_tools.py"),
        "",
        "host-tool-ids-all := " + " ".join(tool.id for tool in tools),
        "host-tool-ids-$(is-host) += " + " ".join(tool.id for tool in tools),
        "host-tool-ids := $(strip $(host-tool-ids-y))",
        "host-tool-build-targets := "
        + " ".join(f"build-hosttool-{tool.id}" for tool in tools),
        "",
    ]

    for tool in tools:
        lines.extend(
            (
                f"host-tool-{tool.id}-makefile := {tool.makefile}",
                f"host-tool-{tool.id}-target := {tool.target}",
                f"host-tool-{tool.id}-output := {tool.output}",
                f"host-tool-{tool.id}-path := $(path-host-tool)/{tool.output}",
                f"host-tool-{tool.id}-published-path := "
                f"$(path-host-tool-publish)/{tool.output}",
                "",
                f".PHONY: build-hosttool-{tool.id} host-tool-{tool.id} "
                f"run-host-tool-{tool.id}",
                f"build-hosttool-{tool.id}: | _prepare-build-hosttool",
                "\t$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 \\",
                "\t\tallow-target-arch=1 mode=$(mode) sanitize=$(sanitize) \\",
                f"\t\thost-tool-{tool.id}",
                "",
                f"host-tool-{tool.id}:",
                f"\t$(q)$(MAKE) -f $(host-tool-{tool.id}-makefile) \\",
                "\t\tglobal-env=$(global-env) \\",
                "\t\tenvironment=host \\",
                "\t\tallow-target-arch=$(allow-target-arch) \\",
                "\t\tmode=$(mode) sanitize=$(sanitize) q=$(q) \\",
                f"\t\thost-tool-id={tool.id} \\",
                f"\t\tctx=$(path-ctx)/{build_ctx.host_tool_name(tool.id)} \\",
                f"\t\tdeps-file=$(path-deps)/host-{tool.id}.mk \\",
                f"\t\t$(host-tool-{tool.id}-target)",
                f"\t$(q)$(mkdir) $(dir $(host-tool-{tool.id}-published-path))",
                f"\t$(q)set -e; published_tmp="
                f"$(call shq,$(host-tool-{tool.id}-published-path).tmp).$$$$; \\",
                f"\t\t$(cp) $(call shq,$(host-tool-{tool.id}-path)) "
                '"$$published_tmp"; \\',
                f"\t\t$(mv) \"$$published_tmp\" "
                f"$(call shq,$(host-tool-{tool.id}-published-path))",
                "",
                f"run-host-tool-{tool.id}: host-tool-{tool.id}",
                f"\t$(q)$(call shq,$(host-tool-{tool.id}-published-path))",
                "",
            )
        )
    return "\n".join(lines)


def emit_ctx(root: Path) -> dict[str, str]:
    return {
        build_ctx.host_tool_name(tool.id): build_ctx.emit(
            tool.id,
            tool.root,
            f"$(path-obj)/host-tool/{tool.id}",
            f"$(path-host-tool)/{tool.output}",
        )
        for tool in scan_host_tools(root)
    }
