# ADR-0014: Toolchain Compatibility Baseline

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0 onward
- **Supersedes:** None
- **Superseded by:** None

## Context

Warren needs a development environment that is practical to establish on a new
ARM Mac without allowing arbitrary future host tools to become supported by
accident. Different dependencies also provide different reproducibility
mechanisms.

The retained EDK2 sources and QEMU firmware files are immutable inputs that can
be integrity-checked byte for byte. Homebrew formula names, `brew pin`, and a
`Brewfile` do not reproduce historical package versions on another machine.
Homebrew documents that `brew bundle --no-upgrade` does not add lock-file support
and recommends a maintained private tap when long-term control of formula
versions is required.

Warren could own such a tap or redistribute every host tool, but that would make
toolchain packaging a substantial project before Burrow exists. Simply accepting
whatever Homebrew currently distributes would make compiler, linker, emulator,
and image-tool compatibility unbounded.

## Decision

Warren distinguishes **integrity-pinned target inputs**, **supported host-tool
ranges**, and a **last-known-good host-tool tuple**.

### Integrity-Pinned Inputs

Inputs that directly supply target declarations or firmware bytes are pinned by
immutable revision and SHA-256:

- the EDK2 source archive and every retained ABI header; and
- the QEMU AArch64 UEFI code and variable-store template.

Bootstrap rejects a hash mismatch. Updating one of these inputs requires an
explicit dependency update with provenance, license review, build verification,
and debug/release QEMU tests.

The versioned QEMU `virt-11.0` machine and its explicit CPU, memory, interrupt,
and device configuration remain the guest-visible platform contract defined by
ADR-0007. A newer compatible QEMU host does not change that machine name.

### Supported Host-Tool Ranges

Bootstrap accepts these minimum-inclusive, maximum-exclusive ranges:

| Tool | Supported range |
| --- | --- |
| CMake | `>=3.28.0,<5.0.0` |
| Ninja | `>=1.13.0,<2.0.0` |
| LLVM/Clang | `>=22.1.0,<23.0.0` |
| LLD | `>=22.1.0,<23.0.0` |
| QEMU | `>=11.0.0,<12.0.0` |
| mtools | `>=4.0.0,<5.0.0` |
| Python | `>=3.14.0,<3.15.0` |

LLVM/Clang and LLD versions must match exactly. A version outside any supported
range is a bootstrap failure even if a shallow command succeeds.

Every accepted version must still pass the applicable functional probes:

- freestanding AArch64 C++20 compilation;
- AArch64 ELF link, inspection, and binary conversion;
- AArch64 COFF compilation and import-free EFI PE32+ link during the build;
- availability of the exact `virt-11.0` machine and pinned firmware;
- deterministic FAT32 ESP construction; and
- bounded debug and release QEMU boot tests.

Version checks constrain compatibility; probes demonstrate behavior. Neither is
a substitute for the other.

### Last-Known-Good Tuple

The repository records the exact tuple most recently used for the complete
verification matrix:

| Tool | Last known-good version |
| --- | --- |
| CMake | `4.2.3` |
| Ninja | `1.13.2` |
| LLVM/Clang | `22.1.8` |
| LLD | `22.1.8` |
| QEMU | `11.0.3` |
| mtools | `4.0.49` |
| Python | `3.14.6` |

An in-range version differing from this tuple is reported as compatible but not
the recorded known-good version. It proceeds to the functional probes rather
than failing solely because of a patch or otherwise supported update.

Changing the known-good tuple or a supported range requires a focused dependency
update that runs the complete host, AArch64, UEFI debug/release, QEMU, and
reproducibility matrix. Release provenance records the exact resolved tools used
for that build.

### Distribution Boundary

Homebrew is the supported initial macOS distribution channel for host tools. The
checked-in manifest names direct formula requirements but is explicitly not a
lock file. Warren does not automatically run `brew pin`, suppress security
updates, maintain a private tap, or promise that a new machine can install an
old known-good tuple indefinitely.

An already provisioned machine and populated Warren dependency cache can verify
and build offline. Provisioning a new machine offline is not currently promised.

## Alternatives Considered

### Exact Artifact Pinning For Every Host Tool

Warren could maintain a private Homebrew tap, preserve bottles, or download and
build exact upstream releases. This provides stronger environment
reproducibility but transfers packaging, macOS compatibility, transitive
dependency, and security-update maintenance to Warren immediately.

### Current Homebrew Versions Plus Probes

Warren could install the latest formulae and rely only on successful compilation
and boot tests. This is simple but lets unreviewed future major versions enter
the supported environment and makes regressions harder to distinguish from
project changes.

### Containers Or Virtualized Linux Builds

A container image could pin a Linux toolchain effectively. It would make the
supported development path indirect on macOS, complicate virtualization and
file/debug integration, and abandon the chosen native ARM Mac workflow. It may
later complement, but does not replace, the reference host path.

## Consequences

### Benefits

- new development Macs retain a practical installation path;
- target ABI and firmware inputs remain byte-for-byte controlled;
- future major host tools cannot enter silently;
- harmless supported updates do not require preserving all historical bottles;
- every environment reports its exact resolved versions; and
- toolchain updates have a named, testable integration procedure.

### Costs And Risks

- compatible ranges are weaker than artifact-level reproducibility;
- an in-range tool regression can still occur and must be caught by probes;
- pinned firmware hashes may require coordinated updates after QEMU packaging
  changes;
- Homebrew may remove or rename formulae used by the bootstrap; and
- maintaining version extraction and functional probes is Warren's
  responsibility.

## Follow-Up Work

- consume the machine-readable baseline during every bootstrap check;
- add focused negative tests for rejected ranges and LLVM/LLD mismatch;
- include exact resolved versions and target-input hashes in release provenance;
- document the baseline-update verification command; and
- reassess a private tap or archived toolchain when Warren has releases that
  require long-term reconstruction.

## Revisit When

- a supported Homebrew formula disappears or cannot install on the reference
  macOS host;
- in-range incompatibilities become common enough that ranges are misleading;
- Warren begins producing releases that require durable historical rebuilds;
- continuous verification needs a second independently provisioned host; or
- maintaining a private tap becomes cheaper than diagnosing environment drift.
