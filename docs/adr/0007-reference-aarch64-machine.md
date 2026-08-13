# ADR-0007: Reference AArch64 Machine

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 0–3
- **Supersedes:** The unresolved machine details in ADR-0001
- **Superseded by:** None

## Context

ADR-0001 selected QEMU's AArch64 `virt` family but deliberately left the
machine version, CPU baseline, memory size, interrupt controller, and reference
accelerator unresolved. Using drifting `virt`, `max`, or `host` aliases would
make guest-visible behavior depend on the installed QEMU release or development
Mac. Choosing a legacy interrupt controller merely for easier bring-up would
also create avoidable replacement work.

Warren's installed QEMU 11.0.3 provides the versioned `virt-11.0` machine,
Cortex-A57 CPU model, GICv3, TCG and HVF accelerators, and EDK2 AArch64 firmware.

## Decision

The reference First Light machine is:

| Property | Value |
| --- | --- |
| QEMU machine | `virt-11.0` |
| Reference accelerator | TCG |
| CPU | `cortex-a57` |
| CPU architecture baseline | ARMv8.0-A; no optional extension assumed |
| CPU count | One |
| RAM | 512 MiB |
| Interrupt controller | GICv3 |
| MSI/ITS | Disabled until a PCI/MSI requirement exists |
| Base page size | 4 KiB |
| Early console | First PL011 UART |
| Display | None |
| Monitor | None in ordinary run/test profiles |

The reference command always names each guest-visible choice instead of relying
on QEMU defaults. `virt-11.0` remains the compatibility target even after the
host QEMU installation advances.

TCG is authoritative for automated boot and system tests. An optional HVF/host-
CPU profile may accelerate trusted interactive development on Apple Silicon,
but a success observed only there does not satisfy a roadmap gate.

The kernel and Warren ABI use 64-bit address/count representations even where
the reference machine's initial RAM is small. The 4 KiB base page is a Warren
architectural choice shared with the future x86-64 direction; larger mapping
blocks remain an internal optimization.

## Alternatives Considered

### Unversioned `virt`

This automatically gains QEMU improvements but may change guest-visible behavior
when QEMU is upgraded. It remains useful for compatibility experiments, not as a
reference gate.

### `-cpu max`

This exposes the broadest TCG feature set and is useful for targeted feature
experiments. Its feature surface may change, and it would allow generated code
to acquire a CPU requirement Warren never selected.

### `-cpu host` with HVF

This is substantially faster on the development Mac. It makes the guest CPU and
available debugging/test facilities host-dependent and does not support the
chosen semihosting test-result path.

### Cortex-A53

Cortex-A53 is another credible ARMv8.0-A baseline. Cortex-A57 is selected as an
equally conservative, widely emulated application-profile CPU without affecting
Warren's prohibition on optional ISA assumptions.

### GICv2

GICv2 has a smaller initial programming model but is a legacy interrupt
controller with a lower CPU ceiling. GICv3 is the intended long-term AArch64
foundation; its MSI/ITS portion can remain disabled independently.

### 16 KiB or 64 KiB base pages

Larger pages reduce page-table overhead on large systems. Four KiB pages reduce
internal fragmentation, align with the future x86-64 base page, and match the
most convenient bring-up/debugging assumptions.

## Consequences

### Benefits

- deterministic guest-visible CPU and platform behavior;
- an intentionally modest ISA baseline;
- modern interrupt-controller work is done once;
- reference tests run on any QEMU host architecture; and
- a shared 4 KiB page abstraction helps the later x86-64 audit.

### Costs And Risks

- TCG is slower than HVF;
- GICv3 bring-up is more involved than GICv2;
- the versioned machine will eventually omit newer virtual devices/features; and
- the bundled firmware can still change unless its artifact is pinned separately.

### Follow-Up Work

- encode the reference and fast run profiles;
- copy the writable EDK2 variable-store template per run;
- record and verify firmware hashes/provenance; and
- compile Burrow for the explicit ARMv8.0-A baseline.

## Revisit When

- QEMU removes support for `virt-11.0`;
- a required device cannot be represented on the selected machine;
- measured TCG time makes another authoritative test accelerator necessary; or
- the first physical target justifies raising the minimum CPU architecture.
