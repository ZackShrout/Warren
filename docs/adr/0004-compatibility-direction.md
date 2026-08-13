# ADR-0004: Compatibility Direction

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 3–6
- **Supersedes:** None
- **Superseded by:** None

## Context

Warren needs compilers, linkers, build tools, shells, editors, and applications
before native development is credible. Inventing every binary and source
interface would make each port larger. Copying another operating system's full
ABI would allow more reuse but constrain Warren's own resource, service, and
application models before their requirements are understood.

## Decision

Warren is **source-compatible by selective adoption and POSIX-informed design**.

It adopts established object formats and architecture ABIs, beginning with
ELF64, AAPCS64, and conventional C data models. Userspace will provide familiar
files, byte streams, processes, arguments, environment, and command-line
behavior where they offer porting leverage.

Warren does not initially promise POSIX conformance or binary compatibility with
Linux, BSD, macOS, or Windows. Its syscall ABI and kernel resource model are
Warren-owned. Compatibility functions belong in system libraries or explicit
compatibility layers rather than distorting Burrow without a demonstrated need.

## Alternatives Considered

### Full POSIX conformance as an initial requirement

This would provide a clear target and broad source portability. It adds a large
surface before Warren can implement or test its semantics and risks letting
certification goals design the kernel.

### Linux syscall ABI compatibility

This could unlock existing binaries or simplify ports. It is a broad, evolving
contract tied to Linux semantics and would constrain Warren before its object
model is chosen.

### Entirely original interfaces and formats

This maximizes design freedom. It also consumes years on solved tooling problems
and makes self-hosting harder without automatically improving the system.

## Consequences

### Benefits

- existing compiler and object tooling can be adapted;
- source ports expose concrete compatibility requirements;
- Warren retains freedom in syscalls, handles, services, and applications; and
- compatibility policy can remain outside the privileged core where practical.

### Costs And Risks

- ports may need Warren-specific adaptation;
- “POSIX-informed” can become vague without an interface ledger;
- familiar names must not claim semantics Warren does not implement; and
- a later compatibility commitment may expose early semantic mistakes.

### Follow-Up Work

- maintain a documented userspace interface and tool-port ledger;
- specify exact libc behavior rather than relying on resemblance;
- establish ABI tests for ELF loading and C calling conventions; and
- record the libc and syscall decisions separately when requirements are known.

## Revisit When

- a strategically important tool or application has prohibitive porting cost;
- a standards-conformance target becomes valuable to actual Warren users;
- a compatibility layer can be isolated cleanly; or
- the selected source interfaces repeatedly conflict with Warren's intended
  resource model.
