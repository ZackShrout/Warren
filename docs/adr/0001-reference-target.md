# ADR-0001: Initial Reference Target

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0 and Phase 1
- **Supersedes:** None
- **Superseded by:** None

## Context

Warren is developed initially on an Apple Silicon Mac and has long-term AArch64
and x86-64 goals. Direct Apple hardware support would immediately require a large
and machine-specific device, firmware, and debugging effort. Developing two CPU
ports simultaneously would multiply bring-up work before kernel abstractions
have concrete requirements.

The first target needs excellent emulation, deterministic machine construction,
a simple serial path, standard CPU facilities, and a later path to a second
architecture.

## Decision

Warren's initial and sole primary target is **QEMU's generic AArch64 `virt`
machine**, running one virtual CPU.

The boot environment will be UEFI-capable and will hand Burrow a Warren-defined,
versioned boot-information structure through Warren's bootloader. QEMU-only
addresses and devices remain inside the `Platform/QemuVirt` layer. Burrow uses
ELF64 and the AArch64 procedure-call standard.

The exact QEMU machine version, firmware artifact, CPU feature baseline, and boot
protocol are separate Phase 0 decisions because they require verification and
pinning.

x86-64 remains a required design direction but not an active co-equal port. A
narrow x86-64 First Light checkpoint follows the first AArch64 user-boundary
milestone to audit the shared interfaces before the system grows substantially.

Direct Apple Silicon hardware is not an implied target.

## Alternatives Considered

### Direct Apple Silicon first

This would run on the development machine without emulation and offer an
interesting modern platform. Its firmware and device ecosystem create far more
bring-up work, less controlled hardware variation, and a weak first debugging
environment.

### QEMU x86-64 first

x86-64 has extensive hobby-OS documentation and mature emulation. It does not
match the project's preferred first architecture, and choosing AArch64 under
QEMU is not materially constrained by the ARM host.

### A specific physical ARM development board

A documented board could become a good later target. Physical hardware slows
iteration and adds board-specific boot, UART, interrupt, storage, and recovery
variables before Burrow can diagnose them.

### AArch64 and x86-64 together

This would expose portability failures immediately but would duplicate unstable
bring-up work and encourage prematurely generic interfaces. A scheduled boundary
audit captures much of the value at lower initial cost.

## Consequences

### Benefits

- deterministic and scriptable bring-up on the existing Mac;
- reliable early serial and debugger access;
- a named platform instead of vague “ARM64 support”;
- clean separation between host CPU and target machine; and
- deliberate pressure toward replaceable architecture and platform layers.

### Costs And Risks

- emulator behavior can hide real-hardware ordering and device problems;
- generic `virt` support is not evidence of broad AArch64 or UEFI support;
- the later x86-64 port will still require meaningful rework; and
- a custom bootloader adds an artifact that must eventually be built and tested.

### Follow-Up Work

- pin the QEMU machine/CPU and UEFI firmware artifacts;
- specify boot protocol version 1;
- select the earliest host-visible test result mechanism; and
- record the later x86-64 reference machine when its checkpoint begins.

## Revisit When

- the selected QEMU platform cannot model a required architecture feature;
- firmware reproducibility becomes untenable;
- a well-documented physical target offers a high-value validation path; or
- the x86-64 checkpoint reveals that the timing of the second port is too late.
