# BunnySoft Warren

Warren is BunnySoft's long-horizon hobby operating-system project. Its kernel is
Burrow. The project begins on AArch64 under emulation, is designed to admit an
x86-64 port, and ultimately aims to support native software development on
Warren itself.

Warren has completed **Phase 0: Foundation** and **Phase 1: First Light** and
has begun **Phase 2: Burrow Core**.
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
terminate through `PANIC`/4. Burrow now also runs the complete shared
boot-information validator and deterministically reserves its immutable
128-page transition arena from validated `usable` memory. Burrow then abandons
the inherited firmware translation and cache state: the inherited EL1 route
stays at EL1, while the inherited EL2 route performs the reviewed one-way
`ERET`; both reach the same physical, MMU-off EL1h state with masked DAIF. The
common path validates the implemented 4 KiB granule and physical-address width,
then materializes and independently audits bounded four-level TTBR0/TTBR1
hierarchies in the reserved arena. It preflights every live source and target,
activates the owned EL1 regime, branches to the stable image alias, installs the
stable vectors and guarded stack, rebases retained resources, and replaces
TTBR0 with an empty root. It then constructs the fixed 64-byte entry context,
calls architecture-neutral kernel C++, revalidates the aliased boot object,
selects the validated QEMU-virt PL011 through platform code, emits a bounded
allocation-free console diagnostic, writes the retained witness only after the
complete line succeeds, constructs a fixed-capacity inventory from validated
usable memory below the 64 TiB direct-map ceiling, excludes page zero and the
transition arena, reserves the lowest aligned four-page boot extent, publishes
`BURROW_MEMORY_V1`, and accepts only the exact success return. The owned
stable EL1 vectors now preserve all 31 GPRs, interrupted SP, transition stage,
and syndrome state in a fixed 320-byte ABI frame, emit a versioned bounded
exception record through the reusable PL011 path, and terminate without
recursive recovery. After the C++ witness succeeds, CPU 0 configures the
QEMU-virt GICv3 and non-secure physical timer, receives exactly one PPI 30
interrupt through that same complete frame, restores every GPR, returns with
`ERET`, masks further IRQ delivery, and emits a bounded `BURROW_TIMER` proof.
Burrow then advertises the fixed `help,status,exit` monitor, receives `status`
and `exit` over PL011, reports the observed tick, and proceeds only after the
bounded session exits successfully. A retained production panic entry now masks
interrupts, classifies assertion and kernel-panic records separately, publishes
one bounded `BURROW_PANIC_V1` line with a stable identifier and source location,
and enters a nonreturning wait. Dedicated post-monitor fixtures prove assertions
as `PANIC`/2 and kernel panics as `PANIC`/3 through the isolated test transport.
Every Burrow link now structurally verifies its LLD map against the ELF entry and
image extent. Debug builds also prove that LLVM resolves assembly and C++ DWARF
through ELF-relative, fixed stable-alias, and explicit physical-load addresses,
then generate a ready-to-source LLDB command file. Opt-in EL1 and EL2 system
targets start the pinned QEMU profile paused behind a loopback-only GDB stub.

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
- [`docs/specifications/AARCH64_NORMALIZED_ENTRY.md`](docs/specifications/AARCH64_NORMALIZED_ENTRY.md) — implemented register, mapping, and generic-entry contract
- [`docs/specifications/PHYSICAL_MEMORY.md`](docs/specifications/PHYSICAL_MEMORY.md) — implemented normalized inventory and bounded boot-allocation contract
- [`docs/specifications/PL011_CONSOLE.md`](docs/specifications/PL011_CONSOLE.md) — implemented allocation-free console and PL011 driver contract
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
process results for `aarch64-normalized-entry`: UEFI loads Burrow, constructs and
validates the final boot-information object, exits boot services, and transfers
through the reviewed AArch64 boundary; Burrow then validates the directly
observable entry state and reports the terminal result. The system-only Burrow
children contain the QEMU result transport. Both live profiles must also emit
the reusable `BURROW_CONSOLE` diagnostic from C++, the one-shot
`BURROW_TIMER` diagnostic after a handled IRQ, a bounded `BURROW_MONITOR`
ready/status/exit exchange over the same PL011, and the matching
`BURROW_NORMALIZED_ENTRY` diagnostic after C++ returns. Separate
target fixtures prove matching pass, explicit-failure, and panic serial/process
results, production assertion and kernel-panic paths, every normalized failure
allocation from 75 through 83, both stack guards, text-write and data-execute
protection, and stale-identity removal.
UEFI fault fixtures prove rejection of malformed finalized handoff data and
loader-side post-exit containment. Focused Burrow products do not contain that
transport or any fault injection.

Debug builds also emit `burrow-stable.lldb`. To start the complete system paused
and attach LLDB from another terminal:

```sh
cmake --build build/system-aarch64-debug --target WarrenSystemDebug-el1
cmake --build build/system-aarch64-debug --target WarrenSystemLldb
```

Use `WarrenSystemDebug-el2` for the inherited EL2 route. The debugger listener
is bound only to `127.0.0.1:1234`; the QEMU launcher uses a disposable copy of
the firmware variable store.

Before final map capture, loader diagnostics use the UEFI console. After a
successful exit, the loader and first-entry witness use minimal direct PL011
output under the inherited firmware identity mapping. Those assembly paths are
not reusable consoles. After owned translation is active and every identity
mapping has been removed, platform C++ constructs the allocation-free PL011
reader and writer on the checked upper MMIO alias. Receive remains bounded and
polling-only; the normalized assembly witness and
test transport remain independent. Semihosting exists
only in trusted QEMU test artifacts and is neither an ordinary shutdown path nor
a physical-machine interface.

The `aarch64-debug` and `uefi-aarch64-debug` presets remain available for
focused product builds. The Burrow, UEFI, and system profiles each have a
matching release preset.

## Licensing

Warren-owned source and documentation are licensed under the
[Apache License 2.0](LICENSE). Third-party material retains its original license
and provenance.
