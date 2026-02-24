# 标志位
flags-ld  ?=
libraries-ld ?=

ifneq ($(script-ld),)
script-ld-arg := -T $(script-ld)
else
script-ld-arg :=
endif

define ld-link
	$(q)$(compiler-ld)  -L"$(path-e)/build/bin/libs/$(architecture)" $(flags-ld) $(script-ld-arg) -o $(1) $(2) --start-group $(libraries-ld) --end-group
endef