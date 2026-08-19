# AArch64 Normalized Entry Branch Plan

- **Status:** Ready
- **Branch:** `feature/aarch64-normalized-entry`
- **Base:** `main` after integration of `feature/burrow-first-entry`
- **Roadmap phase:** Phase 1 — First Light
- **Primary outcome:** Normalize supported EL1 and EL2 handoffs into one
  Burrow-owned EL1 environment, remove firmware identity mappings, and reach a
  bounded architecture-neutral C++ entry function.

## Why This Branch Is Next

The completed first-entry feature proves that Warren can leave UEFI boot
services, transfer through the reviewed loader boundary, and execute Burrow
assembly at either EL1 or EL2 with a validated fixed header, bootstrap stack,
and PL011 record. It deliberately leaves firmware translation state active and
does not install vectors, consume the complete boot-information object, descend
from EL2, create owned mappings, or call kernel C++.

Those omissions now form one dependency chain. Calling generic C++ before
emergency vectors, complete object validation, a controlled stack, and owned
translation state would make firmware behavior an accidental kernel ABI.
Building a general allocator, console, timer, or monitor first would depend on
the same missing execution environment. The next vertical slice is therefore
the complete architecture boundary from the proven assembly witness to a
minimal normalized C++ entry.

The pinned QEMU 11.0.3 and EDK2 profile already provides both live starting
states. On 2026-08-19, the completed first-entry system image booted at EL1 with
the ordinary `virt-11.0,gic-version=3` profile and at EL2 after adding
`virtualization=on`; both routes reached Burrow and returned the agreed result.
The normalized-entry branch makes those two profiles permanent tests rather
than substituting disassembly for live EL2 evidence.

## Governing Decisions And Authority

This branch implements ADR-0002, ADR-0007, ADR-0011, and ADR-0016. It consumes
the implemented boot-information, Burrow-image, loader-handoff, and test-result
specifications. The exact register program, table descriptors, transition
storage policy, vector behavior, and C++ entry context become one implemented
subordinate specification:

```text
docs/specifications/AARCH64_NORMALIZED_ENTRY.md
```

The specification is written and reviewed before the first system-register or
translation-table implementation lands. It records the values and ordering for
both initial exception levels rather than relying on comments scattered across
assembly.

Work stops for explicit architectural discussion if evidence would require any
of the following:

- changing the accepted 48-bit, 4 KiB, lower/upper-half layout;
- requiring an AArch64 feature beyond the ARMv8.0-A Cortex-A57 baseline;
- weakening live EL2 coverage to source inspection or a mocked state value;
- preserving a firmware mapping after normalized kernel entry;
- mapping MMIO as normal cacheable memory or making it part of the direct map;
- creating a writable alias of executable Burrow pages;
- changing the loader register contract or boot-information major version;
- treating a synchronous exception, unsupported feature, table-capacity error,
  or failed transition as successful entry;
- introducing a general allocator, C++ runtime, standard library, or third-party
  dependency to cross the boundary; or
- making architecture-neutral C++ include AArch64, UEFI, QEMU, or PL011 policy.

## Observable Completion

At branch completion, fresh Debug and Release system images prove all of the
following under pinned TCG:

- the ordinary firmware profile reaches the production Burrow entry at EL1;
- the `virtualization=on` firmware profile reaches the same production entry at
  EL2 and descends through the reviewed path to EL1;
- an emergency vector table is installed at the current EL before risky state
  changes, and a stable owned EL1 table is installed before generic C++;
- the exact environment-neutral v1 validator accepts the complete handoff object
  before any memory-map entry is used for allocation or mapping policy;
- a bounded one-shot planner selects transition pages only from validated
  `usable` memory and records every consumed page for later ownership transfer;
- Burrow builds and activates four-level, 4 KiB translation tables with the
  required image, boot-information, PL011, table-arena, and guarded-stack
  mappings;
- execution moves to the stable higher-half image, the stack and vectors move to
  their owned higher-half mappings, and live pointers are rebased deliberately;
- every TTBR0 transition alias is removed before architecture-neutral C++ runs;
- the C++ entry receives no low-half pointer and contains no architecture,
  platform, firmware, or test-transport dependency;
