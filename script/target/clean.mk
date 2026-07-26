.PHONY: clean cleandist

clean:
	$(q)$(rmdir) $(path-build)

cleandist:
	$(q)$(rm) $(path-cache)/clang.mk
	$(q)$(rm) $(path-cache)/config.mk
	$(q)$(rm) $(path-cache)/kernel.mk
	$(q)$(rm) $(path-cache)/libraries.mk
	$(q)$(rm) $(path-cache)/build-libs.mk
	$(q)$(rm) $(path-cache)/programs.mk
	$(q)$(rm) $(path-cache)/path.mk
	$(q)$(rm) $(path-cache)/qemu.mk
	$(q)$(rm) $(path-cache)/.configure.mk
	$(q)$(rm) $(path-cache)/build-header-*.mk
	$(q)$(rm) $(path-cache)/deps-*.mk
