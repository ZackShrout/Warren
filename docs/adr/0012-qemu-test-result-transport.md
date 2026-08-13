# ADR-0012: QEMU Test-Result Transport

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 1 onward
- **Supersedes:** None
- **Superseded by:** None

## Context

Serial output is essential for diagnosis but does not by itself give automated
tests an unambiguous process result. A hung guest, panic, assertion, ordinary
shutdown, or successful test must be distinguishable without fragile parsing of
the last visible line. The solution must not become a Burrow or physical-hardware
dependency.

## Decision

Reference TCG system tests use two coordinated channels:

1. **PL011 serial records** provide human-readable and machine-parseable progress,
   panic, failure, and pass messages.
2. **Arm semihosting `SYS_EXIT_EXTENDED`** terminates the trusted QEMU process with
   an explicit test result after the final serial record is emitted.

Serial test records use a versioned, line-oriented prefix such as:

```text
WARREN_TEST:1:BEGIN:first-light
WARREN_TEST:1:PASS:first-light
```

The exact field escaping and result-code table are specified before the first
test harness is implemented.

Semihosting code lives exclusively in `Platform/QemuVirt` test support and is
compiled only into explicitly test-enabled images. It is never exposed as a
syscall, SDK facility, application service, ordinary shutdown mechanism, or
physical-platform requirement. Production/interactive images do not enable it.

The host harness always enforces a timeout and reports one of at least:

- pass;
- explicit test failure;
- panic/assertion;
- QEMU process failure; or
- timeout/hang.

TCG is the authoritative test accelerator because QEMU semihosting is not
available under HVF. Fast HVF runs use serial diagnostics and a later ordinary
platform shutdown path but do not replace the TCG result gate.

Semihosting is enabled only for trusted Warren-built test images because it
bypasses guest/host isolation and includes host-access operations Warren does not
need or authorize.

## Alternatives Considered

### Serial parsing only

This is portable and already required. A guest can print a pass marker and then
hang, panic during output, or lose buffered bytes, leaving host exit semantics
ambiguous.

### QEMU `isa-debug-exit`

This is common for x86 hobby-OS tests. It is not a natural device on the AArch64
`virt` platform and would create a nonstandard device dependency.

### PSCI shutdown alone

PSCI is closer to real platform behavior and will be valuable for ordinary
shutdown. It does not naturally encode a rich test result and can make success
indistinguishable from other powered-off states.

### QMP orchestration

QMP is valuable for external control and inspection. Making the guest signal
completion through a separately invented device or serial/QMP coordination does
not reduce early complexity.

## Consequences

### Benefits

- automated tests receive an actual terminal result;
- serial retains full diagnostics;
- timeout remains a distinct outcome;
- no fake hardware device or permanent ABI is introduced; and
- the target-specific mechanism is tiny and removable.

### Costs And Risks

- reference tests must use TCG;
- semihosting is unsafe for untrusted guest images;
- two result channels must agree; and
- a later x86-64 checkpoint needs its own target-specific exit transport behind
  the same host test-result abstraction.

### Follow-Up Work

- define result codes and serial record grammar;
- implement the minimal AArch64 semihosting operation;
- prove pass, fail, panic, process-error, and timeout classifications;
- prevent semihosting from entering release artifacts; and
- add a non-semihosted interactive shutdown path later.

## Revisit When

- QEMU removes or changes the required semihosting operation;
- TCG test time becomes prohibitive;
- a standard virtual device provides safer equivalent result semantics; or
- the multi-architecture host harness benefits from a different uniform transport.
