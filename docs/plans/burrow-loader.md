# Burrow Loader Branch Plan

- **Status:** Complete — verified 2026-08-13
- **Branch:** `feature/burrow-loader`
- **Base:** `main` after integration of `feature/burrow-image`
- **Roadmap phase:** Phase 1 — First Light
- **Primary outcome:** Locate, validate, allocate, load, zero-fill, and relocate
  the packaged Burrow image through UEFI without leaving boot services or
  transferring execution.

## Why This Branch Is Next

The completed Burrow-image slice now produces one independently audited runtime
artifact and packages its exact bytes at `EFI/WARREN/BURROW.ELF`. ADR-0005
assigns ELF loading to Warren's UEFI bootloader, while ADR-0009 defines the
physical load-bias model and permits only `R_AARCH64_RELATIVE` runtime
relocations. Those contracts make production loading the next smallest
dependency-complete proof.

This branch deliberately stops before handoff. File discovery, untrusted-byte
validation, firmware page allocation, segment materialization, BSS clearing,
and relocation application already form a security-sensitive vertical slice.
Keeping the final memory map, boot-information object, `ExitBootServices()`, and
first instruction in a later branch lets loader failures remain observable
through the proven UEFI console and shutdown path.

## Governing Decisions And Authority

This branch implements accepted ADRs 0002, 0005, 0008, and 0009 and consumes the
image contract established under ADR-0016. It uses the result grammar from
ADR-0012 for the headless success marker but does not claim the later PL011 plus
semihosting transport. The exact production-loader acceptance rules are a
subordinate contract aligned with `AARCH64_BURROW_IMAGE.md`; accepted ADR text
is not edited.

ADR-0010 and ADR-0015 govern the boot-information object, but this branch does
not construct one. ADR-0011 governs the final AArch64 handoff, but none of its
register, interrupt-mask, stack, exception-level, or post-boot-services promises
are made here.

Work stops for explicit architectural discussion if implementation evidence
would require any of the following:

- accepting an ELF type, machine, byte order, load model, or relocation model
  outside the implemented Burrow image contract;
- selecting a fixed Burrow physical address rather than firmware-chosen pages;
- symbol resolution, a dynamic interpreter, a needed library, PLT binding, or a
  general dynamic linker;
- constructing page tables or changing firmware's current translation regime;
- making UEFI declarations or handles visible to the environment-neutral image
  parser;
- treating a missing or rejected Burrow image as success on the combined system
  path;
- transferring control, calling `ExitBootServices()`, or weakening the accepted
  handoff contract to make the slice appear to boot; or
- adding a new runtime or third-party dependency.

## Observable Completion

At branch completion:

- the combined system bootloader opens exactly
  `EFI/WARREN/BURROW.ELF` from the device that loaded `BOOTAA64.EFI`;
- Warren-owned production C++ validates the runtime ELF from bounded byte reads
  rather than invoking, translating, or trusting the Python artifact verifier;
- UEFI allocates one contiguous page-aligned physical extent at a
  firmware-selected address and the loader computes one checked load bias;
- every `PT_LOAD` file range is copied to its load-bias-relative destination and
  every memory-only tail is zero-filled;
- the loader accepts zero runtime relocations and correctly applies synthetic
  valid `R_AARCH64_RELATIVE` relocations in host tests;
- every rejection leaves a stable loader error and never exposes a partial image
  as a successful load result;
- a successful QEMU combined-system test reports the physical image extent,
  load bias, and relocated entry as diagnostics and emits
  `WARREN_TEST:1:PASS:burrow-loader`;
- success still ends through UEFI `ResetSystem()` while boot services and
  firmware mappings remain active; and
- no instruction from Burrow executes.

The QEMU result proves that firmware file I/O and allocation reached a validated
loaded-image state. It is not evidence of boot-information construction,
`ExitBootServices()`, instruction-cache synchronization for transfer, AArch64
entry, PL011 output, exception handling, or kernel execution.

## Deliverables

### 1. Environment-Neutral Production ELF Loader

Add a Warren-owned freestanding C++ loader core under the bootloader ownership
boundary. Its public input is a bounded random-access byte source with an exact
byte count, not an EFI file handle, host path, Python object, or unchecked native
ELF structure. The source exposes only the minimal operation needed to read an
exact byte range. Host fixtures and the UEFI file adapter exercise the same
parser and materializer.

The core uses fixed-width integer types, explicit little-endian decoding, fixed
capacity for the implemented segment set, and checked arithmetic before every
offset, size, alignment, page-count, address, and table calculation. It does not
allocate, include EDK2 or hosted C++ headers, depend on packed-structure aliasing,
or dereference image-selected addresses during validation.

Parsing produces a complete immutable load plan containing at least:

