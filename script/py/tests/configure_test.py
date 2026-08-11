from __future__ import annotations

from pathlib import Path
from contextlib import redirect_stderr
from io import StringIO
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import configure


class ConfigureComponentContextTests(unittest.TestCase):
    def test_generate_writes_subdirectories_and_removes_stale_files(self) -> None:
        original_cache_root = configure.CACHE_ROOT
        original_config_cache = configure.CONFIG_CACHE

        with TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory)
            configure.CACHE_ROOT = cache_root
            configure.CONFIG_CACHE = cache_root / ".configure.mk"
            try:
                configure.generate("default")
                self.assertTrue((cache_root / "ctx" / "lib-mini-cstd.mk").is_file())
                self.assertTrue(
                    (cache_root / "ctx" / "module-usrboot-image.mk").is_file()
                )
                self.assertTrue(
                    (cache_root / "ctx" / "host-tool-mk-usrboot.mk").is_file()
                )
                host_tools_fragment = (cache_root / "host-tools.mk").read_text(
                    encoding="utf-8"
                )
                self.assertIn(
                    "host-tool-ids-all := mk-usrboot",
                    host_tools_fragment,
                )
                self.assertIn(
                    "host-tool-build-targets := build-hosttool-mk-usrboot",
                    host_tools_fragment,
                )
                initrd_fragment = (cache_root / "initrd.mk").read_text(encoding="utf-8")
                self.assertIn("initrd-module-ids := usrboot-image", initrd_fragment)
                self.assertFalse((cache_root / "build-header-lib-mini-cstd.mk").exists())
                self.assertFalse((cache_root / "deps-kernel.mk").exists())

                kernel_ctx = (cache_root / "ctx" / "kernel.mk").read_text(
                    encoding="utf-8"
                )
                self.assertIn("owner-id := kernel", kernel_ctx)
                self.assertIn("obj-root ?= $(path-obj)/kernel", kernel_ctx)
                self.assertIn("target ?= $(kernel-path)", kernel_ctx)

                stale_ctx = cache_root / "ctx" / "lib-obsolete.mk"
                stale_ctx.write_text("stale\n", encoding="utf-8")
                stale_ctx_state = cache_root / "ctx" / "obsolete.state"
                stale_ctx_state.write_text("stale\n", encoding="utf-8")
                legacy_ctx = cache_root / "build-header-lib-obsolete.mk"
                legacy_ctx.write_text("stale\n", encoding="utf-8")
                stale_host = cache_root / "host.mk"
                stale_host.write_text("host-arch := x86_64\n", encoding="utf-8")
                stale_host_deps = cache_root / "deps" / "host-tayclib.mk"
                stale_host_deps.write_text("stale\n", encoding="utf-8")
                legacy_deps = cache_root / "deps-host-tayclib.mk"
                legacy_deps.write_text("stale\n", encoding="utf-8")
                configure.generate("default")
                self.assertFalse(stale_ctx.exists())
                self.assertFalse(stale_ctx_state.exists())
                self.assertFalse(legacy_ctx.exists())
                self.assertFalse(stale_host.exists())
                self.assertFalse(stale_host_deps.exists())
                self.assertFalse(legacy_deps.exists())

                kernel_deps = (cache_root / "deps" / "kernel.mk").read_text(
                    encoding="utf-8"
                )
                self.assertIn("kernel-dep-ids-y +=", kernel_deps)
                self.assertIn("usrboot", kernel_deps)
                self.assertIn(
                    "kernel-dep-ids-$(is-riscv64) += sbi", kernel_deps
                )
                self.assertNotIn("kernel-dep-ids-riscv64 :=", kernel_deps)
            finally:
                configure.CACHE_ROOT = original_cache_root
                configure.CONFIG_CACHE = original_config_cache

    def test_arch_and_mode_arguments_are_accepted_but_ignored(self) -> None:
        self.assertEqual(
            configure.parse_arguments(
                ["config=default", "arch=riscv64", "mode=release"]
            ),
            ("default", ("arch", "mode")),
        )

        stderr = StringIO()
        with mock.patch.object(configure, "generate") as generate, redirect_stderr(stderr):
            result = configure.main(
                ["config=default", "arch=loongarch64", "mode=debug"]
            )

        self.assertEqual(result, 0)
        generate.assert_called_once_with("default")
        self.assertIn("warning: arch= is ignored", stderr.getvalue())
        self.assertIn("warning: mode= is ignored", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
