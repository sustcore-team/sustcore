# Resolve the active environment once. Toolchain fragments place host and
# freestanding candidates in y-* or n-* variables and consume only y-*.
is-host := $(if $(filter host,$(environment)),y,n)
is-freestanding := $(if $(filter freestanding,$(environment)),y,n)
selected-environment-count := $(words $(filter y,$(is-host) $(is-freestanding)))
$(if $(filter 1,$(selected-environment-count)),,$(error unsupported or ambiguous build environment: $(environment)))

$(is-host)-environment-macros-cpp := -UTAY_ENV_HOST -UTAY_ENV_FREESTANDING -DTAY_ENV_HOST=1
$(is-freestanding)-environment-macros-cpp := -UTAY_ENV_HOST -UTAY_ENV_FREESTANDING -DTAY_ENV_FREESTANDING=1
environment-macros-cpp := $(y-environment-macros-cpp)

host-mode-flags-debug := -O0 -g
host-mode-flags-release := -O3 -DNDEBUG
$(if $(filter y,$(is-host)),\
    $(if $(filter debug release,$(mode)),,$(error unsupported host mode: $(mode))))
host-mode-flags := $(host-mode-flags-$(mode))
host-sanitize-compile-flags := $(if $(strip $(sanitize)),\
    -fsanitize=$(sanitize) -fno-omit-frame-pointer)
host-sanitize-link-flags := $(if $(strip $(sanitize)),-fsanitize=$(sanitize))
