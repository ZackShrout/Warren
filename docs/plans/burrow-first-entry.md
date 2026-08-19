# Burrow First Entry Branch Plan

- **Status:** Complete — verified 2026-08-19
- **Branch:** `feature/burrow-first-entry`
- **Base:** `main` after integration of `feature/burrow-loader`
- **Roadmap phase:** Phase 1 — First Light
- **Primary outcome:** Construct boot-information protocol v1, leave UEFI boot
  services under the accepted handoff contract, and prove execution reached
  Burrow's AArch64 assembly entry through post-firmware diagnostics and the
  test-only QEMU result transport.

## Completion Record

The completed branch passes the documented bootstrap, host, focused AArch64,
focused UEFI, and combined system matrix from newly configured build trees.
The host suite contains 12 tests, each focused UEFI suite contains one
reproducibility test, and each combined Debug/Release system suite contains nine
tests, including live success, malformed-header rejection, malformed-console
rejection, loader post-exit containment, and transport failure/panic fixtures.
Artifact verification proves the Burrow and UEFI test transports are absent
from ordinary Debug and Release products.

The pinned UEFI/QEMU profile hands off at EL1, so live execution proves the EL1
path. The accepted EL2 path and classification remain covered by assembly and
disassembly verification but are not claimed as a live EL2 boot. Burrow assembly
executes under inherited firmware identity mappings; kernel C++ does not run,
no exception vector is installed, and no reusable console exists yet.

## Why This Branch Followed The Loader Slice

The completed Burrow-loader slice leaves one validated, relocated Burrow image
in a live firmware allocation and reports its physical extent, load bias, and
entry. The accepted boot-information layout and consumer validator already
exist, and ADR-0011 defines the exact machine state the loader must provide.
The remaining gap between a loaded image and the first Burrow instruction is
therefore bounded enough to prove as one vertical slice.

This branch owns that boundary and stops immediately on Burrow's side of it. It
adds the final memory map, bootstrap stack, boot-information object,
instruction-cache synchronization, `ExitBootServices()` retry discipline,
register setup, and a narrow assembly-entry witness. It does not combine those
loader responsibilities with exception recovery, EL2 normalization, owned page
tables, higher-half transfer, or architecture-neutral C++ entry. Those stateful
steps remain the focused `feature/aarch64-normalized-entry` branch.

The proof remains headless. UEFI console output is useful before the exit, but
cannot prove that boot services ended or that Burrow executed. A minimal
post-exit PL011 diagnostic and the test-only semihosting result defined by
ADR-0012 provide that evidence without being presented as the eventual console,
panic system, or ordinary shutdown path.

## Governing Decisions And Authority

This branch implements accepted ADRs 0005, 0007, 0008, 0010, 0011, 0012, and
0015. It consumes the image and transition constraints from ADRs 0009 and 0016
without beginning the owned-virtual-memory transition. The normative wire and
test-result contracts remain `BOOT_INFORMATION_V1.md` and
`TEST_RESULT_PROTOCOL_V1.md`; accepted ADR text is not edited.

The exact UEFI-memory-type translation, final-map acquisition algorithm,
bootstrap-stack size, cache-maintenance routine, and first-entry witness are
subordinate implementation contracts. They must be documented and tested, but
they may not silently change the accepted boot-information fields, handoff
registers, machine state, or result protocol.

Work stops for explicit architectural discussion if implementation evidence
would require any of the following:

- passing a UEFI structure, pointer, handle, memory descriptor, or service table
  to Burrow;
- changing the boot-information v1 layout, feature semantics, memory kinds, or
  validation order;
- omitting source memory metadata or treating firmware-runtime memory as
  reclaimable because Warren does not call runtime services;
- calling a firmware boot or runtime service after successful
  `ExitBootServices()`;
- transferring with a register, stack, interrupt-mask, exception-level, or
  translation state outside ADR-0011;
- disabling the inherited MMU or caches in the loader;
- installing exception vectors, normalizing EL2, constructing page tables, or
  entering the stable higher-half image in this branch;
