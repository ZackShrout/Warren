# BunnySoft Warren

Warren is BunnySoft's long-horizon hobby operating-system project. Its kernel is
Burrow. The project begins on AArch64 under emulation, is designed to admit an
x86-64 port, and ultimately aims to support native software development on
Warren itself.

Warren has completed **Phase 0: Foundation** and begun **Phase 1: First Light**.
The repository builds and independently audits Burrow's AArch64 ELF image, then
packages its debug-stripped runtime copy beside Warren's UEFI bootloader. The
bootloader opens that exact packaged file from its own boot device, validates it
with Warren-owned production C++, allocates firmware-selected pages, copies and
zero-fills the load image, constructs and validates boot information from the
final UEFI memory map, exits boot services, and transfers through the reviewed
AArch64 boundary. Pinned QEMU boots prove Burrow's assembly witness executes at
both inherited EL1 and EL2, accepts the observable handoff contract, and
installs a terminal emergency vector table at the active EL. Deliberate
synchronous exceptions at both levels report their architectural state and
terminate through `PANIC`/4. The current proof deliberately stops before EL
normalization, owned mappings, stable higher-half vectors, or
architecture-neutral kernel C++ entry.

## Project Vocabulary

| Name | Role | Status |
| --- | --- | --- |
| Warren | Operating system and product | Adopted |
| Burrow | Kernel | Adopted |
| Hare | Command shell | Adopted |
| Meadow | Desktop environment | Adopted; distant horizon |
| `warrend` | System-service manager and service suite | Working name |
| Warren SDK | Toolchain, libraries, headers, and developer tools | Adopted |
| Forage | Package manager | Adopted |

**Forage** replaces the earlier Carrot candidate, avoiding a collision with
BunnySoft's game engine while fitting Warren's vocabulary naturally.

## The First Kernel Goal

The first executable milestone is deliberately small:

> Warren boots reproducibly on QEMU's AArch64 `virt` machine, receives a
> documented boot-information structure, initializes a serial console, handles
> synchronous exceptions safely, starts a timer, and enters a diagnostic
> monitor.

It is not a GUI, a shell, or a general-purpose operating system. It is the first
trustworthy platform on which those things can eventually be built.

## Planning Index

- [`docs/VISION.md`](docs/VISION.md) — purpose, identity, principles, and explicit non-goals
- [`docs/GUARDRAILS.md`](docs/GUARDRAILS.md) — constraints that protect portability and the long-term plan
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — provisional technical architecture and subsystem boundaries
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — capability-gated horizons with honest exit criteria
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — intended macOS cross-development and verification workflow
- [`docs/BRANCHING.md`](docs/BRANCHING.md) — branch scope, integration gates, and protection of `main`
- [`docs/SELF_HOSTING.md`](docs/SELF_HOSTING.md) — staged path from cross-compilation to native development
- [`docs/FILESYSTEM.md`](docs/FILESYSTEM.md) — staged storage plan and criteria for the eventual system filesystem
- [`docs/DECISIONS.md`](docs/DECISIONS.md) — architectural-decision-record policy and decision index
- [`docs/specifications/AARCH64_BURROW_IMAGE.md`](docs/specifications/AARCH64_BURROW_IMAGE.md) — implemented Burrow ELF and packaging contract
- [`docs/specifications/AARCH64_NORMALIZED_ENTRY.md`](docs/specifications/AARCH64_NORMALIZED_ENTRY.md) — register, mapping, and generic-entry contract for the current branch
- [`CODE_STANDARDS.md`](CODE_STANDARDS.md) — Warren-specific C++ and assembly standards

## Working Agreement

- Build one observable, testable vertical slice at a time.
- Keep the last completed vertical slice bootable while replacing or refactoring
  foundational machinery.
- Treat future architecture support as a boundary-design requirement, not as a
  demand to implement two kernels simultaneously.
- Prefer established formats and conventions unless Warren has a concrete
  reason to differ.
- Record decisions and their consequences. Do not let accidental implementation
  details become policy.
- Keep distant goals visible, but never describe them as current capabilities.

## Host Bootstrap

On the initial macOS development host, verify dependencies without changing the
machine:

```sh
./tools/bootstrap.sh --check
```

Install missing Homebrew dependencies, run the complete AArch64 toolchain probe,
and generate ignored machine-local paths under `.warren/`:

```sh
./tools/bootstrap.sh --install
```

After bootstrap, build and test the current combined debug system through the
checked-in presets:

```sh
cmake --preset system-aarch64-debug
cmake --build --preset system-aarch64-debug
ctest --preset system-aarch64-debug
```

The build keeps Burrow and UEFI in separate compiler environments, then places
`BOOTAA64.EFI` and `BURROW.ELF` into the 64 MiB
`build/system-aarch64-debug/artifacts/warren-system-esp.img`. CTest extracts and
compares both packaged payloads, rebuilds the ESP twice for byte equality, runs
the production loader against the generated runtime ELF on the host, and boots
it on the pinned QEMU machine. The combined test requires matching serial and
process results for `burrow-first-entry`: UEFI loads Burrow, constructs and
validates the final boot-information object, exits boot services, and transfers
through the reviewed AArch64 boundary; Burrow then validates the directly
observable entry state and reports the terminal result. The system-only Burrow
children contain the QEMU result transport. Separate target fixtures prove
matching pass, explicit-failure, and panic serial/process results; UEFI fault
fixtures prove rejection of malformed finalized handoff data and loader-side
post-exit containment. Focused Burrow products do not contain that transport.

Before final map capture, loader diagnostics use the UEFI console. After a
successful exit, the loader and first-entry witness use minimal direct PL011
output under the inherited firmware identity mapping. That one-way output is
not the later reusable Burrow console. Semihosting exists only in trusted QEMU
test artifacts and is neither an ordinary shutdown path nor a physical-machine
interface.

The `aarch64-debug` and `uefi-aarch64-debug` presets remain available for
focused product builds. The Burrow, UEFI, and system profiles each have a
matching release preset.

## Licensing

Warren-owned source and documentation are licensed under the
[Apache License 2.0](LICENSE). Third-party material retains its original license
and provenance.