- the source image size;
- the three accepted `PT_LOAD` descriptions and their R, RX, or RW class;
- the page-rounded physical allocation span;
- the ELF-relative entry;
- the `PT_DYNAMIC` range and permitted relocation-table description; and
- enough bounded metadata to materialize without reparsing unchecked bytes.

The production parser enforces the load-time subset of the implemented image
specification, including:

- ELF64, little-endian, current-version, System V, AArch64 `ET_DYN` identity;
- exact ELF and program-header structure sizes and bounded program-header reads;
- one load span beginning at ELF-relative address zero and ending below
  `0x80000000`;
- exactly one nonempty R, RX, and RW `PT_LOAD`, each 4 KiB-compatible, mutually
  page-disjoint, file-bounded, congruent, and with `p_filesz <= p_memsz`;
- an entry wholly inside file-backed executable storage;
- one bounded `PT_DYNAMIC` wholly contained in writable loaded storage;
- a terminated dynamic table with no interpreter, needed library, text
  relocation, PLT, REL, RELR, initialization, finalization, or other unsupported
  runtime request; and
- either no runtime relocation table or one complete ELF64 Rela table whose
  entries meet the accepted AArch64-relative rules.

Section headers, debug sections, build symbols, and the named entry symbol remain
build-time artifact-verifier concerns. The production loader neither requires
nor trusts them because loading is defined by program headers and dynamic
metadata. Any deliberate difference between artifact-audit and production-load
rules is recorded in `AARCH64_BURROW_IMAGE.md` rather than left implicit.

The core returns a narrow typed status and a load result only after every stage
has succeeded. Error values are stable enough for focused tests and loader
diagnostics but are not promoted into a public kernel ABI.

### 2. Host Tests For The Actual Loader

Add C++ host tests that compile the same production loader core used by UEFI.
Fixtures are assembled from explicit bytes or a small Warren-owned builder;
they do not invoke LLD, the Python verifier, or the parser under test to define
their expected structure.

The test suite covers at least:

- the current zero-relocation runtime-image shape;
- a valid image containing one and multiple `R_AARCH64_RELATIVE` entries;
- deterministic loading at several injected physical bases;
- exact file-byte copies, zero-filled BSS and allocation gaps, relocated target
  values, entry calculation, and returned physical extent;
- short and failed source reads at the ELF header, program headers, load bytes,
  dynamic table, and relocation table;
- every identity, header-size, table-bound, alignment, congruence, permission,
  overlap, file-size, memory-size, and entry rejection;
- missing, duplicated, malformed, unterminated, or forbidden dynamic metadata;
- relocation size/count overflow, wrong entry width, wrong type, nonzero symbol
  index, negative or out-of-image addend, misaligned target, target outside
  writable storage, and duplicate target writes;
- allocation spans and physical-address computations that overflow; and
- destination buffers that are null, undersized, or incorrectly aligned.

The generated `burrow-runtime.elf` is also exercised through the production
loader during the build or test matrix. This complements rather than replaces
the independent Python artifact audit: the two implementations should agree on
the accepted runtime artifact without sharing parsing code.

### 3. Bounded UEFI File Source

Extend the curated EDK2 header snapshot only by the minimal protocol and GUID
closure required for loaded-image discovery, simple-file-system access, and
file metadata. Every added upstream header remains tied to the existing pinned
EDK2 revision, integrity manifest, license, and single Warren include boundary.
No EDK2 library or helper implementation is introduced.

Protocol GUID values are instantiated as Warren-owned local constants from the
authoritative EDK2 header macros. The bootloader does not reference EDK2's
external `gEfi*Guid` definitions, because those symbols normally come from an
EDK2 library that Warren deliberately does not link.

The UEFI adapter:

1. opens the loaded-image protocol for its own image handle;
2. opens the simple-file-system protocol on that image's device handle;
3. opens the filesystem root and the fixed absolute Burrow path;
4. obtains and validates the regular file's exact byte size;
5. implements bounded positioned reads with explicit handling for EFI errors,
   short reads, zero-progress reads, and `UINTN` conversion; and
6. closes every opened protocol/file resource on each returning path.

It does not enumerate volumes, search alternate directories, fall back to a
developer path, accept a command-line override, or silently select another
kernel. A missing file, directory in place of the file, unsupported protocol,
invalid size, read failure, and premature end of file are distinct diagnostic
stages.

UEFI status codes remain at the firmware boundary. Ordinary loader code sees
only the bounded byte-source contract, and ordinary image validation never
includes `Uefi.h` or an EDK2 protocol header.

### 4. Firmware Allocation And Image Materialization

After a complete load plan is accepted, the UEFI front end requests one
contiguous `AllocateAnyPages` extent using 4 KiB UEFI pages and a loader-owned
memory type. Because the accepted image begins at virtual address zero, the
returned physical start is the image load bias. Page-count conversion and the
entire physical extent are checked before any destination address is formed.