- calling architecture-neutral kernel C++ merely to make the entry witness
  richer;
- hard-coding a QEMU device address in generic architecture or kernel-core
  code;
- compiling semihosting into an ordinary production or interactive Burrow
  artifact;
- treating a UEFI pass marker, clean firmware shutdown, serial line alone, or
  QEMU process status alone as proof of first entry; or
- adding a new runtime or third-party dependency.

## Observable Completion

At branch completion:

- the bootloader allocates and describes one page-aligned bootstrap stack and
  one contiguous boot-information protocol 1.0 object;
- the object contains the loaded Burrow extent, load bias, physical entry,
  stack extent, a normalized final UEFI memory map, and the reference PL011
  early-console record;
- the normalized map is sorted, nonoverlapping, retains UEFI type and attribute
  metadata, and overlays the exact Burrow, stack, and boot-information
  allocations with their dedicated Warren kinds;
- the same allocation-free validator already used by host tests accepts the
  completed object before the exit attempt;
- the bootloader obtains a final map key only after every file is closed and
  every persistent handoff allocation exists, retries stale-map exits without
  using a stale object, and makes no firmware call after a successful exit;
- executable Burrow bytes receive the required AArch64 data/instruction-cache
  synchronization before transfer;
- the loader masks DAIF, supplies `x0`, clears `x1` through `x3`, switches to the
  16-byte-aligned bootstrap-stack top, preserves firmware's identity-mapped
  MMU/cache environment, and branches to the relocated entry;
- Burrow's architecture assembly preserves `x0`, records `CurrentEL`, checks the
  directly observable handoff invariants, and emits a post-exit first-entry
  diagnostic without allocating or calling architecture-neutral C++;
- the authoritative combined QEMU test emits
  `WARREN_TEST:1:BEGIN:burrow-first-entry`, then emits
  `WARREN_TEST:1:PASS:burrow-first-entry` from Burrow and terminates QEMU through
  `SYS_EXIT_EXTENDED` with status zero; and
- focused non-test Burrow artifacts contain neither the semihosting trap nor
  the test-result implementation.

The result proves construction of the accepted loader object, successful
termination of UEFI boot services, the ADR-0011 transfer state that entry can
directly inspect, and execution of Burrow assembly under the inherited identity
mapping. It is not proof of complete consumer-side boot-information validation,
emergency exception handling, EL1 normalization, an owned transition stack,
Burrow page tables, higher-half execution, generic kernel entry, or a reusable
console.

## Deliverables

### 1. Environment-Neutral Boot-Information Producer

Add a Warren-owned freestanding producer beside the existing protocol
declarations and validator. Its inputs are fixed-width descriptions of the
loaded image, bootstrap stack, destination object, source memory descriptors,
and optional records. Its output storage and capacity are supplied explicitly;
the producer neither allocates nor includes UEFI headers.

The producer performs checked size, count, stride, alignment, page, range, and
physical-address arithmetic before writing a complete object. It returns a
narrow typed result and exposes no partially valid object on failure. The
canonical protocol 1.0 result for the reference handoff contains:

- the exact `WARRENBI` magic, v1.0 header and page-size values;
- the object's own physical address and exact bounded `total_size`;
- the loaded Burrow physical start, allocation size, unsigned load bias, and
  relocated executable entry;
- a nonzero page-multiple bootstrap-stack start and size whose checked top is
  16-byte aligned;
- the required normalized memory-map section;
- one present PL011 early-console record describing the reference machine's
  first UART as output-capable, 32-bit registers with stride four; and
- zero descriptors, fields, feature bits, reserved fields, and padding for the
  absent command line, initial image, framebuffer, ACPI RSDP, and device tree.

The memory-map bit is present and required. The early-console bit is present but
not required: it describes a mechanism this reference proof consumes without
making PL011 understanding mandatory for every compatible protocol v1 consumer.
Producer tests lock those exact masks.

