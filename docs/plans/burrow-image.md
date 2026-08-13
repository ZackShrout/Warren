# Burrow Image Branch Plan

- **Status:** Complete pending integration
- **Branch:** `feature/burrow-image`
- **Base:** `main` after integration of `foundation/phase-0-contracts`
- **Roadmap phase:** Phase 1 — First Light
- **Primary outcome:** Produce, audit, and package Warren's first Burrow ELF
  image without loading or executing it.

## Why This Branch Is Next

Phase 0 fixed the contracts that constrain the first kernel artifact. ADR-0002
selects the freestanding C++20 and assembly boundary, ADR-0006 requires a
separate AArch64 Burrow toolchain environment, ADR-0009 selects a static
position-independent ELF64 `ET_DYN` image, and ADR-0016 bounds its ELF-relative
virtual addresses and later stable higher-half placement.

The next useful proof is therefore not a kernel handoff. It is evidence that
Clang and LLD can produce exactly the image Warren intends to load, with no host
runtime, accidental dynamic-linking surface, ambiguous segment permissions, or
unreviewed relocation type. Keeping image production separate from image
loading lets the following loader branch consume a stable, independently
audited artifact rather than debugging both halves of the format at once.

## Governing Decisions And Authority

This branch implements accepted ADRs 0002, 0006, 0009, and 0016. It does not
modify their text and is not expected to require a new architectural decision.
The exact linker layout and artifact-audit rules are subordinate implementation
contracts under those decisions.

Work stops for explicit architectural discussion if implementation evidence
would require any of the following:

- a fixed physical load address or an ELF type other than `ET_DYN`;
- a runtime relocation other than `R_AARCH64_RELATIVE`;
- a dynamic interpreter, shared-library dependency, or target runtime library;
- a loadable ELF virtual address at or above `0x80000000`;
- a writable and executable loadable page;
- compiling Burrow and the UEFI bootloader as though they shared one target
  environment; or
- a new third-party dependency.

## Observable Completion

At branch completion:

- the AArch64 debug and release profiles produce a static position-independent
  Burrow ELF64 `ET_DYN` image and linker map;
- the image has an architecture-owned entry symbol but no code path transfers
  control to it yet;
- an independent host-side verifier proves the ELF identity, bounds, entry,
  segment, dynamic metadata, relocation, and forbidden-runtime contracts;
- every loadable page has one unambiguous permission class and the image
  contains real read-only, executable, writable, and zero-filled content;
- a packaged runtime copy and an unstripped symbol-bearing copy have explicit
  roles;
- a combined system-image profile places the runtime image at
  `EFI/WARREN/BURROW.ELF` beside the existing removable-media bootloader;
- the packaged Burrow bytes are verified against the source runtime artifact;
- the combined ESP remains reproducible; and
- QEMU still completes the existing UEFI pipeline test, with no claim that the
  bootloader has opened, parsed, loaded, relocated, or entered Burrow.

The visible success remains the existing headless UEFI `BEGIN`/`PASS` result.
Burrow execution belongs to later branches.

## Deliverables

### 1. Minimal Burrow Source Boundary

Create only the kernel directories that acquire owned content. The expected
initial ownership is:

```text
kernel/
  include/burrow/             architecture-neutral kernel interfaces
  linker/AArch64/burrow.ld    AArch64 ELF image layout
  src/Core/                   freestanding C++ image content
  src/Arch/AArch64/           AArch64 entry symbol and required assembly
```

The ELF entry is the C-compatible symbol `burrow_aarch64_entry`, implemented in
an AArch64 assembly file under the architecture layer. In this branch it is a
deliberately nonfunctional image entry, not the ADR-0011 normalization path.
Nothing calls it, and no passing test may imply that it has run. The later
`feature/burrow-first-entry` branch owns the first transfer, while
`feature/aarch64-normalized-entry` owns the complete normalized entry path.

