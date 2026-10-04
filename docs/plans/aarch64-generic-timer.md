# AArch64 Generic Timer Branch Plan

**Branch:** `codex/aarch64-generic-timer`

**Status:** Complete

## Observable Result

After architecture-neutral C++ entry succeeds, Burrow configures CPU 0's
QEMU-virt GICv3 interface and the AArch64 non-secure physical timer, unmasks IRQ
briefly, survives exactly one PPI 30 interrupt through the owned EL1 vector
table, masks IRQ again, and emits a bounded `BURROW_TIMER` diagnostic before
the existing normalized-entry success record.

## Included Work

- fixed owned mappings for the QEMU-virt GICv3 distributor and CPU 0
  redistributor frames;
- audited Device-nGnRnE page-table policy and activation preflight for those
  mappings;
- a bounded CPU 0 GICv3 bring-up for Group 1 PPI 30;
- physical-counter frequency validation and a deterministic 100 Hz one-shot
  timer interval;
- current-EL SPx IRQ capture, C++ dispatch, complete register restoration, and
  `ERET` only for the handled timer interrupt;
- terminal complete-frame reporting for every other stable vector or unhandled
  interrupt;
- host interval/reporting tests, disassembly verification, and live EL1/EL2
  timer evidence; and
- one test-only timer-initialization failure fixture using normalized-entry
  failure code 82.

## Non-Goals

- a general interrupt registry or device-independent GIC driver;
- multiple CPUs, redistributor discovery, affinity routing, SGIs, SPIs, LPIs,
  ITS, MSI, or interrupt balancing;
- recurring ticks, preemption, scheduling, deferred work, or timekeeping;
- virtual timers, EL0 timer access, or timer virtualization; and
- changing the inherited emergency-vector behavior.

## Failure Behavior

GIC register polling is bounded. Invalid counter frequency, an unrepresentable
interval, inaccessible CPU-interface state, GIC wake/configuration failure, or
diagnostic output failure returns failure code 82 in system-test images and
enters the existing terminal wait in ordinary images. A non-PPI-30 IRQ is
acknowledged, deactivated according to the CPU-interface mode, and then follows
the complete terminal exception-report path; it is never treated as handled.

## Verification Matrix

- Host tests cover exact 100 Hz interval calculation, invalid bounds, exact
  timer diagnostic output, and bounded writer failure.
- Page-table tests prove both GIC ranges use Device memory and are required by
  activation preflight.
- Image verification proves the stable IRQ slot uses the shared full-frame
  entry and that the handled branch restores x0 through x30, releases the
  320-byte frame, and executes `ERET`.
- EL1 and EL2 QEMU routes require one exact PPI 30 tick diagnostic before
  normalized success.
- A test-only initialization fixture requires `FAIL`/82 and forbids both timer
  and normalized-success diagnostics.
- Complete Debug and Release host/system matrices remain green.

## Merge Gates

- Every verification item above passes.
- No IRQ remains unmasked after the one-shot proof.
- The branch contains no scheduler, periodic timekeeping, or unrelated cleanup.
- Architecture, development, roadmap, image, normalized-entry, and test-result
  documentation describe the implemented boundary honestly.
- The completed branch is merged to `main` and pushed to the configured remote.
