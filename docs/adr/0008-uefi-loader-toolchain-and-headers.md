# ADR-0008: UEFI Loader Toolchain And Headers

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 0–1
- **Supersedes:** The unresolved implementation details in ADR-0005
- **Superseded by:** None

## Context

ADR-0005 requires a Warren-owned UEFI bootloader. The implementation still needs
a language, authoritative UEFI ABI declarations, PE/COFF construction path, and
dependency boundary.

Recreating UEFI declarations would avoid a third-party source dependency but
would make Warren responsible for table ordering, structure layout, GUIDs,
calling-convention annotations, status widths, protocol revisions, and every
future correction. Importing the complete EDK2 build and runtime environment
would provide the opposite problem: far more machinery and policy than Warren
needs.

## Decision

Warren's UEFI bootloader uses the project's freestanding C++20 subset and is
built by Warren's CMake/LLVM toolchain.

The bootloader consumes a **curated, pinned snapshot of EDK2 UEFI ABI headers**.
The dependency is quarantined under the following rules:

- only headers needed by the bootloader are retained;
- source revision, selected files, original license, and any Warren patches are
  recorded in the third-party directory;
- no EDK2 build system, BaseTools, libraries, startup code, drivers, services, or
  firmware source participates in Warren's build;
- a single Warren boundary header includes the third-party declarations;
- ordinary bootloader modules consume Warren-owned wrapper types/functions where
  that prevents UEFI details from spreading;
- Burrow and userspace may not include the EDK2 headers; and
- ABI layout assertions and a real firmware smoke test validate the boundary.

EDK2 material remains under its upstream `BSD-2-Clause-Patent` terms and is not
relicensed as Apache-2.0.

The bootloader is compiled to AArch64 COFF with Clang and linked as a PE32+ UEFI
application using `lld-link`. Its entry point is an `extern "C"` EFIAPI function;
the link declares the EFI application subsystem and uses no default libraries.
The first removable-media artifact is:

```text
\EFI\BOOT\BOOTAA64.EFI
```

Warren owns the EFI System Partition construction and bootloader code. The ESP
pipeline uses host-only `mtools`; it is not a target/runtime dependency. The
retained EDK2 revision is `edk2-stable202605` commit
`b03a21a63e3bd001f52c527e5a57feddb53a690b`. Its archive and each of the 25
retained headers have checked-in SHA-256 values. QEMU's prebuilt firmware images
are likewise checked against an integrity manifest before use.

## Alternatives Considered

### Warren-owned UEFI declarations

This would minimize third-party files and maximize local understanding. Silent
ABI drift or a single incorrect table field can corrupt calls in ways that
resemble firmware, compiler, or memory bugs. Warren gains little product identity
from independently transcribing a standardized C ABI.

### Full EDK2 build environment

This provides a comprehensive, tested firmware application ecosystem. It brings
a separate build system, tools, libraries, conventions, and scope that conflict
with the goal of writing and understanding Warren's loader.

### GNU-EFI

GNU-EFI offers a mature standalone UEFI application path. Its runtime and link
model would add another abstraction/build convention alongside Warren's selected
LLVM path.

### C17 bootloader

C17 has a smaller language runtime surface. Freestanding C++20 lets the loader
share Warren's type and ownership discipline without requiring exceptions, RTTI,
allocation, or a standard library.

## Consequences

### Benefits

- authoritative UEFI declarations without adopting a loader framework;
- one compiler/build family for loader and kernel;
- no EDK2 runtime or target dependency;
- ABI corrections can be consumed through an explicit snapshot update; and
- Warren retains complete ownership of loading, validation, handoff, and errors.

### Costs And Risks

- a third-party header snapshot and license must be maintained;
- curating too aggressively can break internal header dependencies;
- COFF and ELF need distinct target/link profiles; and
- wrapper leakage could allow EDK2 conventions to spread through loader code.

### Completed Evidence

- exact EDK2 revision, minimal closure, provenance, and license are recorded;
- Clang emits AArch64 COFF and `lld-link` emits an import-free EFI application;
- the entry point and PE subsystem are verified during every build;
- ESP construction is unprivileged and byte-for-byte tested; and
- debug and release images boot on the reference QEMU firmware.

## Revisit When

- the retained headers grow beyond a bounded ABI surface;
- EDK2 declarations prevent strict freestanding compilation;
- Warren can replace them with generated declarations verified directly against
  the UEFI specification; or
- another authoritative header source materially reduces the dependency without
  increasing ABI risk.
