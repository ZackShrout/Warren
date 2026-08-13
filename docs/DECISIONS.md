# Warren Architectural Decisions

Architectural decision records (ADRs) preserve why a foundational choice was
made, what it costs, and what evidence could justify changing it. They prevent
implementation accidents and remembered conversations from becoming invisible
policy.

## When An ADR Is Required

Write or amend an ADR for a decision that:

- changes a guard rail;
- establishes an ABI, persistent format, or boot contract;
- chooses a supported architecture, machine, firmware, or toolchain baseline;
- establishes a dependency direction or subsystem ownership boundary;
- introduces a foundational third-party runtime dependency;
- selects a security or compatibility model; or
- reverses a previously accepted decision.

Ordinary implementation details and easily reversible local choices do not need
an ADR.

## Statuses

- **Proposed:** under active evaluation; implementation must not rely on it as
  settled policy.
- **Accepted:** current direction.
- **Superseded:** replaced by a named later ADR; retained as history.
- **Rejected:** considered and deliberately not selected.
- **Withdrawn:** no longer relevant before a decision was made.

## Process

1. Copy `docs/adr/0000-template.md` to the next four-digit number.
2. State the problem before the preferred solution.
3. Include real alternatives, consequences, and reversal costs.
4. Identify which roadmap gate requires the decision.
5. Mark it Proposed while evaluating or prototyping.
6. Accept it before dependent production implementation merges.
7. Supersede rather than erase it when the decision changes.

ADRs are prospective when possible. If code exposes an undocumented foundational
decision, record it immediately and label the discovered constraint honestly.

## Accepted Records Are Deliberate History

An accepted ADR is not edited to make later code or documentation convenient.
Code, plans, and general documentation conform to the accepted decision. If new
evidence requires a different architectural direction, discuss that change
explicitly and record it through a new ADR that supersedes or narrows the old
one.

Even a grammatical, formatting, or apparently editorial change to an accepted
ADR requires explicit discussion before editing. Clarification normally belongs
in a subordinate specification or new ADR so the original decision record
remains trustworthy. Silent retroactive cleanup is forbidden.

## Decision Index

| ADR | Title | Status |
| --- | --- | --- |
| [0000](adr/0000-template.md) | ADR template | Template |
| [0001](adr/0001-reference-target.md) | Initial reference target | Accepted |
| [0002](adr/0002-language-and-low-level-abi.md) | Implementation language and low-level ABI | Accepted |
| [0003](adr/0003-initial-kernel-structure.md) | Initial kernel structure | Accepted |
| [0004](adr/0004-compatibility-direction.md) | Compatibility direction | Accepted |
| [0005](adr/0005-warren-owned-bootloader.md) | Warren-owned bootloader | Accepted |
| [0006](adr/0006-cmake-and-ninja-build.md) | CMake and Ninja build system | Accepted |
| [0007](adr/0007-reference-aarch64-machine.md) | Reference AArch64 machine | Accepted |
| [0008](adr/0008-uefi-loader-toolchain-and-headers.md) | UEFI loader toolchain and headers | Accepted |
| [0009](adr/0009-position-independent-burrow-image.md) | Position-independent Burrow image | Accepted |
| [0010](adr/0010-boot-information-protocol-v1.md) | Boot-information protocol v1 principles | Accepted |
| [0011](adr/0011-aarch64-kernel-handoff.md) | AArch64 kernel handoff | Accepted |
| [0012](adr/0012-qemu-test-result-transport.md) | QEMU test-result transport | Accepted |
| [0013](adr/0013-project-license.md) | Project license | Accepted |
| [0014](adr/0014-toolchain-compatibility-baseline.md) | Toolchain compatibility baseline | Accepted |
| [0015](adr/0015-boot-information-v1-layout.md) | Boot-information protocol v1 layout | Accepted |
| [0016](adr/0016-initial-aarch64-virtual-memory-layout.md) | Initial AArch64 virtual-memory layout | Accepted |

## Phase 0 Decision Queue

No pre-implementation Phase 0 architectural decision remains unresolved. The
test-result grammar and numeric classifications are fixed by
`specifications/TEST_RESULT_PROTOCOL_V1.md` beneath ADR-0012. The register-level
EL2/EL1 normalization sequence is a focused First Light implementation decision
governed by ADR-0011.

The EDK2 snapshot and QEMU firmware provenance are now checked-in integrity
manifests governed by ADR-0008. A dedicated ADR may still be useful if their
update policy grows beyond that existing decision.