The shared materializer receives only the validated plan, bounded source, and
exact destination span. It:

1. initializes the complete allocation to a deterministic zero state;
2. copies each file-backed load range to `load_bias + p_vaddr`;
3. proves all memory-only segment tails remain zero;
4. walks the validated Rela table;
5. writes `load_bias + addend` only to aligned eight-byte targets wholly inside
   writable loaded storage; and
6. returns the physical extent, load bias, and `load_bias + e_entry` only after
   materialization succeeds.

No segment receives a writable-and-executable permission class. The loader
records the validated permission plan for the later Burrow page-table work, but
this branch does not pretend that firmware mappings enforce Burrow's eventual
R/RX/RW layout.

On an error after page allocation, the front end clears or invalidates the
partial result, releases the allocation, reports the failing stage, and follows
one bounded firmware failure path. On success, the loaded pages remain allocated
until firmware shutdown so the reported result describes a live image; the next
branch will transfer their ownership into boot information instead of shutting
down.

### 5. Diagnostics And Combined-System Proof

Refactor the current UEFI entry into small Warren-owned stages without adding a
formatting library, heap, global constructors, or hidden compiler runtime. Add
bounded console helpers sufficient to print fixed messages, EFI/loader status,
and 64-bit hexadecimal physical values. Diagnostics never incorporate
unbounded bytes from the ELF or filesystem.

The combined system image becomes the bootable success surface. It emits:

```text
WARREN_TEST:1:BEGIN:burrow-loader
<bounded diagnostic containing image start, size, load bias, and entry>
WARREN_TEST:1:PASS:burrow-loader
```

The host QEMU harness accepts an explicit expected test identifier so the same
protocol parser remains authoritative. The test passes only after the packaged
runtime image has been opened, validated by production C++, allocated, loaded,
zeroed, and relocated. It then requests firmware shutdown exactly as the current
pipeline does.

The focused UEFI-only ESP contains no Burrow payload and therefore is no longer
presented as a successful complete system boot. Its PE/COFF audit, deterministic
packaging, and any deliberately retained firmware-entry scaffold test remain
focused regression surfaces; the matching `system-aarch64-*` profiles own the
reference loader QEMU result. Missing Burrow must never pass on that combined
path.

System-image content identity and reproducibility tests remain in place. Debug
and release combine only matching Burrow and UEFI products, and neither product
is rebuilt with the other's compiler target.

### 6. Documentation Reconciliation

Update `AARCH64_BURROW_IMAGE.md` with the exact implemented production-reader,
load-plan, relocation-application, and UEFI allocation rules. Reconcile the
current pre-Burrow UEFI compatibility record in `TEST_RESULT_PROTOCOL_V1.md`
with the new `burrow-loader` test identifier. Update `ARCHITECTURE.md`,
`DEVELOPMENT.md`, `ROADMAP.md`, and the README wherever the boot path, commands,
artifacts, diagnostics, test identifier, or current capability changes.

Documentation must say that Burrow is loaded but not executed. It must not call
the UEFI result Burrow first light, claim that boot services have ended, imply
that boot information exists at runtime, or state that firmware mappings enforce
the later stable virtual layout.

## Implementation Order

1. Define the environment-neutral byte source, checked ELF decoder, load plan,
   typed errors, and independent host fixtures.
2. Add host materialization and relocation tests, including synthetic relative
   relocations and injected physical bases.
3. Expand and verify the minimal EDK2 header closure, then implement fixed-path
   UEFI file access and bounded diagnostics.
4. Add firmware page allocation, segment loading, zero-fill, relocation, error
   cleanup, and the successful live-image result.
5. Move the authoritative QEMU success gate to the combined system profile while
   preserving focused UEFI structural and reproducibility checks.
6. Exercise the generated debug/release runtime images through both independent
   verifiers, reconcile documentation, and run the complete clean matrix.

Implementation may expose a better local order, but firmware integration does
not precede malformed-input coverage for the production parser, and QEMU success
does not precede verification of loaded bytes and applied relocations on the
host.

## Explicit Non-Goals

- Constructing any boot-information v1 object or normalized UEFI memory map
- Allocating or reserving the bootstrap stack for handoff
- Obtaining the final UEFI memory map or implementing its retry loop
- Calling `ExitBootServices()`
- Masking interrupts or establishing the ADR-0011 register contract
- Calling or otherwise executing `burrow_aarch64_entry`
- Claiming that Burrow code, BSS consumers, or relocation-dependent globals ran
- Performing the final instruction-cache synchronization required before entry
- Installing exception vectors, reading or changing `CurrentEL`, or descending
  from EL2