The producer translates a source map into Warren entries in one deterministic
pass plus bounded overlay processing. The implemented UEFI mapping policy is
recorded beside the producer and reconciled with `BOOT_INFORMATION_V1.md`. It
must account explicitly for every UEFI memory type available in the pinned
header baseline, preserve the original numeric type and attributes under source
kind UEFI, and fail closed or reserve an unknown type rather than classifying it
as usable. In particular:

- conventional memory becomes `usable`;
- loader-owned memory becomes `loader reclaimable` except where a live Warren
  resource overlay assigns a stronger kind;
- boot-services code and data become `firmware reclaimable` only after the
  successful exit represented by this handoff;
- runtime-services code and data remain `firmware runtime`;
- ACPI reclaim, ACPI NVS, persistent, unusable, MMIO, and reserved classes keep
  their corresponding non-usable Warren meanings; and
- the Burrow allocation, bootstrap stack, and complete allocated
  boot-information extent split underlying descriptors as necessary and become
  `kernel image`, `bootstrap stack`, and `boot information` respectively.

Output entries are 4 KiB-aligned, sorted by physical start, nonoverlapping, and
coalesced only when normalized kind and every retained source field agree. Each
live overlay must be wholly covered by the source map, mutually disjoint, and
represented by its exact page extent. The producer does not infer availability
from gaps or repair malformed, overlapping, misaligned, overflowing, or
unrepresentable source descriptors.

### 2. Producer And Translation Host Tests

Compile the actual producer and existing consumer validator into one focused
host suite. Independent fixtures describe source maps and expected wire bytes;
they do not call the producer to calculate expected offsets, kinds, splits, or
feature masks.

Positive coverage includes at least:

- one simple map and one map requiring every live allocation to split a source
  descriptor at both ends;
- live allocations already aligned with descriptor boundaries;
- adjacent entries that may and may not be coalesced;
- every pinned UEFI memory type and representative attribute combinations;
- exact header fields, section offset/count/stride, zero padding, canonical
  section order, and object byte size;
- the reference early-console record;
- several physical placements for the object, image, and stack; and
- round trips in which the independent v1 validator accepts the exact produced
  bytes and physical address.

Negative and boundary coverage includes zero and overflowing ranges, page-count
conversion overflow, unsorted or overlapping source maps, source gaps beneath a
live resource, overlapping overlays, insufficient output or work capacity,
unrepresentable descriptor counts, unknown source types, invalid optional
record combinations, misaligned object/stack/image ranges, an invalid entry,
and every failure point at which a partial result must remain unusable.

Tests keep producer validation and consumer validation conceptually separate.
A producer bug must not be hidden because both sides share an expected-value
helper, and successful producer validation does not remove Burrow's later duty
to validate an object supplied by another loader.

### 3. Bootstrap Stack And Final UEFI Memory Map

Extend the narrow EDK2 header snapshot only if the pinned `GetMemoryMap()` and
`ExitBootServices()` declarations are not already present in the existing
closure. No EDK2 library implementation or generic memory-map helper is added.

The bootloader allocates a dedicated initial bootstrap stack as loader-owned
pages. The initial implementation uses a documented fixed 64 KiB extent unless
implementation evidence requires a reviewed change. The size is boot policy,
not a new public ABI promise. The allocation is zeroed, its physical range and
top are checked, and it remains live through transfer.

Final-map preparation occurs only after Burrow has been materialized and its
file closed. The bootloader obtains a size estimate, allocates a map buffer with
checked growth room, calculates worst-case normalized entries after resource
splits, and allocates enough pages for the contiguous v1 object. Any scratch
storage that must remain valid until exit is itself present in the final source
map. No variable-length data is placed on the firmware stack.

Map acquisition and exit use an explicit bounded state machine:

1. finish every persistent allocation and close every file or protocol resource
   that the existing loader owns;
2. call `GetMemoryMap()` into owned storage and validate descriptor size,
   version, byte count, iteration bounds, and map key;
3. translate that exact snapshot, overlay live Warren resources, construct the
   v1 object, and run the shared validator against its exact readable size and
   physical address;
