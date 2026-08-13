# ADR-0009: Position-Independent Burrow Image

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 1–2
- **Supersedes:** None
- **Superseded by:** None

## Context

A fixed-address ELF executable would make the first loader smaller but require a
specific physical range to remain available after firmware initialization. It
would also give physical placement accidental significance in Burrow's earliest
interfaces. A fully dynamic executable model would be unnecessary and expand the
loader into a dynamic linker.

## Decision

Burrow is linked as a static, position-independent AArch64 ELF64 `ET_DYN` image.

The bootloader:

1. validates the ELF header, architecture, endianness, program-header bounds,
   segment alignment, sizes, permissions, and overlap;
2. allocates suitable physical pages through UEFI;
3. loads and zero-fills `PT_LOAD` segments;
4. computes one image load bias;
5. applies only the explicitly supported AArch64 relative runtime relocations;
6. rejects every unknown relocation, dynamic-linking request, or interpreter;
7. records the physical image extents and load bias in boot information; and
8. transfers control to the relocated entry point.

The initial supported dynamic relocation set is limited to
`R_AARCH64_RELATIVE`. Link-time relocations resolved within the final image are
not part of this runtime set. Burrow has no dynamic interpreter or shared-library
dependency.

Burrow does not begin as a higher-half kernel. It enters through the firmware's
identity mapping, then replaces that environment with Burrow-owned virtual memory
during the memory-management phase. Kernel core does not treat the initial
physical placement as a stable address or ABI.

## Alternatives Considered

### Fixed-address ELF64 `ET_EXEC`

This minimizes loader and debugger complexity. It can collide with firmware
allocations and requires a later load-model change. The resulting address would
be easy to embed throughout early code accidentally.

### Full dynamic ELF loading

This could support symbol resolution and shared libraries. It has no First Light
requirement and would make the bootloader own userspace-linker policy.

### Higher-half Burrow from the first instruction

This gives the desired long-term kernel address separation immediately. It
requires the loader to construct kernel page tables or Burrow to execute a more
complex relocation/trampoline path before basic diagnostics work.

## Consequences

### Benefits

- firmware may choose physical placement safely;
- the boot contract exposes actual placement rather than a magic constant;
- the loader remains much smaller than a dynamic linker;
- future physical targets and KASLR-like work have a viable foundation; and
- ELF validation and relocation logic can be host-tested extensively.

### Costs And Risks

- initial loader implementation is larger than fixed `ET_EXEC` loading;
- debugger symbol loading needs the runtime load bias;
- compiler/link flags must prevent unexpected runtime relocations; and
- malformed ELF and arithmetic overflow become security-sensitive loader inputs.

### Follow-Up Work

- define the linker script and PIE flags;
- create valid and malformed ELF test fixtures;
- audit the final dynamic relocation table as a build gate;
- generate debugger symbol-load commands from boot diagnostics; and
- select the later higher-half virtual layout in a separate ADR.

## Revisit When

- the compiler cannot emit the constrained relocation model reliably;
- measured loader complexity outweighs physical-placement flexibility; or
- a future secure-boot or verified-image format requires a different kernel
  container while retaining equivalent placement semantics.
