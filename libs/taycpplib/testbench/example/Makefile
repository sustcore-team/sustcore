ifeq ($(program-id),taycpplib-range-example)
sources-cpp := range.cpp
else ifeq ($(program-id),taycpplib-refc-example)
sources-cpp := refc.cpp
else ifeq ($(program-id),taycpplib-owner-example)
sources-cpp := owner.cpp
else ifeq ($(program-id),taycpplib-expected-example)
sources-cpp := expected.cpp
else ifeq ($(program-id),taycpplib-panic-example)
sources-cpp := panic.cpp
else ifeq ($(program-id),taycpplib-string-example)
sources-cpp := string.cpp
else ifeq ($(program-id),taycpplib-allocator-example)
sources-cpp := allocator.cpp
else ifeq ($(program-id),taycpplib-array_list-example)
sources-cpp := array_list.cpp
else ifeq ($(program-id),taycpplib-intrusive-list-example)
sources-cpp := intrusive_list.cpp
else ifeq ($(program-id),taycpplib-map-example)
sources-cpp := map.cpp
else ifeq ($(program-id),taycpplib-set-example)
sources-cpp := set.cpp
else ifeq ($(program-id),taycpplib-format-example)
sources-cpp := format.cpp
else ifeq ($(program-id),taycpplib-path-example)
sources-cpp := path.cpp
else ifeq ($(program-id),taycpplib-logger-example)
sources-cpp := logger.cpp
else ifeq ($(program-id),taycpplib-storage-example)
sources-cpp := storage.cpp
else ifeq ($(program-id),taycpplib-static-vector-example)
sources-cpp := static_vector.cpp
else ifeq ($(program-id),taycpplib-array-example)
sources-cpp := array.cpp
else ifeq ($(program-id),taycpplib-bitmap-example)
sources-cpp := bitmap.cpp
else ifeq ($(program-id),taycpplib-fifo-example)
sources-cpp := fifo.cpp
else ifeq ($(program-id),taycpplib-function-wrappers-example)
sources-cpp := function_wrappers.cpp
else ifeq ($(program-id),taycpplib-flat-containers-example)
sources-cpp := flat_containers.cpp
else ifeq ($(program-id),taycpplib-slot-map-example)
sources-cpp := slot_map.cpp
else ifeq ($(program-id),taycpplib-intrusive-tree-example)
sources-cpp := intrusive_tree.cpp
else ifeq ($(program-id),taycpplib-binary-search-example)
sources-cpp := binary_search.cpp
else ifeq ($(program-id),taycpplib-heap-example)
sources-cpp := heap.cpp
else ifeq ($(program-id),taycpplib-guard-example)
sources-cpp := guard.cpp
else ifeq ($(program-id),taycpplib-unique-ptr-example)
sources-cpp := unique_ptr.cpp
else ifeq ($(program-id),taycpplib-lock-example)
sources-cpp := lock.cpp
else
$(error unsupported taycpplib host example: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