4. call `ExitBootServices()` with the key from that same snapshot without an
   intervening console write, allocation, free, protocol operation, or other
   boot-service call;
5. once any `ExitBootServices()` call returns, enter a restricted retry phase:
   firmware may already be partially shut down, so the loader may use only UEFI
   memory-allocation services needed to reacquire or resize the map plus another
   `ExitBootServices()` attempt; it may not use console, protocol, event, image,
   watchdog, or other boot services, and it may not use runtime services;
6. on the UEFI-defined stale-key result, reacquire the map, rebuild and
   revalidate the object, and retry from the new key;
7. if either buffer is too small, resize it with checked growth and restart
   rather than truncating data, while respecting the restricted phase after the
   first exit attempt; and
8. before the first exit attempt, report bounded exhaustion or another terminal
   error through the existing firmware failure path; after the first attempt,
   report only through the direct post-firmware PL011/test path when safe, then
   terminate through test semihosting or enter a masked wait loop.

The bootloader never retries using an old key or an object built from a
different map. It never logs through UEFI between final capture and exit. Once
`ExitBootServices()` succeeds, the code path is `[[noreturn]]`, firmware pointers
are dead, cleanup and `ResetSystem()` are forbidden, and failures use only the
minimal post-firmware platform/test facilities or a masked wait loop.
An unsuccessful first exit attempt is also a one-way diagnostic boundary: even
though the loader may perform the narrowly permitted memory-map retry work, it
never returns to the ordinary UEFI console, cleanup, protocol, or reset path.

### 4. Cache Synchronization And Loader Handoff Stub

Add a small AArch64 loader-owned assembly boundary for operations that cannot be
expressed as ordinary UEFI C++ calls. Before transfer it synchronizes every
materialized executable instruction range according to the ARMv8.0-A cache-line
sizes reported by architectural state, with reviewed clean, invalidate, DSB,
and ISB ordering. Arithmetic over the maintained range is checked before the
routine is invoked. The operation does not disable caches, change translation
tables, or claim the permission work reserved for Burrow-owned mappings.

The nonreturning post-exit handoff path:

1. retains the boot-information physical address, relocated entry, checked
   stack top, and reference diagnostic description without consulting firmware;
2. reads the current exception level for the required loader diagnostic;
3. emits a fixed bounded PL011 line proving that `ExitBootServices()` returned
   successfully and recording whether entry is at EL1 or EL2;
4. masks all DAIF classes immediately before handoff;
5. sets `sp` to the exact declared bootstrap-stack top;
6. places the boot-information physical address in `x0` and zeroes `x1`, `x2`,
   and `x3`; and
7. branches, rather than calls, to the relocated Burrow entry.

It does not promise or preserve `x18`, floating/vector state, caller-saved
registers, or a return address. It does not modify the inherited little-endian,
non-secure EL1/EL2, identity-mapped MMU/cache environment. Static/disassembly
checks confirm the final sequence and that no compiler-generated epilogue or
firmware call can follow the branch.

The post-exit PL011 primitive is owned by reference-platform support, not by
generic boot protocol or AArch64 policy. It is deliberately one-way and
allocation-free. This branch does not expose input, formatting, global logging,
locking, interrupt-driven I/O, or a permanent console interface.

### 5. Burrow AArch64 First-Entry Witness

Replace the deliberate `brk` marker with the smallest reviewed assembly entry
that can prove the accepted boundary without beginning normalization. Before it
reuses incoming registers, the entry preserves `x0`, captures `CurrentEL`, and
records those values in Burrow-owned writable image storage without allocation.

The witness checks only invariants that are safe and necessary under the trusted
loader handoff, including:

- `x0` is nonzero and eight-byte aligned;
- `x1`, `x2`, and `x3` arrived as zero;
- `sp` is 16-byte aligned and equals the checked top described by the direct
  bootstrap-stack header fields;