At least one retained input contributes to each loadable permission class and
to zero-filled storage. This prevents an empty linker-script declaration from
masquerading as evidence that text, read-only data, writable data, and BSS are
actually separated. Temporary layout sentinels must be named as such and carry
a documented removal gate when real kernel content supersedes them.

All Warren-owned source and linker inputs follow `CODE_STANDARDS.md`, including
the creation-date ownership header. Assembly remains limited to the entry
boundary; image metadata, verification policy, and ordinary logic do not move
into assembly.

### 2. Freestanding Compile And Link Contract

The `aarch64-debug` and `aarch64-release` presets own the Burrow build. The
target uses `aarch64-none-elf`, the checked-in AArch64 Warren toolchain, and
target-scoped options that enforce:

- freestanding C++20 or deliberate C17;
- no exceptions, RTTI, stack protector, thread-safe statics, or C++ atexit
  machinery;
- no host headers, startup files, default libraries, or platform SDK;
- hidden-by-default implementation visibility;
- function and data sections suitable for audited garbage collection;
- no unresolved symbols; and
- no build identifier or host-path-bearing metadata in the runtime image.

LLD links through the explicit `burrow.ld` script. The script and link options
produce these loadable classes on distinct 4 KiB pages:

1. read-only ELF headers and retained read-only metadata;
2. read-only executable code;
3. read-write, execute-never initialized data and zero-filled BSS.

Every `PT_LOAD` uses 4 KiB-compatible file and virtual alignment, satisfies the
ELF offset/virtual-address congruence rule, and remains disjoint after page
rounding. The first loadable virtual address is zero so the physical load bias
from ADR-0009 is also the base of the complete loaded image. All loadable bytes
and the entry remain below the ADR-0016 limit of `0x80000000`.

The link may contain only the dynamic metadata needed to describe the accepted
relative-relocation model. It has no interpreter or needed library. Zero
runtime relocations is valid for this minimal image; if any runtime relocation
is emitted, every entry must be `R_AARCH64_RELATIVE`. A synthetic relocation is
not added merely to make the table nonempty—the loader branch will test the
supported relocation with purpose-built fixtures.

The final link emits:

```text
artifacts/burrow.elf          unstripped ELF used for symbols and inspection
artifacts/burrow-runtime.elf  debug-stripped ELF placed in the ESP
artifacts/burrow.map          linker map for human and automated review
```

Stripping removes only non-runtime debug material. The verifier confirms that
the runtime copy preserves the entry, program headers, loadable bytes, dynamic
metadata, and relocations required for loading. Disassembly remains an explicit
developer target rather than an always-generated source of build churn.

### 3. Burrow Image Verifier

Add a bounded Warren-owned host tool that reads ELF64 little-endian structures
directly from bytes using the Python standard library. It is an artifact
verifier, not a reusable target ELF library and not the implementation that the
UEFI loader will trust.

The verifier rejects at least:

- truncated headers, tables, segments, sections, and arithmetic overflow;
- a class, byte order, ELF version, object type, or machine other than the
  selected AArch64 contract;
- an entry outside an executable `PT_LOAD`;
- load ranges outside the 2 GiB ELF-relative image window;
- invalid alignment, offset congruence, file-size/memory-size, overlap, or
  page-permission combinations;
- writable-executable loadable pages;
- `PT_INTERP`, `PT_TLS`, dynamic dependencies, imports, undefined target
  symbols, PLT-style binding, or initialization/finalization arrays;
- any runtime relocation other than `R_AARCH64_RELATIVE`;
- relocation targets outside writable loaded storage; and
- compiler-runtime or host-runtime symbols not deliberately owned by Warren.

Host tests exercise positive parsing and focused malformed variants rather than
validating only the one file LLD currently emits. LLVM inspection tools may
produce supplementary human-readable reports, but the gate does not scrape
their presentation text as its primary parser.

The built debug, release, unstripped, and runtime images all pass the applicable
audit. Expected differences between symbol and runtime copies are asserted
rather than assumed.

