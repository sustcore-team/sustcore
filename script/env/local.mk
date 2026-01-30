include $(global-env)

path-f := $(firstword $(MAKEFILE_LIST))
# 去除dir函数末尾的斜杠
path-d := $(patsubst %/,%,$(dir $(path-f)))
