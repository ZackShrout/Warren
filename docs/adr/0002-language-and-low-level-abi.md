# ADR-0002: Implementation Language And Low-Level ABI

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0 and Phase 1
- **Supersedes:** None
- **Superseded by:** None

## Context

BunnySoft normally uses C++23. Burrow requires a freestanding environment,
predictable generated code, explicit runtime ownership, assembly entry points,
and a plausible future compiler bootstrap on Warren. Using the entire current
hosted C++ ecosystem would introduce runtime and library assumptions Warren does
not yet provide. Pure C would reduce those assumptions but discard useful type,
namespace, compile-time, and ownership features already familiar to the project.

Assembly cannot be avoided for initial CPU state, exception vectors, context
switching, and selected architecture operations. Those boundaries must not
depend on C++ name mangling or object layout.

## Decision

Burrow uses a **documented freestanding subset of C++20** plus narrowly scoped
architecture assembly.

The kernel is built without exceptions, RTTI, hosted startup files, or an
assumed standard library. It owns every compiler-runtime helper present in the
image. Dynamic initialization requiring hidden runtime machinery is forbidden.
Individual language and library facilities remain opt-in even if the compiler
accepts them.

All assembly/C++ entry points and structures crossing an assembly, firmware,
boot, syscall, or persistent boundary use a C-compatible ABI and explicitly
specified integer widths and layouts. Architecture assembly follows the
standard platform procedure-call ABI.

Userspace may eventually support a broader C++20 environment through Warren SDK,
but it does not dictate Burrow's runtime model. Raising the language baseline is
an ADR-level change because it affects native toolchain bootstrap requirements.

## Alternatives Considered

### Full C++23

This matches current BunnySoft projects and provides useful newer features. It
raises the minimum future native compiler, makes the actual safe freestanding
surface easier to overstate, and offers little near-term benefit over a selected
C++20 subset.

### C17 plus assembly

C has a small and well-understood freestanding model and many kernel precedents.
It gives up namespaces, stronger abstraction tools, typed compile-time code, and
the established BunnySoft development style without eliminating ABI or undefined
behavior hazards.

### Rust

Rust provides valuable memory-safety and type-system properties. It would add a
different compiler/bootstrap chain and project-wide language model. It may be
evaluated later for userspace or isolated components but is not the initial
Burrow language.

### Write most low-level code in assembly

This exposes machine behavior directly but makes algorithms harder to test,
review, port, and maintain. Assembly is reserved for boundaries that require it.

## Consequences

### Benefits

- retains BunnySoft naming and structural conventions;
- permits strong types and zero-cost abstractions in freestanding code;
- keeps the future native compiler target older and narrower than C++23;
- makes low-level ABI boundaries explicit; and
- allows most target-independent logic to be host-tested.

### Costs And Risks

- “freestanding C++20 subset” requires continuous enforcement and documentation;
- compiler-generated helper calls and static initialization can surprise us;
- no standard library means Warren must provide carefully bounded primitives;
- C++ undefined behavior remains dangerous in privileged code; and
- the future native toolchain is still substantially harder than a C-only one.

### Follow-Up Work

- encode forbidden compiler features and flags in the toolchain configuration;
- add link-time auditing for unexpected runtime symbols;
- specify assembly syntax per architecture;
- provide kernel-owned fixed-width types and utilities without shadowing host
  definitions accidentally; and
- establish ABI layout tests at every language boundary.

## Revisit When

- a required feature cannot be implemented clearly within the selected subset;
- the native compiler plan makes a different baseline materially cheaper;
- compiler support renders the chosen baseline unmaintainable; or
- a memory-safe language can be introduced with a credible Warren bootstrap and
  interoperable runtime plan.
