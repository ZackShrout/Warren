# Diagnostic Monitor Branch Plan

**Branch:** `codex/diagnostic-monitor`

**Status:** Complete

## Observable Result

After the one-shot timer proof, Burrow publishes a bounded monitor-ready record,
accepts `help`, `status`, and `exit` commands from the QEMU-virt PL011 receive
path, reports the handled status command with the observed timer tick, and exits
the monitor before the existing normalized-entry success result.

## Included Work

- an allocation-free byte-reader abstraction paired with the existing console
  writer;
- bounded PL011 receive polling through the audited upper MMIO alias;
- a fixed-size, architecture-neutral command parser and monitor loop;
- exact `help`, `status`, and `exit` responses plus deterministic unknown-command
  behavior;
- an eight-command session bound and a 15-byte command bound;
- a QEMU harness path that waits for the ready record before injecting real
  serial input;
- host coverage for input, parsing, bounds, output failure, and PL011 polling;
- live EL1 and EL2 evidence that `status` and `exit` are accepted in order; and
- one test-only monitor failure fixture using normalized-entry failure code 83.

## Non-Goals

- a shell, command-line editing, history, quoting, variables, or scripting;
- IRQ-driven UART input, buffering, concurrency, or background work;
- exposing memory, register, or mutation commands;
- indefinite interactive sessions or an unbounded read loop; and
- changing the test-result protocol beyond the adjacent failure allocation.

## Failure Behavior

Invalid reader/writer state, receive timeout, command overflow, exhausted command
budget, invalid timer state, or output failure returns a typed monitor error.
The architecture boundary maps any monitor error to failure code 83 in system
tests and the existing masked terminal wait in ordinary images. Unknown and
empty commands produce a bounded error record and consume one command slot; they
do not escape the session bounds.

## Verification Matrix

- Console and PL011 host tests cover valid reads, invalid callbacks, empty FIFO
  timeout, and exact received bytes.
- Monitor host tests cover all commands, CR/LF handling, unknown input, command
  overflow, command-budget exhaustion, receive timeout, and output failure.
- The image verifier proves the activation path calls the monitor after the
  timer proof and before normalized success.
- EL1 and EL2 QEMU routes wait for the ready marker, inject `status` and `exit`
  through PL011, and require both exact responses in order.
- A test-only monitor fixture requires `FAIL`/83 and forbids status, exit, and
  normalized-success diagnostics.
- Complete Debug and Release host/system matrices remain green.

## Merge Gates

- Every verification item above passes.
- All monitor reads and command sessions have explicit finite bounds.
- The branch contains no shell, scheduler, heap, or unrelated cleanup work.
- Architecture, development, roadmap, image, console, normalized-entry, and
  test-result documentation describe the implemented boundary honestly.
- The completed branch is merged to `main` and pushed to the configured remote.
