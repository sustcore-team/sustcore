ifeq ($(program-id),taycpplib-algorithm-test)
sources-cpp := algorithm.cpp
else ifeq ($(program-id),taycpplib-range-test)
sources-cpp := range.cpp
else ifeq ($(program-id),taycpplib-owner-test)
sources-cpp := owner.cpp
else ifeq ($(program-id),taycpplib-refc-test)
sources-cpp := refc.cpp
else ifeq ($(program-id),taycpplib-units-test)
sources-cpp := units.cpp
else ifeq ($(program-id),taycpplib-panic-test)
sources-cpp := panic.cpp
else ifeq ($(program-id),taycpplib-expected-test)
sources-cpp := expected.cpp
else ifeq ($(program-id),taycpplib-expected-panic-test)
sources-cpp := expected_panic.cpp
else ifeq ($(program-id),taycpplib-string-view-test)
sources-cpp := string_view.cpp
else ifeq ($(program-id),taycpplib-sort-test)
sources-cpp := sort.cpp
else ifeq ($(program-id),taycpplib-find-test)
sources-cpp := find.cpp
else ifeq ($(program-id),taycpplib-misc-test)
sources-cpp := misc.cpp
else ifeq ($(program-id),taycpplib-allocator-test)
sources-cpp := allocator.cpp
else ifeq ($(program-id),taycpplib-allocator-panic-test)
sources-cpp := allocator_panic.cpp
else ifeq ($(program-id),taycpplib-string-test)
sources-cpp := string.cpp
else ifeq ($(program-id),taycpplib-string-panic-test)
sources-cpp := string_panic.cpp
else ifeq ($(program-id),taycpplib-list-test)
sources-cpp := list.cpp
else ifeq ($(program-id),taycpplib-map-test)
sources-cpp := map.cpp
else ifeq ($(program-id),taycpplib-set-test)
sources-cpp := set.cpp
else ifeq ($(program-id),taycpplib-container-failure-test)
sources-cpp := container_failure.cpp
else ifeq ($(program-id),taycpplib-format-test)
sources-cpp := format.cpp
else ifeq ($(program-id),taycpplib-path-test)
sources-cpp := path.cpp
else ifeq ($(program-id),taycpplib-logger-test)
sources-cpp := logger.cpp
else
$(error unsupported taycpplib host test: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