- every DAIF mask bit is set;
- `CurrentEL` reports EL1 or EL2; non-secure state remains an inherited handoff
  promise that `CurrentEL` does not itself distinguish;
- the fixed v1 header prefix has the accepted magic, major version, header and
  page sizes, self physical address, and consistent image/entry fields; and
- the early-console descriptor and record are bounded within `total_size` and
  identify the supported reference PL011 output mechanism before it is used.

This is intentionally not the complete ordered consumer validation from
`BOOT_INFORMATION_V1.md`. The entry does not walk the normalized map, trust an
optional external resource, or reinterpret producer validation as consumer
validation. Complete allocation-free consumption belongs after the next branch
has emergency vectors and a normalized execution environment.

On the test-enabled QEMU image, the witness emits one bounded diagnostic with
the observed EL and boot-information address, then emits the terminal
`burrow-first-entry` result. Success uses `PASS` and semihosting status zero.
Distinct contract-check failures use documented test-specific `FAIL` codes.
An impossible test-support invariant or unexpected return from semihosting uses
the protocol panic/infrastructure path when it can still report safely.
The ordinary non-test image contains no result transport; until the normalized
entry branch gives it a next stage, its successful witness ends in a masked wait
loop rather than claiming an ordinary shutdown.

No path executes `brk`, deliberately raises an exception, installs `VBAR_EL1`
or `VBAR_EL2`, changes `SPSR_EL2`, `HCR_EL2`, `SCTLR_EL1`, `TCR_EL1`, `MAIR_EL1`,
`TTBR0_EL1`, or `TTBR1_EL1`, or calls architecture-neutral C++.

### 6. Test-Only QEMU Result Transport And Build Isolation

Implement the exact ADR-0012 AArch64 `SYS_EXIT_EXTENDED` operation under
`kernel/src/Platform/QemuVirt` test support. Its naturally aligned two-word
argument block contains `ADP_Stopped_ApplicationExit` and the matching Warren
result code and remains readable through the `HLT #0xF000` trap. The PL011
terminal line is completely emitted before the trap.

Test transport is guarded by an explicit build option that defaults off. The
combined system-test orchestration opts its Burrow child build in and launches
trusted images with native TCG semihosting enabled. Focused `aarch64-debug` and
`aarch64-release` products remain ordinary non-test artifacts. A Release-mode
system test is still a test-enabled artifact with Release optimization; its
name does not make it distributable production output.

The authoritative system CTest becomes `WarrenSystemBurrowFirstEntry` and
expects `burrow-first-entry`. The host harness requires agreement between the
terminal PL011 record and QEMU process status. It retains its timeout and the
existing precedence among explicit failure, panic, protocol error, QEMU process
failure, and hang.

Transport-focused QEMU fixtures prove at least one matching pass, explicit
failure, and panic result through the exact target trap and host classifier.
They are separate trusted test images or deliberately selected build variants,
never production behavior. Existing host parser coverage continues to own
malformed records, channel disagreement, process errors, and timeout
classification.

Build or artifact inspection proves the semihosting implementation, trap, test
strings, and result block are absent from focused non-test debug and release
runtime images. No compile definition may make semihosting available through a
public SDK header, syscall, generic kernel shutdown, or ordinary platform API.

### 7. Documentation Reconciliation

Update `BOOT_INFORMATION_V1.md` only where the implemented UEFI-producer mapping
or canonical reference object needs subordinate clarification. Update
`TEST_RESULT_PROTOCOL_V1.md` from the temporary `burrow-loader` compatibility
record to the first real two-channel `burrow-first-entry` result. Reconcile
`ARCHITECTURE.md`, `DEVELOPMENT.md`, `ROADMAP.md`, and the README wherever the
boot path, artifacts, build options, QEMU command, test names, or current proof
changes.

Documentation must distinguish three output eras:

- UEFI console diagnostics before final map capture;
- minimal direct PL011 diagnostics after successful exit; and
- the later reusable Burrow console that this branch does not implement.

