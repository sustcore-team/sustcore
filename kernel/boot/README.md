# Kernel Boot Layer

`kernel/boot` owns the hand-off from an architecture boot protocol to the permanent kernel. The
RISC-V64 SBI and LoongArch64 laboot backends construct the same `BootInfoHeader` ABI and enter
`__bsp_early_main()` after enabling a temporary high-half mapping and switching to the permanent BSP
stack.

The common early path is split by responsibility:

- `constructors.cpp` clears normal/init BSS and, after `HEAP_READY`, runs preinit/init arrays exactly
  once;
- `validate.cpp` validates the BootInfo pointer and size, FREE-parent/reservation chains, FDT header,
  complete FDT structure and reserved physical coverage before derived data is consumed;
- `early.cpp` copies the validated BootInfo into init storage, constructs the per-page metadata,
  publishes calculated usable areas to Buddy, enables SLUB, runs ordinary constructors, activates
  the final KernelSpace, and reclaims boot memory before transferring control to permanent
  `bsp_main()`;
- `context.cpp` owns the permanent BootInfo/FDT copies and the boot/init reclaim transitions.

The current single-CPU stage preserves BootInfo/FDT, installs the final architecture page-table root,
then removes and releases `.init.*` from permanent `bsp_main()`. Remote TLB shootdown and secondary CPU
activation remain later-stage work.

The boot layer must initialize Buddy's permanent embedded descriptor pool before publishing or
allocating physical memory. All data derived from BootInfo or FDT must be validated before use, and
anything needed after reclaim must be copied to kernel-owned storage.

## Startup milestones

The BSP startup order is deliberately linear:

1. `RESET -> EARLT_CPPRT`: normal and init BSS are clear, early traps are installed, and only the
   constant-initialized C++ bootstrap runtime is available. Ordinary constructors have not run.
2. `EARLT_CPPRT -> MEMORY_READY`: BootInfo has been validated, PageDatabase is initialized, Buddy's
   permanent descriptor pool is attached, and usable physical pages are published. The boot page
   table still supplies the KPA direct map.
3. `MEMORY_READY -> HEAP_READY`: the constant-initialized global `MixedSlabsAllocator` is probed and
   published. Allocation ABI entry points may now use SLUB/Buddy.
4. `HEAP_READY -> GLOBAL_CTORS_READY`: `.preinit_array` and `.init_array` are executed exactly once.
   Constructors may allocate, but must not assume KernelSpace or KernelMM is ready.
5. `GLOBAL_CTORS_READY -> VIRTUAL_MEMORY_READY`: BootInfo/FDT are persisted, KernelSpace adopts its
   allocated roots, KernelMM loads kernel and HHDM layouts, every managed physical region is checked
   for final HHDM coverage, and the final root is activated.

Boot-time heap allocations are valid before the final root is active because both architecture boot
page tables provide the KPA direct map. KernelMM must map every PageDatabase parent region before root
activation, which preserves those allocations across the transition.

## Pre-heap global object rule

Every global object that can be reached before `HEAP_READY` must be zero-initialized or explicitly
declared `constinit`, with a `constexpr` construction path where a constructor is needed. This is a
compile-time requirement, not merely a startup-order convention. Such an object must not:

- require an `.init_array` entry before it can be used;
- allocate memory or perform dynamic destructor registration as part of becoming usable;
- depend on BootInfo-derived state, KernelSpace, KernelMM, or another later singleton;
- hide dynamic initialization behind a function-local static.

Ordinary dynamic initialization belongs to the `HEAP_READY -> GLOBAL_CTORS_READY` phase. The kernel
linker scripts collect the input `.init_array` into the `INIT_ARRAY`-typed `.init.rodata` output
section. During bring-up, inspect it with `llvm-readelf -x .init.rodata <kernel>` and identify entries
with `llvm-nm -n <kernel>`; any object required by an earlier phase must already be usable without
those entries.

## Singleton storage policy

Singleton identity and fallible resource acquisition are separate concerns. A singleton whose object
representation has a constant-initializable empty state should be an ordinary direct static object;
an explicit readiness state publishes when its runtime resources have been attached.

The direct-static set is now:

- the global `MixedSlabsAllocator`, initialized as an empty allocator and published by `init_heap()`;
- `KernelSpace`, initialized with an empty locked PageTable and completed by adopting allocated roots;
- `KernelMM`, initialized with empty layout state and published after all bootstrap layouts load;
- the existing Buddy, including its permanent embedded descriptor pool, plus PageDatabase, logger,
  boot Context, early console, milestone/owner counters, and static-destructor registry.

There are no remaining byte-storage singleton indirections recommended for conversion in the current
kernel tree. Similar future manager objects should follow the same direct `constinit` plus explicit
`initialize()`/readiness pattern when their empty state is meaningful.

Some byte arrays are storage resources rather than singleton indirection and should remain as such:
SLUB object slots, BootInfo wire-data backing, page-table pages, and runtime Buddy descriptor-pool
backing. Converting those arrays into a direct manager object would conflate physical backing storage
with object lifetime.
