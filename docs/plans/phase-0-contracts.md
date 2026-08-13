# Phase 0 Contracts Branch Plan

- **Status:** Planned
- **Branch:** `foundation/phase-0-contracts`
- **Base:** `main`
- **Roadmap phase:** Phase 0 — Foundation
- **Primary outcome:** Convert every remaining pre-implementation Phase 0
  decision into an exact, testable contract.

## Why This Branch Is Next

Warren's UEFI pipeline is proven, but Burrow implementation currently depends on
three decisions that remain deliberately incomplete:

1. the exact boot-information protocol v1 representation;
2. the initial and transitional virtual-memory layout; and
3. the cross-toolchain baseline and update policy.

ADR-0012 also requires an exact serial test-record grammar and result-code table
before the Phase 1 harness grows beyond the current UEFI pipeline marker.

Starting a kernel image or ELF loader before resolving these contracts would
allow temporary structure layouts, addresses, and tool versions to become policy
through code. This branch closes that gap without mixing design work into the
first Burrow implementation branch.

## Observable Completion

At branch completion:

- the Phase 0 decision queue contains no unresolved pre-implementation item;
- boot-information v1 has one canonical byte-level definition with executable
  size, alignment, and offset checks;
- valid and malformed protocol objects are exercised by host tests;
- the AArch64 target compiles the canonical protocol declaration without host
  headers or ABI assumptions;
- the initial identity-mapped period and the later Burrow-owned virtual layout
  have explicit ownership, address ranges, transition gates, and reserved space;
- bootstrap can distinguish the recorded known-good toolchain family from an
  unsupported or stale configuration according to a documented update policy;
- the Phase 1 test-record grammar and result classifications are exact; and
- the existing UEFI debug and release images still build reproducibly and boot
  successfully in QEMU.

No Burrow executable is produced by this branch.

## Deliverables

### 1. Toolchain Baseline And Update Contract

Create an accepted ADR that defines:

- which tools form the compatibility baseline;
- which versions are exact integrity-pinned inputs and which are recorded
  known-good host tools;
- what bootstrap validates automatically;
- when a version mismatch is fatal versus diagnostic;
- how a dependency update is proposed, tested, and recorded; and
- how an already populated dependency cache supports offline verification.

Add a machine-readable checked-in baseline consumed by bootstrap. The policy
must not pretend Homebrew provides immutable historical environments when it
does not, and it must not allow arbitrary future major versions to pass without
an explicit Warren verification update.

The new ADR begins as Proposed. It becomes Accepted only after explicit review
and discussion; implementation work does not grant itself authority to accept
the decision.

### 2. Boot-Information Protocol V1 Specification

Write the exact byte-level specification required by ADR-0010, including:

- magic and version values;
- byte order and object alignment;
- header field order, widths, offsets, total size, and reserved zeros;
- capability and required-field flags;
- kernel image, entry, load-bias, and bootstrap-stack descriptions;
- table offset/count/stride rules;
- normalized memory-map entry layout and memory-kind values;
- optional command-line, initial-image, early-console, framebuffer, ACPI, and
  device-tree representations;
- string encoding and termination rules;
- arithmetic, containment, overlap, and compatibility validation order; and
- ownership and reservation rules through early physical-memory initialization.

Place the canonical ABI declaration in an architecture-neutral Warren SDK
header. It must be usable from freestanding C++ and at C/assembly boundaries
without exposing pointers, compiler enums, `bool`, `size_t`, or implicit padding.

Add compile-time checks for every ABI size, alignment, and offset. Add host tests
covering at least:

- the smallest valid object;
- a representative object containing a memory map and optional descriptors;
- unsupported major and compatible minor versions;
- truncated headers and tables;
- integer overflow in offset/count/stride arithmetic;
- overlapping or out-of-bounds contained regions;
- unknown required flags; and
- nonzero reserved fields.

Test fixtures are constructed through Warren-owned builders or explicit bytes;
they may not validate a structure solely by round-tripping it through the same
unchecked implementation.

### 3. Initial Virtual-Memory Layout

Create an accepted ADR defining:

- what remains identity-mapped during initial entry;
- the point at which firmware mappings stop being trusted;
- the initial 4 KiB translation-granule and address-width assumptions;
- kernel, direct-physical-map, MMIO, guard, and future user-space regions;
- which ranges are commitments versus provisional reservations;
- the transition relationship between physical load bias and stable virtual
  addresses;