- Building Burrow-owned page tables or entering the stable higher-half mapping
- Implementing PL011, semihosting, panic, or exception result transport
- Enforcing final R/RX/RW permissions through firmware mappings
- Loading another architecture, another ELF class, an initial image, kernel
  modules, shared libraries, or a general boot manifest
- Compression, signing, secure boot, measured boot, or image authentication
- Replacing the independent Python artifact verifier with production loader code
- Adding libc, libc++, an allocator framework, an EDK2 library, or another
  third-party dependency

## Verification Matrix

The branch must pass from clean build trees:

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

cmake --preset system-aarch64-debug
cmake --build --preset system-aarch64-debug
ctest --preset system-aarch64-debug

cmake --preset system-aarch64-release
cmake --build --preset system-aarch64-release
ctest --preset system-aarch64-release
```

The host suite must run the production parser and materializer, not a model of
them. The combined debug and release CTests must boot their exact generated ESPs
under the pinned QEMU machine and firmware. The current Python artifact audit,
PE/COFF audit, ESP byte-identity check, and reproducibility gates remain active.

At least host debug, focused UEFI debug/release, and combined system
debug/release are freshly configured before merge. Burrow debug/release remain
fresh whenever the image specification or linker output changes.

## Merge Gates

- Production UEFI code locates Burrow only through the loaded boot device and
  fixed `EFI/WARREN/BURROW.ELF` path.
- The actual C++ parser used by UEFI passes positive, boundary, malformed,
  short-read, and arithmetic-overflow host fixtures.
- No image-controlled read, destination write, allocation calculation,
  relocation target, addend, or entry computation occurs before its complete
  bounds and overflow checks.
- The accepted plan contains exactly the implemented R, RX, and RW load classes
  with no page overlap or writable-executable page.
- Allocation is firmware-chosen, contiguous, 4 KiB-aligned, and large enough for
  the complete page-rounded image span.
- File bytes, zero-fill, relocation writes, load bias, physical extent, and entry
  match independent host expectations at multiple physical bases.
- Zero relocations and valid `R_AARCH64_RELATIVE` relocations pass; every other
  runtime relocation form or dynamic-linking request fails closed.
- Partial allocation and file/protocol resources have explicit cleanup on every
  returning failure path; a result cannot reference freed or partial storage.
- The generated debug and release runtime images pass both the independent
  artifact verifier and production-loader acceptance.
- Combined debug and release ESPs remain byte-verified and reproducible, then
  produce `PASS:burrow-loader` under QEMU.
- No test or document claims that Burrow executed, boot services ended, a handoff
  object was created, or final segment permissions were installed.
- Burrow and UEFI remain isolated compiler environments, and no new runtime or
  third-party dependency is introduced.

## Principal Risks

### Letting Two Validators Drift

The Python artifact gate sees build sections and symbols that production loading
does not need, while the C++ loader owns allocation and relocation semantics that
the build gate does not perform. Their shared runtime-image rules must be stated
once in the subordinate specification and exercised against the same generated
artifact without sharing an implementation.

### Trusting ELF Before Bounds Are Proven

Program headers and dynamic entries contain offsets that point to more offsets.
No cast, pointer formation, page rounding, table walk, or destination write is
safe until the relevant multiplication, addition, alignment, containment, and
permission checks have completed.

### Confusing Relative And Physical Addresses

ELF virtual addresses, source file offsets, physical allocation addresses,
relocation targets, and relocated values are distinct domains. Names and helper
interfaces must keep the domains explicit; the load bias is applied exactly
where ADR-0009 requires it and never to a file offset.

### Expanding The Firmware Boundary

Filesystem access requires more UEFI protocol declarations, but it does not
justify EDK2 libraries or firmware types throughout the parser. The pinned
header closure and Warren wrapper remain narrow, reviewable, and mechanically
verified.

### Proving Only The Current Zero-Relocation Image

Today's Burrow happens to need no runtime fixups. Synthetic valid relocation
fixtures are mandatory so the implementation selected by ADR-0009 is proven
before ordinary C++ image content starts depending on it.

### Accidentally Crossing Into Handoff

Once a valid entry exists it is tempting to call it. Without final memory-map
capture, boot information, bootstrap stack ownership, interrupt masking, cache
synchronization, and `ExitBootServices()`, that call would violate ADR-0011 and
make failures less observable. This branch ends at a live loaded-image result
and firmware shutdown.

## Expected Commit Shape

The implementation should remain reviewable in approximately these coherent
steps:

1. production ELF/load-plan core and host malformed-image tests;
2. materialization, zero-fill, and relative-relocation host tests;
3. pinned UEFI filesystem declarations, fixed-path source, and diagnostics;
4. firmware allocation plus combined QEMU loader proof; and
5. specification, operator-documentation, and verification reconciliation.

Commit boundaries may move when one invariant cannot be split safely, but
unrelated cleanup and first-entry work do not join the branch.
