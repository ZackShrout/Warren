# Warren Development Model

This document defines the host-to-target workflow. Bootstrap records exact
resolved paths and tool versions in ignored machine-local state so a build does
not depend on shell path order or Apple's platform defaults.

Repository integration follows `BRANCHING.md`. Development occurs on focused
branches while `main` remains the last verified integrated state. Build profiles
are not Git branches: each work branch must preserve every profile required by
its integration gate.

## Host And Target Are Different Products

The initial host is macOS on Apple Silicon. The initial target is an AArch64
machine defined by QEMU, not macOS and not Apple Silicon hardware.

Host tools may use macOS facilities. Target code may not include host headers,
link host libraries, inspect host paths at runtime, or assume that the host and
target share an architecture merely because both are AArch64.

The adopted first toolchain direction is:

- LLVM/Clang cross-compilation and LLVM binary utilities;
- CMake as build-description and orchestration system;
- Ninja as the reference build executor;
- QEMU system emulation;
- a pinned UEFI firmware image for the reference machine;
- LLDB or GDB according to the selected remote-debugging workflow; and
- Python 3 for bounded host-side tooling where a compiled tool would add no
  target value.

ADR-0006 records the CMake/Ninja and LLVM direction. The first verified toolchain
baseline is CMake 4.2.3, Ninja 1.13.2, LLVM/LLD 22.1.8, QEMU 11.0.3, mtools
4.0.49, and Python 3.14.6 on an ARM Mac. Homebrew remains the distribution
channel; the bootstrap report records the exact resolved versions on every
machine. ADR-0014 defines supported major/minor ranges separately from this
last-known-good tuple; bootstrap rejects out-of-range versions and LLVM/LLD
mismatches before functional probes. The build may not depend on Xcode's
implicit platform target, SDK, linker defaults, or system headers.

## Required Developer Interfaces

Once implementation begins, the repository should expose stable front doors for:

```text
configure   create a target-specific build tree
build       produce bootable and debug artifacts
run         launch the reference QEMU machine interactively
test        run host tests and bounded emulator tests
debug       launch QEMU paused with a documented debugger attachment
image       assemble a reproducible boot image without launching it
clean       remove a named build tree, never source or dependency caches
```

The concrete interface may be CMake presets plus small scripts or another thin
wrapper. Developers should not need to reconstruct a long QEMU command from a
wiki page.

## Host Bootstrap Boundary

`tools/bootstrap.sh` is the dependency front door for the initial macOS host.
Its default `--check` mode is read-only. `--install` explicitly authorizes it to
install missing formulae from the checked-in macOS manifest, run an AArch64
compile/link/object-conversion probe, verify QEMU and firmware assets, and write
machine-local paths under ignored `.warren/`.

Ordinary CMake configuration never installs Homebrew formulae or accesses the
network. It validates the paths resolved by bootstrap and gives an actionable
error if they are absent or stale. Heavy host tools such as LLVM and QEMU do not
use CMake `FetchContent`.

The bootstrap does not silently install Homebrew, developer-command-line tools,
or other machine-wide package managers. Those foundational host changes remain
explicit prerequisites. Source-build fallbacks may be added only for tools that
lack a trustworthy binary distribution, with pinned sources and checksums.

EDK2 ABI headers are the bounded exception to the system-package path. Bootstrap
downloads one immutable upstream archive, verifies its SHA-256, and extracts
only the 28 individually hashed files on the checked-in allowlist. QEMU's UEFI
code and variable-store images are also verified against a checked-in integrity
manifest before any build paths are generated.

## Current Burrow First-Entry Slice

The stable combined developer commands are:

```sh
./tools/bootstrap.sh --check
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
cmake --preset system-aarch64-debug
cmake --build --preset system-aarch64-debug
ctest --preset system-aarch64-debug
```

Use `system-aarch64-release` for the matching optimized build. Each system
profile configures isolated Burrow and UEFI child trees, compiles them for
`aarch64-none-elf` and `aarch64-pc-windows-msvc` respectively, and composes only
their verified outputs. It does not compile either product using the other's
environment.

