# BunnySoft Warren

Warren is BunnySoft's long-horizon hobby operating-system project. Its kernel is
Burrow. The project begins on AArch64 under emulation, is designed to admit an
x86-64 port, and ultimately aims to support native software development on
Warren itself.

Warren is currently in **Phase 0: Foundation**. There is intentionally no Burrow
kernel implementation yet. The repository does contain the completed UEFI
first-light scaffold: Warren's AArch64 bootloader can be built, placed in an EFI
System Partition, booted under QEMU, and smoke-tested automatically. This proves
the toolchain and boot-image pipeline without pretending the kernel exists.

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
- [`docs/SELF_HOSTING.md`](docs/SELF_HOSTING.md) — staged path from cross-compilation to native development
- [`docs/FILESYSTEM.md`](docs/FILESYSTEM.md) — staged storage plan and criteria for the eventual system filesystem
- [`docs/DECISIONS.md`](docs/DECISIONS.md) — architectural-decision-record policy and decision index
- [`CODE_STANDARDS.md`](CODE_STANDARDS.md) — Warren-specific C++ and assembly standards

## Working Agreement

- Build one observable, testable vertical slice at a time.
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

After bootstrap, build and test the current UEFI slice through the checked-in
presets:

```sh
cmake --preset uefi-aarch64-debug
cmake --build --preset uefi-aarch64-debug
ctest --preset uefi-aarch64-debug
```

The build produces `BOOTAA64.EFI` and a 64 MiB `warren-esp.img` under
`build/uefi-aarch64-debug/artifacts/`. The smoke test boots the image on the
pinned QEMU machine and requires
`WARREN_TEST:1:PASS:uefi-first-light` before firmware shutdown.

## Licensing

Warren-owned source and documentation are licensed under the
[Apache License 2.0](LICENSE). Third-party material retains its original license
and provenance.