It must also say that Burrow assembly executed but kernel C++ did not, firmware
identity mappings remain active, no exception vector is installed, and the
test-only semihosting artifact is not a production or physical-machine image.

## Implementation Order

1. Define the environment-neutral producer, exact UEFI translation table,
   resource-overlay algorithm, typed errors, and independent host fixtures.
2. Add bootstrap-stack and handoff-object sizing/allocation, then exercise
   generated objects through the existing validator.
3. Implement final `GetMemoryMap()` capture, capacity growth, stale-key retry,
   and `ExitBootServices()` state transitions while preserving the current
   pre-exit failure path.
4. Add instruction-cache synchronization and the reviewed nonreturning loader
   handoff assembly.
5. Replace Burrow's trap marker with the minimal register/header witness and
   one-way QEMU-virt diagnostic path.
6. Add opt-in PL011 plus semihosting result transport, pass/fail/panic target
   proofs, and non-test artifact exclusion checks.
7. Move the combined debug/release system gate to `burrow-first-entry`,
   reconcile documentation, and run the complete clean matrix.

Implementation may expose a safer local order, but an exit attempt does not
precede producer host coverage, a branch does not precede cache synchronization,
and semihosting is not enabled before build isolation and artifact-exclusion
tests exist.

## Explicit Non-Goals

- Complete consumer-side validation or consumption of the normalized memory map
- Installing emergency EL1 or EL2 exception vectors
- Deliberately testing an architectural exception before vectors exist
- Normalizing EL2 state or descending to EL1
- Replacing the firmware stack with a Burrow-allocated guarded stack
- Constructing or activating Burrow-owned translation tables
- Entering the stable higher-half Burrow image or removing identity mappings
- Enforcing final R/RX/RW permissions or eliminating writable executable aliases
- Calling architecture-neutral kernel C++
- Implementing a general early console, input, formatting, logging, panic, or
  assertion subsystem
- Using GICv3, the ARM generic timer, interrupts, multiple CPUs, or an allocator
- Discovering or consuming ACPI, a device tree, framebuffer, command line, or
  initial image
- UEFI runtime services, runtime virtual mappings, ordinary reboot, or ordinary
  shutdown after the exit
- Supporting HVF as an authoritative result path or semihosting physical
  hardware
- Loading another kernel format, architecture, module, or application
- Adding libc, libc++, an EDK2 library, or another third-party dependency

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

The host suite must exercise the exact producer and memory-map translation used
by UEFI. The focused Burrow builds must verify the ordinary non-test artifacts.
The combined debug and release CTests must boot their exact generated test ESPs
under pinned TCG, firmware, and machine settings with native semihosting enabled.
Existing ELF, PE/COFF, loader, boot-information, protocol, ESP byte-identity,
and reproducibility gates remain active.

At least host debug, focused AArch64 debug/release, focused UEFI debug/release,
and combined system debug/release are freshly configured before merge. The
Burrow disassembly is inspected around cache maintenance, loader transfer,
first entry, and the test-only trap. Negative target result artifacts are built
and run through the same host harness as the success path.

## Merge Gates

- The canonical producer emits bytes accepted by the independent v1 validator
  and fails closed on capacity, arithmetic, map, resource, and feature errors.
- Every pinned UEFI type has a documented Warren mapping; unknown or unsupported
  values never become usable memory.
- The final normalized map retains source type and attributes, is ordered and
  nonoverlapping, and completely overlays the boot-information, Burrow, and
  bootstrap-stack page extents with their dedicated kinds.
- The handoff object, map buffer, stack, and loaded image remain live and
  disjoint through transfer; every pre-exit failure has explicit cleanup or a
  documented retained-allocation shutdown path.
- Every `ExitBootServices()` call uses the key and object built from the same
  final snapshot, with no intervening firmware operation and no stale-key reuse.
- After the first `ExitBootServices()` attempt, no path reaches UEFI console,
  protocol, event, image, watchdog, runtime-reset, cleanup, or general failure
  helpers; only permitted memory-map retry services and another exit attempt
  remain reachable before direct PL011/test failure or a masked wait loop.