CTest extracts the bootloader and runtime ELF from the combined ESP and compares
their bytes to the selected inputs, proves complete ESP reproducibility,
materializes the generated runtime ELF through the production C++ loader on the
host, and runs the combined `burrow-first-entry` QEMU test through explicit EL1
and EL2 profiles. `WarrenSystemBurrowFirstEntryEl1` names
`virtualization=off`; `WarrenSystemBurrowFirstEntryEl2` names
`virtualization=on`. Both gates require the requested post-exit loader EL,
Burrow's matching first-entry observation, and agreement between Burrow's
terminal PL011 record and QEMU semihosting status. The host profile also
exercises the profile-selection logic, independent malformed loader fixtures,
handoff storage and finalization, the artifact verifier, ESP input failures,
the boot-information ABI, the serial classifier, and toolchain gates.

The combined system build explicitly enables the test-only QEMU transport in
its Burrow child. Focused `aarch64-debug` and `aarch64-release` products leave
that option off, and their artifact verification rejects the semihosting trap,
terminal markers, argument blocks, and fault injection. The system build also
composes dedicated failure, panic, and emergency-vector ESP fixtures.
`WarrenSystemEmergencyVectorEl1` and `WarrenSystemEmergencyVectorEl2` inject the
same `BRK #0x777` after the validated console record and require vector class 4,
ESR `0xF2000777`, the requested current EL, and agreed `PANIC`/4.
`WarrenSystemQemuResultFailure` requires an agreed `FAIL`/64 result, while
`WarrenSystemQemuResultPanic` requires an agreed
`PANIC`/2 result through the same QEMU harness used by the successful boot.
`WarrenSystemBurrowRejectsInvalidHeader` corrupts the finalized magic after
firmware exit and requires `FAIL`/68 with no Burrow success diagnostic.
`WarrenSystemBurrowRejectsInvalidConsole` clears the finalized console output
flag and requires `FAIL`/72, proving that the QEMU failure reporter does not
trust the rejected record.
`WarrenSystemPostExitFailureContainment` enters the loader's nonreturning
post-exit failure routine and requires its direct diagnostic plus `FAIL`/73,
again without entering Burrow.

Focused profiles remain supported:

```sh
cmake --preset aarch64-debug
cmake --build --preset aarch64-debug

cmake --preset uefi-aarch64-debug
cmake --build --preset uefi-aarch64-debug
ctest --preset uefi-aarch64-debug
```

`cmake --build build/aarch64-debug --target BurrowDisassembly` prints
source-aware AArch64 disassembly without generating a persistent report.
The focused UEFI ESP intentionally contains no Burrow payload, so its CTest
surface audits structure and reproducibility rather than presenting a QEMU boot
as successful. Use the combined system preset for the loader proof.

## Build Trees And Artifacts

Build outputs remain outside source directories. A conventional local layout is:

```text
build/
  host-debug/
  aarch64-debug/
  aarch64-release/
  uefi-aarch64-debug/
  uefi-aarch64-release/
  system-aarch64-debug/
    products/host/
    products/burrow/
    products/uefi/
    artifacts/
  system-aarch64-release/
  x86_64-debug/       future
```

Generated artifacts should have unambiguous roles:

- unstripped ELF image for symbols and debugging;
- stripped or packaged image used by the boot environment;
- linker map;
- disassembly on request rather than as an always-dirty source artifact;
- boot image or EFI system partition image;
- captured test serial log; and
- machine-readable test result.

No generated file is checked in unless it is a deliberate fixture whose source
and regeneration procedure are documented.

The bare AArch64 profiles emit `burrow.elf`, `burrow-runtime.elf`, and
`burrow.map` under their `artifacts/` directory. The combined profiles emit
`warren-system-esp.img` under their own `artifacts/` directory. Exact image and
packaging contracts live in `specifications/AARCH64_BURROW_IMAGE.md`.

## Build Profiles

### Debug

Debug is the primary development profile. It favors:

- symbols and frame information;
- assertions and expensive invariant checks;
- deterministic initialization patterns where helpful;
- sanitizer-like kernel diagnostics implemented within Warren's constraints;
- verbose subsystem logging; and
- optimization sufficient to expose realistic compiler behavior without
  destroying debuggability.

### Release

Release favors representative performance and size while preserving defined
panic and error behavior. Disabling assertions must not remove required input
validation or create unused-variable side effects.

Both profiles must boot-test. A debug-only system is not releasable, and a
release-only failure is still a failure.

## Reproducibility And Inputs

