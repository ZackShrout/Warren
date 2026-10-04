# PL011 Console Branch Plan

**Branch:** `feature/pl011-console`

**Status:** Complete

## Observable Result

After Burrow has reached architecture-neutral C++ under its owned EL1 address
space, it consumes the already validated early-console record through QEMU-virt
platform code and emits a fixed `BURROW_CONSOLE` diagnostic through a reusable,
allocation-free console interface. Both inherited EL1 and EL2 system profiles
must observe the diagnostic before the existing normalized-entry success
record.

## Included Work

- a small device-class byte-writer interface with bounded text writes;
- a PL011 output driver using 32-bit MMIO accesses and bounded `FR.TXFF`
  polling;
- QEMU-virt platform selection that accepts only the reference PL011 physical
  address, output capability, register stride, and register width described by
  the validated boot-information object;
- architecture-neutral kernel-entry publication through the selected writer;
- host tests for generic write behavior, PL011 success and timeout behavior,
  platform rejection, and witness publication ordering;
- EL1 and EL2 QEMU evidence that kernel C++ emitted the reusable-console
  diagnostic; and
- documentation of the new boundary and its verification.

## Non-Goals

- console input;
- UART clock, baud, FIFO, or line-control reconfiguration;
- interrupt-driven transmission, buffering, allocation, locking, or SMP
  serialization;
- structured logging, panic, assertion, or exception-dispatch policy;
- support for a second UART, platform, or virtual mapping policy; and
- removal of the assembly-only emergency and test-result writers.

The assembly writers remain deliberately independent. They are required before
C++ console construction and on paths where the reusable console itself cannot
be trusted.

## Ownership And Boundaries

- `Drivers/Console` owns bounded byte and text output independent of hardware.
- `Drivers/Pl011` owns PL011 register semantics and bounded polling.
- `Platform/QemuVirt` owns the reference physical address, the stable MMIO
  alias, and conversion of a validated boot record into a concrete writer.
- `Core/KernelEntry` owns complete object/context validation and publishes the
  first architecture-neutral diagnostic only after a writer is available.

No generic driver or Core interface embeds the QEMU PL011 address.

## Failure Behavior

- Invalid writer state, invalid text input, and a failed byte write return an
  explicit error without allocation.
- A continuously full PL011 transmit FIFO fails after a fixed polling budget;
  it does not spin forever in reusable C++ code.
- Platform selection rejects a missing or mismatched early-console record.
- Kernel-entry failure preserves the retained witness. The existing assembly
  boundary classifies any failure before the witness as kernel-entry failure.

## Verification Matrix

- The host debug profile compiles the shared interfaces without hosted
  dependencies.
- Host unit tests cover exact output, empty output, invalid writer/text,
  mid-write failure, PL011 writes, transmit timeout, and platform descriptor
  rejection.
- Kernel-entry tests prove that the witness is written only after the complete
  diagnostic succeeds.
- Focused Burrow debug and release images pass the independent ELF audit and
  remain free of QEMU test transport.
- Combined debug and release system tests retain every existing gate.
- Both live entry routes require:

  ```text
  BURROW_CONSOLE:driver=pl011:mode=polling:output=ready
  ```

## Merge Gates

- Every verification item above passes.
- The complete branch diff contains no unrelated cleanup.
- Architecture, development, roadmap, image, normalized-entry, and test-result
  documentation describe the implemented boundary honestly.
- `main` remains bootable through the existing normalized-entry result.
