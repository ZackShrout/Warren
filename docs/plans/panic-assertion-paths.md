# Panic And Assertion Paths Branch Plan

**Branch:** `codex/panic-assertion-paths`

**Status:** Complete

## Observable Result

After the diagnostic monitor exits, dedicated QEMU fixtures invoke the same
production panic path available to Burrow code. Assertion failure emits a
bounded versioned record with a stable identifier and source location before
terminating as `PANIC`/2. Kernel panic emits the corresponding record before
terminating as `PANIC`/3.

## Included Work

- an architecture-neutral, allocation-free panic record formatter;
- distinct assertion and kernel-panic classifications;
- bounded identifier, file, and message fields with exact validation;
- interrupt masking before panic publication;
- a one-way recursion latch and minimal recursive-panic record;
- a QEMU-virt platform panic entry that never returns;
- ordinary masked terminal wait and isolated test-only semihosting termination;
- host coverage for formatting, bounds, validation, output failure, and latch
  behavior;
- live assertion and kernel-panic fixtures after the monitor exchange; and
- image verification that production panic code remains while fixture and
  semihosting machinery stay absent from ordinary images.

## Non-Goals

- structured logging, log levels, or general diagnostic routing;
- stack unwinding, symbolization inside the guest, or crash persistence;
- SMP stop-the-world panic coordination;
- recovery, callbacks, destructors, exceptions, or a shutdown protocol; and
- a global assertion macro policy or removal of release input validation.

## Verification Matrix

- Core host tests require exact assertion and panic records, maximum-size
  fields, every invalid input class, output failure, and first/recursive latch
  results.
- Debug and Release ordinary images contain the production formatter and
  terminal wait but no injected record, QEMU marker, or semihosting trap.
- Live assertion and kernel-panic images complete the monitor exchange first,
  require their exact `BURROW_PANIC_V1` records, forbid normalized success, and
  require matching serial/process `PANIC`/2 and `PANIC`/3 results.
- Complete host and Debug/Release system matrices remain green.

## Merge Gates

- Panic masks interrupts and cannot return.
- Recursive entry is classified without re-entering the full formatter.
- Every emitted text field is explicitly bounded.
- The branch contains no logging, unwinding, scheduler, or unrelated work.
- Architecture, development, roadmap, image, console, normalized-entry, and
  test-result documentation describe the implemented boundary honestly.
- The completed branch is merged to `main` and pushed to the configured remote.