- permissions expected for code, read-only data, writable data, stacks, and
  unmapped guards; and
- which decisions are deliberately deferred until the memory-management phase.

This branch specifies the layout; it does not build page tables or enable new
translations.

### 4. Test-Result Protocol V1

Complete ADR-0012's required subordinate specification:

- record prefix and ASCII grammar;
- test identifier and field-character restrictions;
- `BEGIN`, `PASS`, `FAIL`, and `PANIC` records;
- numeric result-code allocation;
- required ordering and terminal-record rules;
- disagreement handling between serial and semihosting results; and
- host classifications for explicit failure, panic, QEMU failure, and timeout.

Update the current UEFI smoke marker only if necessary to conform to the final
grammar, preserving its existing behavior and test coverage.

### 5. Documentation Reconciliation

Update `ARCHITECTURE.md`, `DECISIONS.md`, `DEVELOPMENT.md`, and `ROADMAP.md` so
they agree with the completed contracts. Existing accepted ADR text is not
edited. Clarifications are recorded in subordinate specifications or new ADRs;
an actual direction change follows explicit discussion and supersession.
Phase 0 may be declared complete only when its exit gates are demonstrably met;
finishing the documents alone is not sufficient if their executable checks are
missing.

## Implementation Order

1. Inventory every field, address, version, and tool assumption currently
   implied by the accepted ADRs and build scripts.
2. Settle and implement the toolchain baseline policy so subsequent evidence has
   a defined environment.
3. Write the boot-information byte specification and canonical declarations.
4. Implement the independent validator tests and target compile checks.
5. Settle the initial virtual-memory layout using the now-exact handoff data.
6. Specify the serial and semihosted result protocol and update its host tests.
7. Reconcile non-ADR Phase 0 documents and run the complete integration matrix.

Prototypes may reorder local work, but no dependent implementation is treated as
policy before its governing ADR is accepted.

## Explicit Non-Goals

- Building or linking a Burrow ELF image
- Parsing or relocating ELF in the UEFI bootloader
- Calling `ExitBootServices()` for a kernel handoff
- Implementing AArch64 entry, exception vectors, page tables, or EL2 descent
- Implementing PL011 or semihosting target code
- Allocating physical pages in Burrow
- Adding x86-64 definitions merely for symmetry
- Selecting a permanent userspace address-space ABI
- Introducing a general serialization framework or new target runtime dependency

## Verification Matrix

The branch must pass:

```sh
./tools/bootstrap.sh --check
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
cmake --preset uefi-aarch64-debug
cmake --build --preset uefi-aarch64-debug
ctest --preset uefi-aarch64-debug
cmake --preset uefi-aarch64-release
cmake --build --preset uefi-aarch64-release
ctest --preset uefi-aarch64-release
```

The branch may add narrower protocol-specific presets or targets, but these
commands remain the integration front doors. Clean out-of-tree configuration is
required for at least the host and AArch64 ABI checks before merge.

## Merge Gates

- All new architectural decisions have been explicitly discussed, accepted, and
  indexed; the branch does not self-accept its own ADRs.
- The Phase 0 decision queue is empty or replaced only by explicitly later-phase
  questions with a named revisit gate.
- ABI declarations have compile-time layout checks on host and AArch64 targets.
- Protocol validators include positive, boundary, and malformed-object cases.
- No canonical ABI header includes UEFI, macOS, or architecture-private headers.
- Existing UEFI first-light and ESP reproducibility tests pass in debug and
  release profiles.
- Documentation describes only implemented evidence and accepted contracts.
- No new dependency is introduced without provenance, license, integrity, and a
  removal or update story.

## Principal Risks

### Designing An Imaginary Future

The protocol and address layout can become overgeneralized. Every mandatory
field and reserved range must be justified by Phase 1 or a named later consumer.
Optional extensibility is preferable to speculative mandatory machinery.

### Mistaking Host Layout For Wire Layout

Native structs alone are insufficient evidence. Explicit offsets, independent
byte fixtures, target compilation, and overflow tests must agree.

### False Toolchain Reproducibility

Recording version strings is not the same as pinning artifacts. The ADR must
name exactly what Warren guarantees and must not overstate reproducibility.

### Freezing Virtual Addresses Too Early

Only ranges needed to prevent collisions and guide the first page-table design
should be committed. Later user-space, shared-library, and randomization policy
remain revisitable until they have real consumers.
