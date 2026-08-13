# ADR-0003: Initial Kernel Structure

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 1–4
- **Supersedes:** None
- **Superseded by:** None

## Context

Warren ultimately needs processes, drivers, filesystems, IPC, services, and a
graphical environment. A microkernel could isolate many of those components, but
it requires a mature IPC, scheduling, bootstrap, service, and debugging story
before basic drivers are pleasant to develop. A single undifferentiated kernel
would bring up quickly but allow machine and subsystem coupling to spread.

## Decision

Burrow begins as a **modular monolithic kernel**.

Initial core services and drivers execute in one privileged address space. Their
source, ownership, and dependency boundaries are explicit and must not depend on
their physical colocation. User-facing policy—including service supervision,
sessions, network configuration, package management, and the desktop—runs in
user space once the required mechanisms exist.

Burrow does not promise a stable in-kernel module ABI, dynamic kernel extension,
or a future microkernel conversion. Drivers are statically linked initially.

## Alternatives Considered

### Microkernel from the beginning

This offers strong isolation and forces explicit service interfaces. It makes
early device bring-up depend on IPC, process creation, naming, scheduling, and
recovery mechanisms that do not yet exist.

### Unstructured monolithic kernel

This minimizes ceremony during First Light. It encourages architecture details,
device policy, and global state to become inseparable before portability can be
tested.

### Hybrid kernel as a declared goal

“Hybrid” does not answer which components share a protection domain or how their
interfaces are enforced. Warren will name actual boundaries rather than adopt an
ambiguous label.

## Consequences

### Benefits

- short path to observable hardware and memory bring-up;
- ordinary calls and shared debugging for early drivers;
- architectural modularity without premature IPC infrastructure; and
- user-space policy remains an explicit long-term direction.

### Costs And Risks

- a faulty driver can corrupt the entire kernel;
- module boundaries rely on review and build enforcement rather than protection;
- moving a component to user space later may require interface redesign; and
- global conveniences can erode the planned structure if guard rails are weak.

### Follow-Up Work

- enforce include and dependency direction in the build;
- require driver-class interfaces rather than platform globals;
- define interrupt-context and blocking rules before concurrent subsystems grow;
  and
- evaluate user-space drivers only when IPC and recovery semantics are concrete.

## Revisit When

- a driver class presents unacceptable kernel reliability or security risk;
- measured IPC and scheduling capabilities make user-space drivers practical;
- independently updateable kernel extensions become a supported product need; or
- the initial structure prevents a required security boundary.
