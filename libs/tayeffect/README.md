# tayeffect

`tayeffect` is a header-only, allocation-free C++23 library for typed deferred
effects and CRTP plan-perform-commit operations. It supports host and
freestanding builds and depends only on `taycpplib`.

## Deferred effects

A request declares the effect tag and result it expects from a handler:

```cpp
struct console_write {};

struct println_request {
    using effect_type = console_write;
    using value_type = void;
    const char* text;
};
```

`perform<Error>()` stores the request. `then()` lazily binds another effectful
step, `transform()` lazily applies a pure function, and `or_else()` supplies a
typed recovery program after failure. None of these calls executes a request.

```cpp
auto program = tay::effect::then(
    tay::effect::perform<error>(read_request{}),
    [](int value) {
        return tay::effect::perform<error>(println_request{format(value)});
    });

auto result = tay::effect::endpoint<policy>::close(
    std::move(program), handler);
```

`endpoint::close()` is the execution boundary. Its policy must handle every
tag in the program's deduplicated `effects_type`. A handler overload must be
`noexcept` and return exactly
`tay::expected<Request::value_type, Error>`. An `effectful` program is move-only
and can only be closed as an rvalue, so ownership and consumption are explicit.

The library does not allocate and does not use exceptions, RTTI, virtual
dispatch, coroutines, or a dynamic handler stack. The program is a concrete
request/bind/transform type that the compiler can inline.

## Staged operations

Derive a stateless operation from `tay::staged::operation<Derived>` and provide:

```cpp
using plan_type = ...;
using receipt_type = ...;  // may be void
using result_type = ...;   // may be void
using error_type = ...;

tay::expected<plan_type, error_type> plan(args...) const;
tay::expected<receipt_type, error_type> perform(const plan_type&) const;
result_type commit(const plan_type&, receipt_type&&) const noexcept;
```

`execute(args...)` perfect-forwards its arguments to `plan()` exactly once.
`perform()` and `commit()` receive only the immutable plan. For a void receipt,
the receipt argument is omitted; for a void result, commit returns void.
Commit is required to be infallible and `noexcept`.

`perform()` may instead return an `effectful<receipt_type, error_type, ...>`.
In that case the derived operation also declares an endpoint and handler:

```cpp
using effect_endpoint = tay::effect::endpoint<irq_policy>;

irq_handler effect_handler(const plan_type&) const noexcept;
```

Alternatively, `perform()` may return a compile-time Env program whose
`run(env)` result is exactly `tay::expected<receipt_type, error_type>`. The
operation then provides the environment:

```cpp
auto perform(const plan_type& plan) const {
    return tay::effect::program<UnmaskOp>{plan.irq};
}

IrqManager& effect_environment(const plan_type& plan) const noexcept;
```

The CRTP executor closes a tagged program or runs an Env program before
commit. Plan, perform, handler, or environment failure skips commit. This
retains the direct API:

```cpp
constinit handler_enable enable_handler{};
const auto result = enable_handler.execute(desc, irq, manager);
```

See `testbench/example/` for effect composition, a plain staged operation, and
an IRQ-style operation whose effect is closed automatically.

## Compile-time Env programs

For small host-side or embedded workflows, the library also provides
`tay::effect::program<Operation>` and `flat_map_op`. Each operation is a
concrete template type and receives an environment only at `run()` time:

```cpp
struct ReadOp {
    template <typename Env>
    auto run(Env& env) const { return env.readLine(); }
};

auto readLine() { return tay::effect::program<ReadOp>{}; }

auto program = readLine().flatMap([](const std::string& line) {
    return printLine(normalize(line));
});

ConsoleEnv env;
program.run(env);
```

This model has no virtual functions, type erasure, allocation, or runtime
effect registry. `flatMap` stores the exact lambda type in the nested program.
It is intentionally separate from the tagged `effectful` endpoint model: use
the Env form when the environment itself is the intended execution boundary,
and use `effectful` when a subsystem must statically restrict and close named
kernel effects.