### 4. Separate Toolchains And System-Image Orchestration

The UEFI application and Burrow remain separate CMake target environments:

- `aarch64-*` compiles and links AArch64 ELF Burrow artifacts;
- `uefi-aarch64-*` compiles and links the ARM64 PE/COFF bootloader; and
- new `system-aarch64-*` orchestration profiles assemble the matching outputs
  into one ESP and own combined-image tests.

The orchestration layer invokes explicit child configure/build targets or an
equivalent dependency graph; it does not recompile one product with the other
product's compiler target. Debug combines with debug and release with release.
The generated system ESP lives under its own build tree so a UEFI-only artifact
cannot be mistaken for a complete Warren system image.

The stable combined front doors become:

```sh
cmake --preset system-aarch64-debug
cmake --build --preset system-aarch64-debug
ctest --preset system-aarch64-debug

cmake --preset system-aarch64-release
cmake --build --preset system-aarch64-release
ctest --preset system-aarch64-release
```

The existing host, bare AArch64, and UEFI presets remain directly usable for
focused work and regression testing.

### 5. Deterministic ESP Packaging

Extend the Warren-owned ESP builder to accept the verified Burrow runtime image
as an explicit input and place it at:

```text
EFI/BOOT/BOOTAA64.EFI
EFI/WARREN/BURROW.ELF
```

Directory and file timestamps remain normalized. The builder fails on a missing
or ambiguous input and does not search a developer's filesystem for artifacts.
A packaging test extracts or independently reads both files and proves their
bytes match the selected bootloader and runtime-image inputs. Reproducibility is
tested by constructing the combined ESP twice from identical inputs.

The UEFI bootloader is unchanged except where build plumbing requires explicit
artifact paths. It does not inspect `BURROW.ELF` in this branch. The existing
firmware-only UEFI ESP and smoke test remain valid regression surfaces while the
combined system image becomes the main path toward the loader.

### 6. Documentation Reconciliation

Add a subordinate AArch64 Burrow-image specification recording the exact
implemented ELF header, program-header, dynamic-tag, relocation, symbol,
section, and packaging rules. Update `DEVELOPMENT.md`, `ARCHITECTURE.md`,
`ROADMAP.md`, and the README where their build commands or current capability
statements change.

Accepted ADR text is not edited. If implementation contradicts an accepted
decision, the code and subordinate documentation do not redefine history; work
returns to explicit architectural discussion.

## Implementation Order

1. Add host-tested ELF decoding and malformed-image fixtures for the properties
   the artifact gate must enforce.
2. Add the minimal C++ and AArch64 source boundary, linker script, and Burrow
   CMake target.
3. Produce and audit debug and release `burrow.elf`, map, and runtime artifacts;
   eliminate every unexpected section, symbol, helper, and relocation.
4. Extend deterministic ESP construction and add byte-identity packaging tests.
5. Add the system-image orchestration presets without weakening the separate
   Burrow and UEFI toolchain environments.
6. Reconcile operator and architecture documentation, then run the complete
   clean integration matrix.

Implementation may expose a better local order, but the verifier is present
before the image contract is treated as proven and combined packaging does not
precede a passing image audit.

## Explicit Non-Goals

- Opening or parsing ELF from the UEFI bootloader
- Allocating pages for Burrow through UEFI
- Loading, zero-filling, or relocating Burrow in target code
- Constructing boot-information v1 for a real handoff
- Calling `ExitBootServices()` for kernel transfer
- Executing `burrow_aarch64_entry`
- Establishing a production stack, console, exception vector, or privilege
  normalization path
- Building page tables or entering the stable higher-half virtual image
- Adding x86-64 source or speculative multi-architecture linker abstraction
- Supplying libc, libc++, a general compiler runtime, heap, constructors, or
  global initialization support
- Compression, signing, secure boot, kernel modules, or a general boot manifest
- Turning the host verifier into production loader code