- both initial-EL routes emit one terminal
  `PASS:aarch64-normalized-entry` result only after the C++ witness completes;
  and
- deliberate early-vector, stable-vector, guard-page, permission, and stale-
  identity faults terminate through the expected exception result rather than
  hanging or reporting success.

The visible success diagnostic identifies initial EL, normalized EL1, owned
translation state, identity removal, and C++ arrival. It does not claim a
general console, allocator, exception dispatcher, timer, interrupt controller,
or kernel main loop.

## Transition State Contract

The subordinate specification must define one explicit state machine. These
stage names are stable test and review vocabulary even if local symbol names
change:

1. **Captured:** preserve the physical boot-information address, initial SP,
   `CurrentEL`, and DAIF without trusting external memory.
2. **Emergency vectors:** install a 2 KiB-aligned vector table at the current EL.
3. **Validated:** accept the complete boot-information object and obtain the
   bounded console and normalized memory-map views.
4. **Common EL1:** preserve the EL1 route or descend from EL2 with documented
   EL2/EL1 register state and barriers.
5. **Planned:** select transition storage and construct a complete immutable
   mapping plan without changing translation registers.
6. **Owned tables:** materialize and audit the new table hierarchy in selected
   physical pages.
7. **Activated:** switch to Burrow-owned TTBRs, MAIR, TCR, SCTLR, and associated
   TLB/barrier state while the bounded identity subset remains available.
8. **Higher half:** branch to the stable image alias, install the owned EL1
   vectors, switch to the guarded stack, and rebase surviving resources.
9. **Identity removed:** replace TTBR0 with an empty root, invalidate the
   transition aliases, and prove that no low address remains live.
10. **Kernel C++:** call the architecture-neutral entry with an immutable
    higher-half context and accept its bounded witness result.

Every terminal failure identifies the last completed stage. No failure after a
system-register transition returns to an earlier stage, UEFI, or a low alias.
Ordinary images emit a bounded direct-PL011 diagnostic when possible, mask DAIF,
and wait. Test images may additionally use the isolated result transport.

## Deliverables

### 1. Register-Level Specification And Dual-EL Test Profiles

Add `AARCH64_NORMALIZED_ENTRY.md` with exact, reviewable values and sequencing
for at least:

