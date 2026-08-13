# Warren Vision

## Purpose

Warren exists to be a comprehensible, coherent operating system that can grow
from a single emulated AArch64 machine into a system capable of supporting its
own development.

It is a lifetime learning and engineering project, not an attempt to race a
commercial operating system or recreate every Unix feature. Its value comes
from building real vertical slices, understanding every boundary, and retaining
the freedom to develop an opinionated system where those opinions matter.

Warren has no market deadline and no obligation to reach feature parity with
Linux, Windows, macOS, or any other general-purpose system. That freedom lowers
the required breadth, not the standard of engineering. Warren should feel
grown-up because its contracts, ownership, failure behavior, tooling, and user
experience are coherent—not because it accumulates the largest feature list.

## Standard Of Ambition

Warren aims to be a small modern operating system, not a reconstruction of a
legacy DOS-era environment. “Modern” means using current execution modes,
firmware paths, object formats, protection mechanisms, and deliberately selected
hardware interfaces. It does not mean claiming support for every contemporary
computer or device.

The supported hardware matrix may remain narrow: one precisely defined virtual
AArch64 machine, a later virtual x86-64 machine, and eventually selected physical
systems whose firmware and devices are documented well enough to understand and
maintain. A narrow honest platform contract is more mature than vague universal
compatibility.

## North Star

The long-term Warren experience is:

1. Warren boots on supported AArch64 and x86-64 systems.
2. Burrow provides reliable process, memory, device, and security foundations.
3. `warrend` brings up a usable system through explicit, inspectable services.
4. Hare provides a coherent interactive and scripting environment.
5. Warren SDK can build, debug, and package Warren applications on Warren.
6. Meadow provides a native graphical desktop without becoming a prerequisite
   for administering or developing the system.
7. Warren can build a meaningful release of Warren from source on Warren.

This is a direction of travel. It is not a release promise or schedule.

## Identity

**BunnySoft Warren** is the product. The vocabulary is intended to be warm and
memorable while the technical interfaces remain plain and professional.

- **Burrow** is the kernel, not the entire operating system.
- **Hare** is the shell and command-language environment.
- **Meadow** is the desktop environment and its cohesive user experience.
- **`warrend`** is the system-service manager; individual service daemons may
  use descriptive names instead of forced rabbit terminology.
- **Warren SDK** is the supported developer surface: target definitions,
  headers, libraries, tools, documentation, and debugging support.
- **Forage** is the package manager. Its package model and command-line contract
  remain future design work.

The naming system should clarify ownership. It must never make error messages,
commands, or APIs harder to understand.

## Product Principles

### Comprehensible over impressive

A smaller subsystem whose invariants can be stated and tested is preferable to
a fashionable design that cannot be reasoned about locally.

Every Warren-owned line should have a purpose its maintainers can explain.
Generated code requires an understandable generator and provenance. A third-
party dependency requires a bounded contract that Warren understands; using a
compiler, firmware implementation, or authoritative ABI declaration does not
require recreating that tool or standard inside the repository.

### Observable from first light

Boot progress, exceptions, assertions, and test results must have a dependable
output path. A black screen is not a diagnostic strategy.

### Portable by boundary

Architecture independence comes from narrow, enforced interfaces around CPU,
interrupt, timer, memory-management, and platform code. It does not come from
comments promising a future port.

### Standards are leverage

Warren should use established formats, calling conventions, and familiar source
interfaces where they reduce toolchain or porting cost. ELF, architecture ABIs,
and a deliberately selected POSIX-shaped userspace are leverage, not a loss of
identity.

### Originality requires a reason

Warren may be opinionated about service management, application packaging,
system introspection, permissions, and the relationship between command-line
and graphical applications. It should not invent an incompatible integer type
or object-file format merely to be different.

### Text mode remains first-class

Meadow will be important, but the system must remain bootable, diagnosable, and
developable without it.

### Self-hosting is an architectural constraint

Native development is reached in stages. Every required host tool and runtime
assumption must be discoverable; cross-build conveniences must not silently
become permanent target requirements.

### Progress is capability-gated

Milestones complete when their observable exit criteria pass, not when a date
arrives or a subsystem has accumulated enough code.

Warren may remain unfinished indefinitely, but the last completed capability
must remain usable while the next one is developed. Refactoring is not progress
when it disconnects the only working vertical path without a bounded replacement
and equivalence test.

## Compatibility Philosophy

Warren intends to be **POSIX-informed**, not immediately POSIX-compliant.
Familiar process, file, stream, environment, and command-line concepts will
make toolchain ports tractable. Each supported interface must still be chosen
and documented deliberately.

Source portability is more valuable early than binary compatibility with
another operating system. Compatibility layers may be built later, outside the
Burrow core, when a real application justifies them.

## Explicit Non-Goals

The following are not early-project goals:

- Supporting Apple Silicon hardware directly
- Shipping on physical hardware before the emulated platform is dependable
- Simultaneously implementing AArch64 and x86-64 feature-for-feature
- Full POSIX certification
- Linux, Windows, or macOS binary compatibility
- A stable kernel-internal ABI
- Third-party kernel modules
- Symmetric multiprocessing before single-core correctness
- Networking before process, memory, and filesystem foundations are testable
- Meadow before Warren is useful through a serial or text console
- A from-scratch optimizing compiler before Warren can host a smaller compiler
- Security claims that exceed the implemented threat model

These may become goals through an explicit decision and roadmap update. They
are not forbidden forever; they are forbidden from masquerading as near-term
work.

## What Success Looks Like

Warren succeeds incrementally whenever it produces a dependable new capability
and the understanding needed to maintain it. The project is healthy when:

- the current phase has a small, demonstrable objective;
- architectural assumptions are documented and tested where possible;
- regressions can be reproduced automatically;
- distant work is not forcing unnecessary present-day complexity;
- present-day shortcuts are not silently closing required future paths; and
- working on Warren remains enjoyable enough to continue for years.