- Toolchain, QEMU machine type, firmware, and third-party source revisions are
  pinned or recorded precisely.
- Downloads are checksummed and performed by an explicit dependency/bootstrap
  step, not during an ordinary compile.
- The build records the Warren revision and relevant tool versions.
- Absolute host paths do not enter target interfaces or release metadata unless
  explicitly scrubbed or mapped.
- Build instructions begin from a clean checkout and a named dependency state.
- Offline rebuilds become a requirement once the dependency cache is populated.

Bit-for-bit reproducibility is a later gate, but unexplained inputs are forbidden
from the beginning.

## Warning And Link Policy

Warren-owned code compiles with a strict warning set and treats warnings as
errors in continuous verification. Third-party code may use an isolated warning
policy; its warnings may not be hidden by weakening Warren's policy globally.

The final kernel link must:

- use an explicit linker script;
- emit a map file;
- reject unresolved symbols;
- avoid host startup files and libraries;
- make every deliberately supplied compiler-runtime object visible; and
- fail if incompatible architecture objects enter the image.

Undefined behavior is never an architecture abstraction.

## Test Layers

### Formatting and static checks

Fast checks validate source layout, forbidden dependencies, generated-file
drift, and mechanical standards. They should not rewrite developer files during
CI.

### Host unit tests

Target-independent algorithms—parsers, allocators over simulated memory,
containers, format validation, scheduling policy models—run as ordinary host
tests. Target dependencies are injected or adapted rather than hidden behind
uncontrolled compile definitions.

### Emulator unit and integration tests

Target-only behavior runs in QEMU. A test boot has:

- a fixed timeout;
- exactly one versioned test invocation and unique identifier;
- strict `BEGIN`, `PASS`, `FAIL`, and `PANIC` serial records;
- an agreed serial result and test-only semihosting process status;
- distinct pass, explicit-failure, panic/assertion, protocol-error, QEMU-failure,
  and timeout classifications;
- serial logs retained on failure; and
- no semihosting support in production or interactive artifacts.

The exact ASCII grammar, guest codes, host statuses, ordering, and disagreement
precedence are fixed in `specifications/TEST_RESULT_PROTOCOL_V1.md`. The current
combined proof uses the two-channel `burrow-first-entry` contract after UEFI
boot services have ended. It proves Burrow assembly execution, not the later
normalized kernel entry or an ordinary shutdown facility.

### Interactive tests

Interactive experiments are valuable but do not replace automated gates. Their
steps and expected observations belong in a test note until automated.

## Debugging Workflow

From First Light onward, the supported debug path should provide:

- QEMU stopped before or at kernel entry;
- debugger symbols loaded at their actual virtual addresses;
- architecture-aware register and disassembly views;
- a panic message that includes a stable identifier and exception frame;
- a symbolization tool for captured addresses; and
- an emulator trace mode that is opt-in and bounded.

Every change to virtual layout, relocation, or symbol stripping must update and
test this workflow.

## Dependency Policy

Third-party code is allowed when it provides leverage. Each dependency requires:

- purpose and owning subsystem;
- source and license;
- pinned version and integrity information;
- target/runtime requirements;
- local patches kept explicit; and
- a removal or replacement story where it affects a stable Warren interface.

The kernel has a much higher dependency bar than host tools or user applications.
A header-only library is still a dependency.

## Continuous Verification

The initial CI-equivalent sequence, whether run locally or by a hosted service,
should be:

1. validate formatting and repository invariants;
2. configure from a clean build tree;
3. build host tools and host tests;
4. run host tests;
5. build and byte-audit Burrow debug and release symbol/runtime images;
6. build and test the focused UEFI AArch64 debug and release images;
7. build combined debug and release images through isolated child toolchains;
8. verify packaged bytes, ESP reproducibility, and bounded QEMU completion for
   both combined profiles; and
9. retain logs, maps, and images for failed runs.

The future x86-64 checkpoint joins this matrix without making every experimental
platform a required gate.

## Contribution Shape

A foundational change should include:

- the smallest coherent implementation;
- tests at the lowest useful layer;
- updated interface or decision documentation;
- an explanation of new toolchain/runtime dependencies; and
- no unrelated style or generated-output churn.

Code review asks whether the current phase becomes demonstrably closer to its
exit gate, not merely whether the code is interesting.
