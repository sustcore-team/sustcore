from __future__ import annotations

from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generators import build_host_tools
from metadata.registry import scan_host_tools


ROOT = Path(__file__).resolve().parents[3]


class HostToolRegistryTests(unittest.TestCase):
    def test_scans_mk_usrboot_tool(self) -> None:
        tools = {tool.id: tool for tool in scan_host_tools(ROOT)}

        self.assertIn("mk-usrboot", tools)
        self.assertEqual(tools["mk-usrboot"].target, "build")
        self.assertEqual(tools["mk-usrboot"].output, "mk-usrboot")

    def test_rejects_duplicate_tool_ids(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            for directory in (root / "host-tool" / "first", root / "host-tool" / "second"):
                directory.mkdir(parents=True)
                (directory / "Makefile").write_text("build:\n\t@:\n", encoding="utf-8")
                (directory / "metadata.toml").write_text(
                    "[[hosttool]]\n"
                    'id = "duplicate"\n'
                    'makefile = "Makefile"\n'
                    'target = "build"\n'
                    'output = "duplicate"\n',
                    encoding="utf-8",
                )

            with self.assertRaisesRegex(ValueError, "duplicate host tool id"):
                scan_host_tools(root)


class HostToolGeneratorTests(unittest.TestCase):
    def test_aggregate_lists_every_registered_tool(self) -> None:
        with TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            for tool_id in ("first", "second"):
                directory = root / "host-tool" / tool_id
                directory.mkdir(parents=True)
                (directory / "Makefile").write_text("build:\n\t@:\n", encoding="utf-8")
                (directory / "metadata.toml").write_text(
                    "[[hosttool]]\n"
                    f'id = "{tool_id}"\n'
                    'makefile = "Makefile"\n'
                    'target = "build"\n'
                    f'output = "{tool_id}"\n',
                    encoding="utf-8",
                )

            fragment = build_host_tools.emit(root)

        self.assertIn(
            "host-tool-build-targets := build-hosttool-first build-hosttool-second",
            fragment,
        )
        self.assertIn("build-hosttool-first: | _prepare-build-hosttool", fragment)
        self.assertIn("build-hosttool-second: | _prepare-build-hosttool", fragment)

    def test_emits_build_run_and_context_rules(self) -> None:
        fragment = build_host_tools.emit(ROOT)
        contexts = build_host_tools.emit_ctx(ROOT)

        self.assertIn("host-tool-ids-all := mk-usrboot", fragment)
        self.assertIn("host-tool-build-targets := build-hosttool-mk-usrboot", fragment)
        self.assertIn(
            "build-hosttool-mk-usrboot: | _prepare-build-hosttool", fragment
        )
        self.assertIn("host-tool-mk-usrboot:", fragment)
        self.assertIn("run-host-tool-mk-usrboot: host-tool-mk-usrboot", fragment)
        self.assertIn("deps-file=$(path-deps)/host-mk-usrboot.mk", fragment)
        self.assertIn(
            "host-tool-mk-usrboot-published-path := "
            "$(path-host-tool-publish)/mk-usrboot",
            fragment,
        )
        self.assertIn("$(host-tool-mk-usrboot-published-path).tmp", fragment)
        self.assertIn("host-tool-mk-usrboot.mk", contexts)
        self.assertIn(
            "target ?= $(path-host-tool)/mk-usrboot",
            contexts["host-tool-mk-usrboot.mk"],
        )

    def test_top_level_pipeline_aggregates_tools_and_precedes_target_builds(self) -> None:
        host_layer = (ROOT / "script" / "target" / "host.mk").read_text(
            encoding="utf-8"
        )
        top_level = (ROOT / "Makefile").read_text(encoding="utf-8")

        self.assertIn("build-hosttool: $(host-tool-build-targets)", host_layer)
        self.assertIn("build-host-tools: build-hosttool", host_layer)
        self.assertIn("build-kernel: build-hosttool build-libs", top_level)


if __name__ == "__main__":
    unittest.main()
