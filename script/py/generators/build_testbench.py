#!/usr/bin/env python3
"""Generate host program, header-check, and freestanding-check dispatch metadata."""

from __future__ import annotations

from pathlib import Path

from generators import build_ctx
from make_support.emitter import generated_header
from metadata.registry import scan_testbenches


def _join(values: tuple[str, ...]) -> str:
    return " ".join(values)


def _output_root(kind: str) -> str:
    return {
        "test": "$(path-test)",
        "bench": "$(path-bench)",
        "example": "$(path-example)",
    }[kind]


def emit(root: Path) -> str:
    programs, checks, freestanding_checks = scan_testbenches(root)
    lines = [
        generated_header("script/py/build_testbench.py"),
        "",
        "testbench-program-ids-all := " + " ".join(program.id for program in programs),
        "testbench-program-ids-$(is-host) += " + " ".join(program.id for program in programs),
        "testbench-test-ids-$(is-host) += " + " ".join(program.id for program in programs if program.kind == "test"),
        "testbench-bench-ids-$(is-host) += " + " ".join(program.id for program in programs if program.kind == "bench"),
        "testbench-example-ids-$(is-host) += " + " ".join(program.id for program in programs if program.kind == "example"),
        "testbench-program-ids := $(strip $(testbench-program-ids-y))",
        "testbench-test-ids := $(strip $(testbench-test-ids-y))",
        "testbench-bench-ids := $(strip $(testbench-bench-ids-y))",
        "testbench-example-ids := $(strip $(testbench-example-ids-y))",
        "header-check-ids-all := " + " ".join(check.id for check in checks),
        "header-check-ids-$(is-host) += " + " ".join(
            check.id for check in checks if "host" in check.environments
        ),
        "header-check-ids-$(is-freestanding) += " + " ".join(
            check.id for check in checks if "freestanding" in check.environments
        ),
        "header-check-ids := $(strip $(header-check-ids-y))",
        "freestanding-check-ids-all := "
        + " ".join(check.id for check in freestanding_checks),
        "freestanding-check-ids-$(is-freestanding) += "
        + " ".join(check.id for check in freestanding_checks),
        "freestanding-check-ids := $(strip $(freestanding-check-ids-y))",
        "",
    ]
    for program in programs:
        lines.extend(
            (
                f"hostprog-{program.id}-owner := {program.owner}",
                f"hostprog-{program.id}-kind := {program.kind}",
                f"hostprog-{program.id}-makefile := {program.makefile}",
                f"hostprog-{program.id}-target := {program.target}",
                f"hostprog-{program.id}-output := {program.output}",
                f"hostprog-{program.id}-requires-features := {_join(program.requires_features)}",
                f"hostprog-{program.id}-expect := {program.expect}",
                f"hostprog-{program.id}-stderr-contains := {program.stderr_contains}",
                f"hostprog-{program.id}-stderr-equals := {program.stderr_equals}",
                f"hostprog-{program.id}-path := {_output_root(program.kind)}/{program.output}",
                "",
                f".PHONY: host-program-{program.id} run-host-program-{program.id}",
                f"host-program-{program.id}:",
                f"\t$(if $(filter-out $(host-features),{_join(program.requires_features)})," +
                f"$(q)$(echo) \"SKIP {program.id}: missing feature(s) $(filter-out $(host-features),{_join(program.requires_features)})\"," +
                "$(q)$(MAKE) -f " + program.makefile + " global-env=$(global-env) " +
                "environment=host allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) q=$(q) " +
                f"program-id={program.id} library-owner={program.owner} " +
                f"ctx=$(path-ctx)/{build_ctx.hostprog_name(program.id)} " +
                f"deps-file=$(path-deps)/host-{program.owner}.mk {program.target})",
                "",
                f"run-host-program-{program.id}: host-program-{program.id}",
                f"\t$(if $(filter-out $(host-features),{_join(program.requires_features)}),:," +
                "$(q)$(s-run-hostprog) " +
                f"program=$(hostprog-{program.id}-path) expect={program.expect} " +
                f"stderr-contains=$(call shq,$(hostprog-{program.id}-stderr-contains)) " +
                f"stderr-equals=$(call shq,$(hostprog-{program.id}-stderr-equals)))",
                "",
            )
        )
    for check in checks:
        lines.extend(
            (
                f"headercheck-{check.id}-owner := {check.owner}",
                f"headercheck-{check.id}-header := {check.header}",
                f"headercheck-{check.id}-language := {check.language}",
                f"headercheck-{check.id}-environments-all := {_join(check.environments)}",
                *(
                    f"headercheck-{check.id}-environments-$(is-{environment}) += {environment}"
                    for environment in check.environments
                ),
                f"headercheck-{check.id}-environments := $(strip $(headercheck-{check.id}-environments-y))",
                f"headercheck-{check.id}-requires-features := {_join(check.requires_features)}",
                f"headercheck-{check.id}-before-headers := {_join(check.before_headers)}",
                f"headercheck-{check.id}-after-headers := {_join(check.after_headers)}",
                "",
                f".PHONY: host-header-check-{check.id} freestanding-header-check-{check.id}",
                f"host-header-check-{check.id}:",
                f"\t$(if $(filter y,$(is-host))," +
                f"$(if $(filter-out $(host-features),{_join(check.requires_features)})," +
                f"$(q)$(echo) \"SKIP {check.id}: missing feature(s) $(filter-out $(host-features),{_join(check.requires_features)})\"," +
                "$(q)$(MAKE) -f $(path-s)/host/header-check.mk global-env=$(global-env) " +
                "environment=host allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) q=$(q) " +
                f"library-owner={check.owner} check-header={check.header} " +
                f"check-language='{check.language}' check-before='{_join(check.before_headers)}' " +
                f"check-after='{_join(check.after_headers)}' deps-file=$(path-deps)/host-{check.owner}.mk check),:)",
                "",
                f"freestanding-header-check-{check.id}:",
                f"\t$(if $(filter y,$(is-freestanding))," +
                "$(q)$(MAKE) -f $(path-s)/host/header-check.mk global-env=$(global-env) " +
                "environment=freestanding arch=$(arch) mode=$(mode) q=$(q) " +
                f"library-owner={check.owner} check-header={check.header} " +
                f"check-language='{check.language}' check-before='{_join(check.before_headers)}' " +
                f"check-after='{_join(check.after_headers)}' deps-file=$(path-deps)/{check.owner}.mk check,:)",
                "",
            )
        )
    for check in freestanding_checks:
        lines.extend(
            (
                f"freestandingcheck-{check.id}-owner := {check.owner}",
                f"freestandingcheck-{check.id}-kind := {check.kind}",
                f"freestandingcheck-{check.id}-language := {check.language}",
                f"freestandingcheck-{check.id}-sources := {_join(check.sources)}",
                f"freestandingcheck-{check.id}-expect := {check.expect}",
                f"freestandingcheck-{check.id}-root := {check.root}",
                f"freestandingcheck-{check.id}-library-root := {check.library_root}",
                "",
                f".PHONY: freestanding-check-{check.id}",
                f"freestanding-check-{check.id}:",
                f"\t$(if $(filter y,$(is-freestanding)),"
                "$(q)$(MAKE) -f $(path-s)/host/freestanding-check.mk "
                "global-env=$(global-env) environment=freestanding "
                "arch=$(arch) mode=$(mode) q=$(q) "
                f"library-owner={check.owner} check-id={check.id} "
                f"check-kind={check.kind} check-language='{check.language}' "
                f"check-expect={check.expect} check-source-root='{check.root}' "
                f"library-root='{check.library_root}' "
                f"check-sources='{_join(check.sources)}' "
                f"deps-file=$(path-deps)/{check.owner}.mk check,:)",
                "",
            )
        )
    return "\n".join(lines)


def emit_ctx(root: Path) -> dict[str, str]:
    programs, _, _ = scan_testbenches(root)
    result: dict[str, str] = {}
    for program in programs:
        result[build_ctx.hostprog_name(program.id)] = build_ctx.emit(
            program.id,
            program.root,
            f"$(path-obj)/testbench/{program.kind}/{program.id}",
            f"{_output_root(program.kind)}/{program.output}",
        )
    return result
