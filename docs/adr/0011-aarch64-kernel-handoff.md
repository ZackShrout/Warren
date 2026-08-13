# ADR-0011: AArch64 Kernel Handoff

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 1
- **Supersedes:** None
- **Superseded by:** None

## Context

AArch64 UEFI can execute the Warren bootloader at non-secure EL1 or EL2. Before
`ExitBootServices()`, firmware owns an identity-mapped MMU/cache environment and
requires its semantics to remain intact. The Warren bootloader needs a bounded
handoff, while CPU-state normalization properly belongs to Burrow's architecture
layer rather than kernel core or a firmware-specific loader.

## Decision

Immediately before transfer, the bootloader provides:

| State | Contract |
| --- | --- |
| `x0` | Physical address of boot-information protocol v1 object |
| `x1`–`x3` | Zero |
| Stack | Valid bootloader-provided bootstrap stack, 16-byte aligned |
| Execution state | AArch64, little-endian, non-secure EL1 or EL2 |
| Interrupt masks | DAIF masked by the loader immediately before handoff |
| MMU/caches | Firmware identity-mapped environment remains active |
| `x18` | Not used by loader code; no value promised to Burrow |
| Floating/vector state | No contract beyond architectural/UEFI requirements |

The bootloader logs the observed exception level and successful completion of
`ExitBootServices()` before the final transfer. It makes no firmware call after
the successful exit.

Burrow begins at a narrow architecture-specific assembly entry that:

1. preserves `x0` and records `CurrentEL` without requiring allocation;
2. installs an emergency exception-vector table at the current EL immediately;
3. validates enough boot-information header state to continue safely;
4. establishes an owned transition stack if required;
5. normalizes supported EL2 state and descends to EL1;
6. establishes Burrow-owned early page tables and exception vectors;
7. performs required TLB/cache/barrier sequences; and
8. calls architecture-neutral kernel entry only at normalized EL1.

The exact system-register program and transition order are proven by a focused
prototype and then appended to this ADR or a subordinate architecture
specification. EL1 and EL2 entry are both reference-test cases before the
contract is considered stable.

Burrow does not rely on UEFI runtime services initially. Runtime regions and
attributes remain reserved because ignoring a service is not permission to reuse
its memory.

## Alternatives Considered

### Bootloader fully normalizes to MMU-off EL1

This gives Burrow an extremely clean machine-state contract. It makes the UEFI
loader own delicate exception-level, cache, TLB, timer, and page-table behavior
that must later be reimplemented for another loader.

### Require firmware to invoke only at EL1

This simplifies entry but rejects compliant environments that use the highest
available non-secure privilege level and may enter the loader at EL2.

### Burrow continues indefinitely in firmware mappings

This delays page-table work but makes firmware layout and attributes an implicit
kernel architecture. Firmware mappings are a transition mechanism only.

## Consequences

### Benefits

- UEFI-specific responsibilities stop at the handoff;
- CPU normalization is reusable by another AArch64 loader path;
- both compliant UEFI exception levels can be supported;
- emergency exception visibility begins before complex state changes; and
- generic kernel core sees one normalized EL1 environment.

### Costs And Risks

- architecture entry must reason correctly about two initial privilege levels;
- replacing live page tables and caches is a high-risk operation;
- the loader stack remains an owned resource until Burrow switches away; and
- debugger address/state setup changes across the transition.

### Follow-Up Work

- write and review the register-level transition specification;
- add deliberate exception tests before, during, and after normalization;
- define boot-stack reservation and reclamation;
- produce EL1 and EL2 QEMU test configurations; and
- document debugger attachment at each transition stage.

## Revisit When

- installed/reference firmware proves to have a narrower stable entry state;
- a physical platform cannot meet the contract;
- the dual-EL path adds unbounded complexity without actual portability value; or
- Warren adopts UEFI runtime services and needs a compatible mapping contract.
