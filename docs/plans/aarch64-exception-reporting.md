# AArch64 Exception Reporting Branch Plan

**Branch:** `feature/aarch64-exception-reporting`

**Status:** Complete

## Observable Result

After the owned EL1 vector table and guarded stack are active, every terminal
architectural exception produces one versioned, allocation-free report through
the reusable PL011 console. The report contains its transition stage, vector,
EL1 syndrome state, interrupted SP, and all 31 general-purpose registers before
the existing test-only terminal transport records `PANIC` code 4.

## Included Work

- a C-compatible, fixed 320-byte AArch64 exception-frame ABI;
- a distinct stable vector table that preserves x0 through x30 before calling
  C++ and retains the architected 16-by-128-byte table geometry;
- bounded C++ formatting through `Drivers/Console` with a versioned
  `BURROW_EXCEPTION_V1` record;
- QEMU-virt ownership of the stable PL011 construction used by the terminal
  reporter;
- a reporter-active guard and DAIF-masked, non-returning failure path;
- C and C++ ABI checks, host formatter tests, image-disassembly auditing, and
  EL1 live fault evidence after successful kernel C++ entry; and
- migration of existing stable-vector guard and permission fixtures to the
  complete report while preserving the stackless inherited emergency path.

## Non-Goals

- exception recovery or return with `ERET`;
- IRQ dispatch, the ARM generic timer, scheduling, or user-mode entry;
- symbolization, stack unwinding, backtraces, or source locations;
- SMP-safe panic coordination;
- replacing the pre-normalization EL1/EL2 emergency reporter; and
- a generic multi-platform exception console policy.

## Failure Behavior

The stable reporter masks DAIF, claims the existing reporter-active word, and
never clears it. A nested exception enters a separate silent `WFE` loop. An
invalid frame, invalid stable console construction, or bounded console timeout
also ends in the terminal wait without attempting recovery or recursive output.
The full-frame path requires the owned mapped stack; faults before that contract
continues to use the stackless emergency table.

## Verification Matrix

- C and C++ compile-time checks agree on every frame offset and total size.
- Host tests prove complete field formatting, all 31 GPR fields, invalid-frame
  rejection, and bounded mid-report failure.
- The independent image verifier proves each stable slot allocates the exact
  frame, preserves x0/x1, classifies its vector, and enters the stable reporter;
  it also proves the common entry captures LR and all four EL1 fault registers.
- Existing stable guard, W^X, and stale-identity QEMU fixtures require the new
  record.
- A post-kernel-C++ `BRK #0x77a` fixture requires stage 9, vector 4, the exact
  ESR, the x15 fixture sentinel, x30 presence, and terminal `PANIC` code 4.
- Debug and release host and system profiles remain green.

## Merge Gates

- Every verification item above passes.
- The branch diff contains no recovery, timer, or unrelated cleanup work.
- The normalized-entry, image, architecture, development, test-result, and
  roadmap documents distinguish emergency capture from complete stable
  reporting.
- The completed branch is merged to `main` and pushed to the configured remote.