- No firmware service or firmware console call is reachable after a successful
  exit.
- Executable loaded bytes receive the reviewed ARMv8.0-A cache synchronization
  before the final branch.
- Disassembly confirms DAIF masking, exact stack switch, `x0` handoff,
  `x1`–`x3` clearing, and a nonreturning branch to the relocated entry.
- Burrow assembly records the accepted EL and validates every directly claimed
  entry invariant without installing vectors, changing EL/MMU state, or calling
  generic C++.
- The post-exit diagnostic and terminal result originate without UEFI, and the
  host observes matching PL011 `PASS:burrow-first-entry` and semihosting status
  zero before timeout in debug and release system tests.
- Exact target pass, failure, and panic transport cases agree across serial and
  process status.
- Focused non-test debug and release Burrow artifacts contain no semihosting trap
  or test-result implementation.
- No test or document claims normalized EL1, owned vectors, owned mappings,
  higher-half entry, complete boot-information consumption, or a reusable
  console.
- Burrow, UEFI, and host code retain isolated compiler environments, and no new
  runtime or third-party dependency is introduced.

## Principal Risks

### Invalidating The Final Map Key

Firmware allocations, frees, protocol activity, and even diagnostics may change
the memory map. The final snapshot, produced object, validator result, and exit
key must be treated as one transaction. No convenience logging belongs between
capture and `ExitBootServices()`.

### Under-Sizing A Map That Can Grow

The size probe, buffer allocation, boot-information allocation, and exit retry
can themselves expose more descriptors. Capacity planning needs checked growth
room and a bounded restart path. Truncating descriptors or allocating during the
final transaction is never acceptable.

### Misclassifying Firmware Memory

A normalized map is an ownership decision, not a cosmetic rename. Runtime,
MMIO, ACPI, persistent, unusable, and unknown types must remain unavailable
until their later owners prove otherwise. Resource overlays must split entries
without losing the underlying UEFI metadata.

### Crossing The Firmware Boundary Twice

After successful exit, a familiar console or reset helper becomes a dangling
firmware dependency. The post-exit path must be visibly separate, nonreturning,
and inspectable so no error path can wander back into UEFI.

The first *attempt* is also a one-way boundary for ordinary firmware services.
UEFI permits firmware to partially shut boot services down before returning a
stale-map error. Retry code may reacquire the map, resize through memory-
allocation services when required, rebuild the object, and call
`ExitBootServices()` again, but it must not fall back to the existing UEFI
console, protocol cleanup, runtime reset, or general failure helpers.

### Executing Newly Written Instructions

The loader materializes code as data. Emulator coherence can hide a missing or
incorrect cache-maintenance sequence that fails on physical hardware. The
routine must follow the architectural cache-line discovery and barrier rules,
and its exact instruction sequence must be reviewable in disassembly.

### Mistaking A Marker For Normalized Entry

Reading a trusted fixed header and printing from assembly is intentionally much
smaller than safe general kernel entry. Without emergency vectors, EL
normalization, owned mappings, and full consumer validation, the witness must
not grow into ordinary kernel code.

### Letting Test Transport Escape

Semihosting gives a trusted guest host-side powers Warren does not expose as an
OS interface. A default-off option, platform-specific ownership, target tests,
and negative artifact inspection all enforce that the trap is test machinery
rather than a production shutdown dependency.

## Expected Commit Shape

The implementation should remain reviewable in approximately these coherent
steps:

1. boot-information producer, UEFI translation policy, and host tests;
2. bootstrap-stack allocation plus final-map and exit state machine;
3. AArch64 cache synchronization and loader handoff assembly;
4. Burrow first-entry witness and minimal QEMU-virt diagnostic;
5. isolated PL011/semihosting transport and target result tests; and
6. build, specification, operator-documentation, and verification
   reconciliation.

Commit boundaries may move when one invariant cannot be split safely, but
unrelated cleanup and normalized-entry work do not join the branch.