## Verification Matrix

The branch must pass the existing integration surfaces:

```sh
./tools/bootstrap.sh --check

cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset aarch64-debug
cmake --build --preset aarch64-debug

cmake --preset aarch64-release
cmake --build --preset aarch64-release

cmake --preset uefi-aarch64-debug
cmake --build --preset uefi-aarch64-debug
ctest --preset uefi-aarch64-debug

cmake --preset uefi-aarch64-release
cmake --build --preset uefi-aarch64-release
ctest --preset uefi-aarch64-release
```

It must also pass the new combined paths:

```sh
cmake --preset system-aarch64-debug
cmake --build --preset system-aarch64-debug
ctest --preset system-aarch64-debug

cmake --preset system-aarch64-release
cmake --build --preset system-aarch64-release
ctest --preset system-aarch64-release
```

At least the host, Burrow debug/release, and combined system debug/release paths
are configured from clean build trees before merge. The artifact verifier runs
as a dependency of every Burrow and combined image, not solely as an optional
CTest.

## Merge Gates

- `burrow.elf` is ELF64, little-endian, AArch64, `ET_DYN`, statically linked,
  and position-independent under the accepted load-bias model.
- The entry lies in an executable load segment and every loadable address is
  below `0x80000000`.
- Read-only, executable, writable, and zero-filled content are present and
  page-separated; no loaded page is writable and executable.
- No interpreter, needed library, target import, unexpected undefined symbol,
  constructor runtime, TLS, or unowned compiler helper is present.
- Every runtime relocation, if any, is `R_AARCH64_RELATIVE` and targets writable
  loaded storage.
- Debug and release artifacts, maps, and runtime copies have documented roles
  and pass automated inspection.
- The combined ESP contains byte-identical bootloader and Burrow inputs at the
  documented paths and is reproducible in both profiles.
- The existing UEFI QEMU test continues to pass in debug and release; its output
  is not presented as Burrow execution evidence.
- Burrow and UEFI retain distinct compiler targets and build environments.
- Host verifier tests include malformed format, bounds, permission, dynamic
  metadata, relocation, and symbol cases.
- No accepted ADR is edited and no new dependency is introduced.
- Documentation states exactly what is built, packaged, and still unexecuted.

## Principal Risks

### Mistaking `ET_DYN` For Proven Position Independence

The ELF type alone is not enough. Absolute references, imports, an interpreter,
or an unexpected relocation can still make the image unusable at an arbitrary
physical bias. The byte-level audit and symbol/relocation gates provide the
evidence.

### Mixing Firmware And Kernel Toolchains

Both products are AArch64, but one is ARM64 PE/COFF in a UEFI environment and
the other is freestanding ELF. The system-image layer composes artifacts only;
it never erases that distinction for IDE convenience.

### Testing Empty Permission Classes

A linker script can describe ideal sections that no object populates. Retained
layout content and assertions ensure all permission and BSS rules are exercised
in both optimization profiles.

### Letting The Host Verifier Become A Second Loader

The verifier needs enough ELF knowledge to reject a bad build, but not target
allocation, relocation application, or firmware behavior. Production parsing
and malformed-input behavior remain the next loader branch's responsibility.

### Packaging Symbols Or Host Paths Accidentally

The symbol-bearing ELF is valuable for debugging but is not automatically the
runtime artifact. Explicit stripping, prefix mapping, byte inspection, and
separate artifact names prevent the ESP from inheriting avoidable host details.

## Expected Commit Shape

The implementation should remain reviewable in approximately these coherent
steps:

1. host ELF verifier and its focused tests;
2. Burrow source boundary, linker script, and audited image targets;
3. runtime artifact, system-image orchestration, and deterministic packaging;
4. documentation reconciliation and final verification evidence.

Commit boundaries may move when one invariant cannot be split safely, but
unrelated cleanup does not join the branch.
