# Warren Engineering Guard Rails

These rules protect Warren's long-term goals from attractive short-term
shortcuts. A guard rail may be changed, but only through an architectural
decision record that explains why its original concern no longer applies.

## 1. Platform And Architecture Boundaries

1. The first execution target is QEMU's AArch64 `virt` platform.
2. Direct Apple Silicon support is out of scope until Warren has mature platform
   abstractions and a specific supported machine.
3. Architecture-neutral code must not include headers from `Arch/AArch64/` or
   `Arch/X86_64/`.
4. Platform-neutral code must not access QEMU device addresses or firmware
   structures directly.
5. CPU instructions, register layouts, page-table formats, exception frames,
   interrupt controllers, and context-switch details live behind explicit
   architecture or platform interfaces.
6. Fixed-width integer types are required at hardware and persistent-format
   boundaries. Pointer-sized values use an explicit address or size type.
7. Endianness, page size, stack direction, cache-line size, and the existence
   of unaligned access must never be assumed in shared code without a checked
   contract.

The x86-64 port is not implemented in lockstep. Portability is protected through
dependency rules, compile-time checks, architecture-neutral tests, and a second-
architecture validation milestone before graphical work dominates the system.

## 2. Boot Is A Replaceable Boundary

- Burrow receives a versioned, architecture-neutral boot-information structure.
- Firmware and loader data must be translated before entering kernel core code.
- The kernel must not call UEFI boot services after ownership is transferred.
- The boot path must provide, at minimum, the physical memory map, kernel image
  description, command line, console capability, and optional initial ramdisk.
- A QEMU convenience loader may exist, but Burrow must not depend on QEMU-only
  behavior outside the platform layer.

Changing the loader must not require redesigning the kernel's memory manager.

## 3. Dependency Direction

The intended dependency direction is:

```text
applications / shell / services / desktop
                 |
        Warren system libraries
                 |
             syscalls
                 |
        Burrow kernel core
          /             \
 architecture HAL    platform/drivers
```

- Lower layers do not import higher layers.
- Drivers do not establish global policy.
- Architecture code implements mechanisms requested by kernel core.
- Kernel core consumes stable internal interfaces rather than concrete device
  implementations.
- Shared utilities remain small and may not become a miscellaneous dependency
  dumping ground.

## 4. Freestanding Means Freestanding

Burrow may not assume the presence of:

- a host operating system;
- a hosted C or C++ standard library;
- exceptions or RTTI;
- thread-safe static initialization;
- locale, environment variables, filesystem access, or wall-clock time;
- working heap allocation during early boot; or
- compiler runtime helpers that are not deliberately supplied by Warren.

Every compiler-emitted runtime symbol in the kernel image must be understood and
owned. The link must fail on unresolved or accidentally imported host symbols.

## 5. The ABI Is A Product Surface

- Architecture calling conventions are followed rather than approximated.
- Assembly/C++ boundaries use `extern "C"` and documented layouts.
- Syscalls use fixed-width, versionable data contracts and never expose C++
  object layout.
- Persistent and wire formats are independent of compiler padding.
- ABI changes require tests and a decision record once userspace exists.
- Kernel-private interfaces remain unstable until explicitly promoted.

## 6. One Core Before Many

Single-core correctness comes first. Kernel code must still avoid gratuitous
global state and document ownership so later SMP work is possible. SMP is not
enabled until interrupt routing, memory ordering, synchronization primitives,
and per-CPU state have explicit designs and tests.

Interrupt masking is not a general-purpose synchronization strategy.

## 7. User/Kernel Separation Is Real

Once user mode exists:

- all user pointers are treated as untrusted;
- copy-in/copy-out boundaries are explicit;
- syscalls validate sizes, flags, and object handles;
- a user process cannot rely on or observe a kernel address;
- permissions are checked at the resource boundary; and
- a user-process fault must not become a kernel panic.

Early bring-up code may temporarily run everything privileged, but it must be
clearly marked and removed at the user-mode milestone.

## 8. Observability Is A Prerequisite

No subsystem is considered brought up unless its failures can be distinguished.
At each applicable phase Warren must provide:

- serial output that works before dynamic allocation;
- structured log levels and subsystem tags;
- assertions with source location;
- register and exception-frame dumps;
- a panic path that cannot recurse silently;
- deterministic QEMU launch commands; and
- automated timeout-based boot tests with explicit success markers.

Debug-only instrumentation may be expensive. Release behavior must remain
defined when that instrumentation is absent.

## 9. Tests Follow The Boundary

- Pure algorithms are host-tested whenever their behavior is target-independent.
- Architecture-specific encodings and layouts receive compile-time or emulator
  tests.
- Kernel integration tests run in QEMU and terminate with machine-readable
  results.
- Every fixed bug should gain the smallest practical regression test.
- A passing build is not evidence that an unexecuted boot path works.

Host tests must not cause target code to absorb macOS APIs or data-model
assumptions.

## 10. No GUI-Led Architecture

Framebuffer experiments are welcome after First Light, but Meadow does not
become a roadmap phase until Warren can:

- run isolated user processes;
- provide usable files and streams;
- launch and supervise services;
- accept text input and provide a text console;
- build and run a small native program; and
- debug crashes without graphical facilities.

The window system, compositor, input stack, and GUI toolkit must remain separable
from Burrow policy.

## 11. Self-Hosting Is A Ladder

Cross-compilation is the supported starting point. Native development proceeds
through the stages in `SELF_HOSTING.md`; no stage may be declared complete based
on a single hand-crafted demonstration. Build descriptions should avoid host-
only shell tricks when a portable alternative is reasonable, but early tooling
must not be crippled by pretending Warren already exists.

## 12. Scope Control

- Only one roadmap phase is active at a time.
- Experiments outside that phase live on an explicit experimental path and do
  not define production interfaces.
- A new subsystem needs an owner, purpose, dependency boundary, and exit test.
- Capability gates are not waived because a later feature is more exciting.
- Code volume, TODO count, and elapsed time are not completion criteria.

## Change Checklist

Before merging a foundational change, ask:

1. Which layer owns this behavior?
2. Does shared code acquire an AArch64, QEMU, UEFI, macOS, or compiler-specific
   assumption?
3. Can failure be observed before the affected subsystem is working?
4. Does this create a public ABI or persistent format accidentally?
5. What is the smallest test that proves the contract?
6. Does the change help the current phase exit, or is it unbounded future work?
7. If it is a shortcut, where is its removal gate documented?
