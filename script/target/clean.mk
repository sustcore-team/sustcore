.PHONY: clean cleandist

clean:
	$(q)$(s-clean-build) build-root=$(call shq,$(abspath $(path-build-root)))

cleandist: clean
	$(q)$(s-clean-cache)