- `VBAR_EL1`, `VBAR_EL2`, `SPSR_EL2`, `ELR_EL2`, and `SP_EL1`;
- the supported `HCR_EL2`, `CPTR_EL2`, `CNTHCTL_EL2`, and `CNTVOFF_EL2` policy;
- `MAIR_EL1`, `TCR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, and `SCTLR_EL1`;
- physical-address width and 4 KiB granule checks from `ID_AA64MMFR0_EL1`;
- DAIF state at each stage;
- cache, TLB, `DSB`, and `ISB` ordering for MMU disable, EL2 descent, table
  activation, and TTBR0 replacement;
- the attributes and permissions assigned to every early mapping class; and
- all registers and pointers passed to architecture-neutral C++.

Extend the common QEMU harness with an explicit initial-EL profile rather than
duplicating command lines. The authoritative routes are:

```text
EL1: virt-11.0,gic-version=3,virtualization=off
EL2: virt-11.0,gic-version=3,virtualization=on
```

The harness requires the loader's observed EL and Burrow's normalized result to
match the requested route. If a future QEMU or firmware update stops exposing
one route, the test fails. A direct-entry harness is not silently substituted.
Such a fallback would need a separate reviewed plan proving that it invokes the
exact production entry with an independently constructed valid object.

### 2. Emergency And Stable Exception Vectors

Add architecture-owned AArch64 vector tables with all 16 architected slots,
correct 128-byte slot spacing, and 2 KiB alignment. The current-EL table is
installed before parsing variable-sized data or changing exception state. Until
the PL011 record has been validated, an unexpected exception may only capture a
bounded record and enter a masked wait; it must not dereference the untrusted
record to print.

Once output is safe, the emergency reporter emits a bounded line containing the
transition stage, vector class, `CurrentEL`, `ESR_ELx`, `ELR_ELx`, `FAR_ELx`, and
`SPSR_ELx`. It uses no allocation, formatting library, firmware service, or
return path. Nested exceptions enter a distinct terminal loop rather than
recursing through the reporter.

After higher-half transfer, `VBAR_EL1` names the stable upper-half vector table.
The stable synchronous path preserves the same machine-readable exception
classification while leaving general exception dispatch for a later feature.
IRQ, FIQ, and SError slots remain terminal and cannot be mistaken for handled
interrupt support.

Test-only fault injection selects exact transition stages at build time. At
minimum, live tests deliberately trap:

- at EL2 after `VBAR_EL2` installation and before descent;
- at common EL1 before owned-table activation; and
- at higher-half EL1 after stable-vector and stack installation.

Each fault requires `PANIC`/4, the expected stage diagnostic, and a matching
QEMU process status. Ordinary images contain no injection branch or semihosting
trap.

### 3. Complete Boot-Information Consumption And Transition Planning

Link the existing environment-neutral boot-information validator into Burrow
instead of reimplementing its rules in architecture assembly. The fixed-entry
checks remain the gate to a bounded declared object size; emergency vectors
contain a fault caused by a corrupted declared span. Only after the complete
validator succeeds may code use memory-map entries, feature-selected records,
or external physical ranges.

Add environment-neutral, allocation-free planning code that consumes the
validated memory map and produces immutable fixed-capacity records for:

- the Burrow segment pages and final R/RX/RW permissions;
- the boot-information pages retained read-only through C++ entry;
- the validated PL011 aperture as explicit device MMIO;
- a one-shot physical transition arena selected only from `usable` pages;
- page-table pages within that arena;
- backing pages for one owned early stack; and
- the empty lower-half root needed after identity removal.

The planner never mutates the protocol object or silently turns firmware-
reclaimable memory into usable memory. It uses typed physical addresses, virtual
addresses, byte counts, and page counts with checked arithmetic. Selected pages
are carried in the final C++ context so a later physical-memory manager cannot
rediscover them as free.

The early stack uses a fixed virtual range in ADR-0016's dynamic window, is
read-write and execute-never, and has one unmapped guard page below and above.
The branch implements no reusable allocation or free operation: it makes one
deterministic selection, or fails before changing translation state.

Host tests use independent map fixtures and cover valid fragmented layouts,
exact fits, alignment, deterministic selection, reserved-resource exclusion,
overlap, exhaustion, capacity limits, and every checked-arithmetic boundary.
The generated valid system object is also exercised through the production
consumer.

### 4. Supported EL1 And EL2 Normalization

Split the current first-entry witness into small assembly boundaries and
environment-neutral or architecture-owned C++ where stack use is safe. The EL1
route must not read or write EL2-only state. The EL2 route installs `VBAR_EL2`,
programs only the documented non-secure EL1 execution state, sets the reviewed
`SPSR_EL2` and `ELR_EL2`, and executes one `ERET` into the common EL1 label.

Both routes reach a deliberately identical common state before table
construction. The specification decides whether inherited EL1 translation is
disabled before construction or replaced in place; the implementation may not
depend on an undocumented firmware register value either way. Code and stack
addresses remain valid across the selected sequence because the inherited map
is identity-based and every address transition is checked in advance.

Disassembly verification confirms the EL-specific branches, system-register
accesses, barriers, `ERET`, common-EL label, and absence of calls across any
stack or translation interval where the procedure-call ABI is not valid.

### 5. Fixed-Capacity AArch64 Page-Table Builder

Implement a bounded builder for 48-bit, four-level, 4 KiB AArch64 tables. The
builder consumes the immutable mapping plan and caller-provided zeroed table
pages. It does not allocate, derive policy from raw UEFI types, or overwrite a
live table hierarchy.

The initial hierarchy contains only what is needed to cross the boundary:

- a TTBR0 identity subset for the executing Burrow pages, bootstrap stack,
  boot-information object, transition arena, and PL011 aperture;
- the Burrow image at virtual bias `0xFFFFFFFF80000000` with program-header
  permissions and unmapped segment gaps;
- the retained boot-information and table-arena pages at checked direct-map
  aliases;
- PL011 in the explicit MMIO window with device, privileged, read-write,
  execute-never attributes;
- the owned stack in the dynamic window with both guard pages absent; and
- no broad identity map or convenience mapping of the complete firmware map.

Normal-memory table walks are inner-shareable, write-back, write-allocate.
Image text is read-only executable; read-only data is read-only and execute-
never; writable data is read-write and execute-never. EL0 access is disabled.
No physical page has an alias that makes executable bytes writable.

Runtime mapping policy comes from linker-owned image-boundary symbols whose
values and page classes the ELF verifier cross-checks against the exact
`PT_LOAD` headers. Burrow does not trust or reparse its in-memory ELF metadata
to discover permissions during the transition.

Host tests walk the produced descriptors independently and reject noncanonical
addresses, physical-width overflow, conflicting remaps, wrong attributes,
misalignment, table exhaustion, block/page conflicts, writable-executable
requests, forbidden MMIO/direct-map overlap, and missing transition resources.
Using block descriptors is permitted only when the full block has homogeneous
ownership, attributes, and permissions; page descriptors are the conservative
default.

### 6. Owned-Table Activation And Higher-Half Transfer

Activate the completed tables only after an independent preflight confirms that
the current PC, current stack, boot object, table pages, diagnostic MMIO, target
PC, target stack, and target vectors are all represented exactly as required.
No partially built hierarchy is installed.

The activation boundary performs the specified MAIR/TCR/TTBR/SCTLR, TLB,
barrier, and instruction-synchronization sequence. It then:

1. branches to the corresponding stable higher-half image address;
2. changes `VBAR_EL1` to the stable upper-half vector address;
3. switches SP to the top of the owned guarded stack;
4. converts the boot-information, table-arena, and PL011 references to their
   deliberate upper-half aliases;
5. replaces TTBR0 with the empty lower-half root and invalidates stale entries;
6. executes an `ISB`; and
7. verifies that every surviving code, data, stack, vector, and context pointer
   is canonical and in the expected upper-half region.

No instruction after TTBR0 replacement dereferences or branches through a low
address. Disassembly and symbol checks make the low-to-high branch and the final
TTBR0 replacement directly reviewable.

Target-negative images prove at least:

- the lower and upper guard pages fault;
- a write through the executable image mapping faults;
- instruction fetch from writable data faults; and
- the former physical boot-information alias faults after identity removal.

Each expected fault is accepted only from the stable higher-half vector path.

### 7. Architecture-Neutral C++ Entry Witness

Add a C-compatible `KernelEntryContext` containing only fixed-width values and
higher-half pointers whose backing ranges have already been validated and
mapped. It records at least the boot-information view, transition-arena
reservation, initial exception level, and the fact that identity removal is
complete. The ABI has compile-time host and AArch64 layout checks.

Call an `extern "C"` C++ function under `kernel/src/Core` only after the common
state is EL1 with owned vectors, owned tables, owned guarded stack, and an empty
TTBR0. The function includes no architecture, platform, firmware, or test
header. Its initial bounded job is to validate the context, revalidate the
boot-information view through its upper-half alias, write a retained witness,
and return a narrow success or failure value to the architecture continuation.
It is intentionally not yet the permanent nonreturning kernel main loop.

The architecture continuation verifies that result, emits the normalized-entry
diagnostic through the mapped PL011, and either waits in ordinary builds or
invokes the isolated QEMU result transport in test builds. `PASS` is impossible
before the C++ function returns its exact success value.

Host tests compile and call the same C++ entry logic with valid and malformed
contexts. The Burrow verifier rejects unexpected undefined symbols, runtime
helpers, initialization arrays, architecture imports into Core, and any QEMU
transport in ordinary images.

### 8. Result Protocol, Operator Workflow, And Documentation

Move the combined system success identifier to:

```text
WARREN_TEST:1:BEGIN:aarch64-normalized-entry
WARREN_TEST:1:PASS:aarch64-normalized-entry
```

Assign the remaining test-specific failure codes deliberately in
`TEST_RESULT_PROTOCOL_V1.md`. At minimum, distinguish complete-object
validation, unsupported architectural state, EL2 descent, transition storage,
table construction, table activation, identity removal, and C++ context
failure. Architectural traps continue to use common `PANIC`/4.

Update README, architecture, development, roadmap, handoff, boot-information,
image, and test-result documentation so each states exactly:

- both initial exception levels are live-tested;
- generic C++ starts only at normalized EL1;
- firmware mappings are gone before that call;
- the early stack and page tables are owned but not generally allocatable;
- the exception reporter is terminal, not a dispatcher;
- direct PL011 remains a bounded bring-up path, not the reusable console; and
- semihosting remains test-only and absent from ordinary artifacts.

Document interactive QEMU and debugger commands for the inherited, common-EL1,
higher-half, and C++ symbols. Symbol files remain separate from packaged runtime
images.

## Implementation Order

Implement in this dependency order:

1. write the exact transition specification and make EL1/EL2 QEMU selection a
   first-class harness input;
2. install emergency vectors and prove terminal faults at inherited EL1/EL2;
3. link the full validator, add the one-shot transition planner, and exhaust its
   host tests;
4. implement and disassemble the EL2-to-EL1 normalization plus common EL1 state;
5. build and independently test the fixed-capacity page-table generator;
6. activate owned tables, transfer higher, move vectors and stack, then remove
   every identity alias;
7. add the architecture-neutral C++ context/witness and dual-EL success tests;
8. add permission, guard, stale-alias, and stage-failure target fixtures; and
9. reconcile documentation and run the complete clean matrix.

Implementation evidence may refine local boundaries, but table activation does
not precede host-verified planning, C++ does not precede identity removal, and a
single live exception level never satisfies the branch.

## Explicit Non-Goals

- A reusable physical page allocator, freeing, reclamation, or ownership merge
- A complete direct map of RAM or use of firmware-reclaimable memory
- A general virtual-address allocator or dynamic mapping API
- Heap allocation, constructors, a standard library, or compiler runtime
- A reusable early console, input, formatting, logging, panic, or assertion API
- A general exception dispatcher or recovery from unexpected exceptions
- GICv3 configuration, IRQ/FIQ handling, or the ARM generic timer
- Multiple CPUs, secondary-core release, or per-CPU storage
- EL0, processes, syscalls, scheduling, or userspace address spaces
- ACPI, device-tree, framebuffer, command-line, or initial-image consumption
- UEFI runtime services, ordinary reboot, or ordinary shutdown
- ASIDs, KASLR, LPA/LPA2, top-byte-ignore, larger granules, or high memory
- Replacing the ELF loader, boot-information major version, or system ESP format
- Treating the bounded C++ witness as a running general-purpose kernel

## Verification Matrix

The branch must pass from newly configured build trees:

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

The host suite exercises the exact full-object consumer, transition planner,
page-table builder, and C++ entry logic compiled into Burrow. The focused target
builds verify ordinary image permissions, vector layout, relocation safety, and
absence of test support. Focused UEFI tests retain loader and ESP evidence.

Combined Debug and Release tests boot both initial-EL profiles through the same
UEFI loader and production Burrow entry. Target-negative fixtures run through
the common host classifier and require exact stage diagnostics plus serial/
process agreement. CTest timeouts remain bounded; no expected fault is accepted
as a timeout.

Before merge, inspect disassembly around:

- the initial register capture and both VBAR installations;
- the EL1/EL2 branch, EL2 register program, `ERET`, and common EL1 label;
- table activation, TLB invalidation, barriers, and SCTLR changes;
- the low-to-high branch, SP/VBAR moves, and TTBR0 replacement;
- the C++ call and checked continuation; and
- every test-only semihosting trap.

## Merge Gates

- The subordinate register and mapping specification is complete and matches
  generated disassembly in Debug and Release.
- Both pinned firmware profiles prove their requested initial EL through the
  production loader and production Burrow entry.
- Emergency vectors are installed before variable-size parsing or state
  transitions, and deliberate faults at EL2, common EL1, and stable EL1 report
  exact architectural state.
- Only the existing complete v1 validator authorizes memory-map and resource use.
- Transition pages come only from validated `usable` entries, are disjoint from
  every live resource, and remain reserved in the C++ context.
- The table builder is capacity-bounded, rejects conflicting mappings, and
  produces no writable-executable page or normal-memory alias for MMIO.
- Activation preflight proves every address needed on both sides of the switch.
- The stable image, direct-map resources, MMIO, stack, and vectors occupy only
  their ADR-0016 regions with the specified permissions.
- The stack has two live guard faults, and text-write/data-execute probes fault
  through the stable vector path.
- TTBR0 is empty before generic C++, and a stale physical alias faults after the
  removal sequence.
- No low-half code, stack, vector, context, console, table, or boot-information
  pointer survives into `burrow_kernel_entry`.
- The C++ entry compiles freestanding, has a checked C ABI, uses no architecture
  or platform header, and cannot report success without executing.
- Debug and Release EL1/EL2 routes emit matching
  `PASS:aarch64-normalized-entry` serial and process results before timeout.
- Ordinary Burrow and UEFI artifacts contain no fault injection, QEMU result
  markers, semihosting argument blocks, or semihosting traps.
- Existing loader, first-entry failure, image, protocol, ESP reproducibility,
  and toolchain gates remain green.
- Documentation makes no claim of a general allocator, console, recoverable
  exceptions, interrupts, timer, scheduler, or complete kernel runtime.

## Principal Risks

### Losing The Last Safe Exception Boundary

A bad system-register value can fault before the new vector base is usable.
Vectors are installed first at each active EL, their addresses are checked under
the current mapping, and deliberate faults prove the exact route before later
state changes are enabled.

### Proving EL2 Only On Paper

Source and disassembly can prove instruction presence but not a viable firmware
handoff. The pinned `virtualization=on` profile already reaches Burrow at EL2 and
becomes a required Debug/Release boot. A future regression blocks the merge
instead of silently narrowing support.

### Switching Translation Under A Live Stack

One missing current-PC, SP, vector, table, or MMIO mapping can make the failure
unobservable. The complete plan is built and independently preflighted before
register changes; the activation and higher-half boundaries avoid calls until
the procedure-call ABI is valid again.

### Reusing Apparently Free Memory

The normalized map is immutable evidence, not a mutable allocator. A one-shot
planner may select only `usable` pages, records the exact consumed arena, and
passes that reservation forward so later memory work cannot double-allocate it.

### Creating A Permission Back Door

Correct permissions at the stable image alias are meaningless if an identity or
direct-map alias remains writable. The builder reasons by physical page, rejects
permission conflicts across aliases, excludes image pages from the direct map,
and removes TTBR0 before C++.

### Carrying A Low Pointer Higher

PC and SP can move while a saved boot, console, vector, or context pointer still
names its physical alias. Surviving references are enumerated in the
specification, rebased at one reviewed boundary, and range-checked immediately
before identity removal.

### Letting The C++ Witness Grow Sideways

The first C++ function proves the ABI and execution environment; it is not a
place to smuggle in console, allocation, timer, or platform policy. Its includes,
symbols, return contract, and host tests keep that boundary narrow.

### Letting Test Machinery Become A Kernel Service

Fault injection and semihosting are useful precisely because they are strong
test tools. Default-off build selection, exact artifact inspection, and the
existing two-channel classifier keep them out of ordinary Burrow and physical-
machine semantics.

## Expected Commit Shape

The implementation should remain reviewable in approximately these coherent
steps:

1. normalized-entry specification and dual-EL QEMU profile;
2. emergency vector tables, reporter, and inherited-state fault tests;
3. complete consumer integration, transition planner, and host tests;
4. EL2 descent and common EL1 normalization;
5. fixed-capacity page-table builder and independent host inspection;
6. owned-table activation, higher-half transfer, stack/vector move, and TTBR0
   removal;
7. architecture-neutral C++ context and live EL1/EL2 success;
8. guard, permission, stale-alias, and transition-failure fixtures; and
9. artifact, protocol, operator, roadmap, and clean-matrix finalization.

Commit boundaries may move when a machine-state invariant cannot be split
safely. Unrelated cleanup and the later console, timer, allocator, or monitor
features do not join this branch.
