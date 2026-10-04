# Warren Capability Roadmap

Warren's roadmap is ordered by dependency and evidence, not calendar dates.
Only one primary phase is active at a time. A phase exits when all of its gates
are reproducible from a clean checkout using documented commands.

The horizons are intentionally uneven. Early phases are concrete because their
requirements are visible. Distant phases describe direction and prerequisites;
they are expected to change as Warren teaches us what it needs.

## Continuous Tracks

These tracks advance alongside every applicable phase:

- **Documentation:** decisions, interfaces, failure behavior, and operator notes
- **Verification:** host tests, emulator tests, assertions, and regressions
- **Portability:** dependency checks and absence of target assumptions in shared code
- **Toolchain:** reproducible host tools and visible compiler-runtime dependencies
- **Security:** stated trust boundaries and validation of untrusted input
- **Self-hosting:** reduction and documentation of host-only assumptions

## Near-Term Branch Runway

This runway records dependency order, not a promise that later branch boundaries
will remain unchanged. Only the next branch receives a committed detailed plan.
The runway is reassessed after every merge.

1. **Complete — `feature/burrow-loader`: load without handoff.** The UEFI bootloader
   locates, validates, allocates, loads, zeroes, and applies the permitted
   `R_AARCH64_RELATIVE` relocations to Burrow. It reports the verified entry and
   load bias, then retains the proven firmware-controlled completion path. The
   complete plan is in
   [`plans/burrow-loader.md`](plans/burrow-loader.md).
2. **Complete — `feature/burrow-first-entry`: transfer execution.** The loader
   constructs the accepted boot-information object from the final UEFI memory
   map, calls `ExitBootServices()`, establishes the handoff contract, and enters
   Burrow's architecture assembly. Minimal platform diagnostics, failure
   injection, and test-only result transport provide unambiguous proof without
   pretending the full console or normalized kernel entry is complete. The
   completed plan is in
   [`plans/burrow-first-entry.md`](plans/burrow-first-entry.md).
3. **Complete — `feature/aarch64-normalized-entry`: reach kernel C++ at EL1.**
   Both live UEFI entry routes now converge on owned EL1 translation, stable
   vectors, a guarded stack, an empty TTBR0, and the fixed-ABI
   architecture-neutral C++ witness. The completed plan is in
   [`plans/aarch64-normalized-entry.md`](plans/aarch64-normalized-entry.md).
4. **Complete — `feature/pl011-console`: allocation-free diagnostic output.**
   QEMU-virt platform code consumes the validated early-console record, selects
   the explicit stable PL011 mapping, and gives Core a bounded, allocation-free
   byte writer. Both EL1 and EL2 routes prove C++ emits the console-ready
   diagnostic before publishing the retained entry witness. The completed plan
   is in [`plans/pl011-console.md`](plans/pl011-console.md).
5. **Complete — `feature/aarch64-exception-reporting`: complete stable traps.**
   The owned EL1 vector table now captures a fixed versioned frame containing
   all GPRs, SP, stage, and EL1 syndrome state, then emits a bounded
   `BURROW_EXCEPTION_V1` record through the reusable PL011 driver. The
   completed plan is in
   [`plans/aarch64-exception-reporting.md`](plans/aarch64-exception-reporting.md).
6. **Complete — `codex/aarch64-generic-timer`: survive one timer IRQ.** CPU 0
   configures the QEMU-virt GICv3 and non-secure physical timer, handles one
   PPI 30 through the complete stable frame, restores all GPRs, and returns
   with `ERET` before publishing a bounded timer diagnostic. The completed
   plan is in [`plans/aarch64-generic-timer.md`](plans/aarch64-generic-timer.md).

The completed `foundation/phase-0-contracts`, `feature/burrow-image`,
`feature/burrow-loader`, `feature/burrow-first-entry`,
`feature/aarch64-normalized-entry`, `feature/pl011-console`,
`feature/aarch64-exception-reporting`, and `codex/aarch64-generic-timer` plans
remain available in [`plans/phase-0-contracts.md`](plans/phase-0-contracts.md),
[`plans/burrow-image.md`](plans/burrow-image.md),
[`plans/burrow-loader.md`](plans/burrow-loader.md),
[`plans/burrow-first-entry.md`](plans/burrow-first-entry.md),
[`plans/aarch64-normalized-entry.md`](plans/aarch64-normalized-entry.md),
[`plans/pl011-console.md`](plans/pl011-console.md),
[`plans/aarch64-exception-reporting.md`](plans/aarch64-exception-reporting.md), and
[`plans/aarch64-generic-timer.md`](plans/aarch64-generic-timer.md).
The implemented subordinate contracts are in
[`specifications/AARCH64_BURROW_IMAGE.md`](specifications/AARCH64_BURROW_IMAGE.md),
[`specifications/AARCH64_NORMALIZED_ENTRY.md`](specifications/AARCH64_NORMALIZED_ENTRY.md),
[`specifications/AARCH64_LOADER_HANDOFF.md`](specifications/AARCH64_LOADER_HANDOFF.md),
[`specifications/BOOT_INFORMATION_V1.md`](specifications/BOOT_INFORMATION_V1.md),
[`specifications/PL011_CONSOLE.md`](specifications/PL011_CONSOLE.md),
and
[`specifications/TEST_RESULT_PROTOCOL_V1.md`](specifications/TEST_RESULT_PROTOCOL_V1.md).

The next likely slice is the tiny allocation-free diagnostic monitor. Its exact
command and transport boundary will be chosen from the completed console,
exception, and timer foundation.

## Phase 0 — Foundation

**Status:** Complete

**Objective:** Agree on how Warren will make decisions and prove progress before
creating architectural momentum in code.

Planned work:

- establish the vision, guard rails, coding standards, and ADR process;
- maintain the selected toolchain, build system, and emulator bootstrap;
- specify the exact boot-information v1 field layout and firmware/header provenance;
- define planned source ownership and dependency boundaries;
- design one-command build, run, and timeout-based smoke-test interfaces; and
- record the virtual-address and initial image-layout decisions.

Completed foundation evidence:

- reproducible macOS bootstrap with a compiled AArch64 probe;
- machine-readable tool compatibility ranges, a last-known-good tuple, and
  rejection tests for unsupported or internally mismatched tools;
- hash-pinned, 28-header EDK2 ABI snapshot behind one Warren wrapper;
- Clang/`lld-link` AArch64 UEFI application build;
- deterministic FAT32 ESP construction;
- bounded QEMU UEFI smoke boot in debug and release profiles;
- accepted boot-information 1.0 byte layout with C/C++ host and AArch64 ABI
  checks plus independent valid and malformed fixtures;
- accepted 48-bit AArch64 virtual layout with a bounded identity-to-higher-half
  transition contract; and
- accepted serial/semihosting result grammar with a tested host classifier.

Exit gates:

- every pre-implementation open decision in `ARCHITECTURE.md` has an accepted ADR;
- a new contributor can identify the first milestone and its non-goals;
- planned dependencies are pin-able and obtainable on an ARM Mac; and
- the First Light test protocol has explicit success and failure signals.

## Phase 1 — First Light

**Status:** Active

**Objective:** Boot Burrow reproducibly and make early failure observable.

Capabilities:

- AArch64 startup and defined stack;
- validated boot-information handoff;
- allocation-free PL011 serial output;
- exception vectors and synchronous exception report;
- ARM generic timer interrupt;
- panic and assertion paths;
- a tiny allocation-free diagnostic monitor;
- linker map and symbolized debug workflow; and
- QEMU smoke test with timeout and machine-readable completion.

Completed Phase 1 evidence:

- audited AArch64 ELF64 `ET_DYN` symbol and runtime images in debug and release;
- page-separated read-only, executable, writable, and zero-filled content;
- independent generated malformed-ELF fixtures and build-time image audit;
- deterministic combined ESP packaging with byte-identical Burrow and UEFI
  inputs;
- isolated Burrow/UEFI compiler environments with debug and release system
  profiles;
- an environment-neutral production ELF reader and materializer with malformed,
  zero-fill, physical-base, and synthetic relative-relocation host coverage;
- fixed-path UEFI file access on the bootloader's own device plus
  firmware-selected contiguous page allocation;
- the earlier loader-only QEMU proof of a live physical extent, load bias, and
  relocated entry before transfer;
- an environment-neutral final-map producer, normalized UEFI memory mapping,
  resource overlays, bootstrap stack, and independently validated protocol 1.0
  handoff object;
- bounded `ExitBootServices()` retry discipline, reviewed AArch64 cache
  synchronization, and a nonreturning register/stack transfer boundary;
- debug and release QEMU proof that Burrow accepts EL1 and EL2 firmware entry,
  normalizes both routes to owned EL1 translation, removes identity mappings,
  reaches its generic C++ witness, and reports an agreed two-channel
  `aarch64-normalized-entry` result; and
- target failure fixtures for malformed finalized header and console data,
  loader post-exit containment, inherited and common/stable vector traps, every
  normalized-entry failure code from 75 through 82, both stack guards, image
  permissions, the removed identity alias, and isolated pass/fail/panic
  transport, with test machinery excluded from ordinary images; and
- a platform-selected, allocation-free PL011 writer with bounded transmit
  polling, host driver and failure tests, and live EL1/EL2 C++ output evidence;
- fixed Device-nGnRnE GICv3 distributor and CPU 0 redistributor mappings,
  bounded GIC bring-up, a validated 100 Hz physical-timer interval, full-frame
  IRQ return, and live EL1/EL2 proof of exactly one PPI 30 tick.

Exit demonstration:

> A clean debug build boots in the pinned QEMU machine, prints its validated
> memory map, deliberately survives a handled timer interrupt, reports a test
> exception correctly, accepts a diagnostic monitor command, and returns an
> unambiguous test result to the host.

Not included: heap, processes, filesystem, graphics, physical hardware.

## Phase 2 — Burrow Core

**Objective:** Establish trustworthy ownership of memory and interrupt-driven
kernel execution.

Capabilities:

- normalized physical memory map;
- boot allocator and physical page-frame allocator;
- kernel page tables with permission and guard-page tests;
- explicit MMIO mappings;
- kernel heap with misuse diagnostics;
- interrupt dispatch and deferred-work foundation;
- basic kernel task contexts; and
- architecture-neutral host tests for allocators and containers.

Exit demonstration:

> Burrow rebuilds its own mappings, detects allocator corruption in a negative
> test, schedules multiple kernel test tasks, and runs an extended QEMU stress
> test without leaking page ownership.

## Phase 3 — The User Boundary

**Objective:** Run isolated unprivileged code through a documented syscall path.

Capabilities:

- user address spaces;
- ELF user-image loading from an embedded initial image;
- exception return to AArch64 user mode;
- syscall entry, validation, error convention, and tracing;
- process and thread identities;
- opaque handles or the selected resource-object model;
- safe user-memory copy operations; and
- process exit and fault isolation.

Exit demonstration:

> Two isolated user programs write through the syscall ABI, cannot read each
> other's private pages, and can fault or exit without crashing Burrow.

## Portability Checkpoint — x86-64 First Light

After Phase 3, and before architecture-neutral kernel surface area grows much
further, Warren validates its boundaries with a narrow x86-64 port.

This checkpoint implements only enough x86-64 platform support to boot, log,
handle exceptions and a timer, manage core memory, and run the same minimal user
program. It does not require feature parity or begin simultaneous development of
two primary targets.

The checkpoint may cause shared interfaces to change. That is its purpose.

## Phase 4 — Files, Processes, And Hare Seed

**Objective:** Become a small but genuinely usable text-mode system.

Capabilities:

- process creation and waiting;
- standard input/output/error conventions;
- an initial ramdisk and VFS foundation;
- paths, directories, and file handles;
- a text console and input path;
- essential system calls for a small C runtime; and
- Hare Seed: an intentionally tiny interactive command runner.

Exit demonstration:

> Warren reaches a prompt, enumerates files, starts multiple programs by path,
> connects their standard streams, reports their exit status, and recovers from
> a crashing child process.

Hare Seed is not yet a rich scripting language or polished shell.

## Phase 5 — Native Development Seed

**Objective:** Compile and run a small Warren program from within Warren.

Capabilities:

- writable storage suitable for a source/build workspace;
- a documented Warren target ABI and SDK sysroot;
- enough libc and POSIX-shaped interfaces for selected tools;
- native file utilities, editor path, assembler/linker or equivalent driver;
- a deliberately small native C compiler; and
- a build-description path that works both from macOS and Warren.

Exit demonstration:

> On Warren, edit a multi-file C program, compile and link it against Warren SDK,
> run it, observe a failing diagnostic, correct it, and rebuild it without host
> intervention.

This is native development, not full self-hosting.

## Phase 6 — System Services And Packages

**Objective:** Turn a booted collection of programs into a manageable system.

Candidate capabilities:

- `warrend` service description, dependency, supervision, and logging;
- system shutdown and restart coordination;
- user/session foundation;
- timekeeping and persistent configuration;
- package archive, metadata, verification, install, and removal transactions;
- Warren SDK packaging and target discovery; and
- system inspection commands.

Exit criteria will be written when Phase 5 exposes concrete service and package
requirements. Forage's package model and command-line contract are selected no
earlier than those requirements.

## Phase 7 — Networking

**Objective:** Provide a diagnosable network stack and ordinary user-space
network services.

Likely sequence:

- emulated network device driver;
- Ethernet, ARP/NDP, IP, ICMP, UDP, then TCP;
- socket-shaped or deliberately selected user API;
- DHCP and DNS user services; and
- native package retrieval only after transport security has a credible plan.

Networking is not required to prove early kernel fundamentals.

## Phase 8 — Graphical Substrate

**Objective:** Establish graphics and input as user-space system facilities, not
a kernel-directed desktop.

Prerequisites include stable user processes, IPC, shared memory, event waiting,
service supervision, device/input paths, and text-mode recovery.

Likely capabilities:

- framebuffer or virtual GPU path;
- input event service;
- shared surfaces and damage tracking;
- user-space compositor;
- font and 2D rendering libraries;
- window/application protocol; and
- graphical diagnostics with a text-console fallback.

## Phase 9 — Meadow

**Objective:** Build a coherent native Warren desktop and application model.

Potential work includes sessions, panels, launcher, file management, terminal,
settings, accessibility, notifications, clipboard, toolkit, and application
packaging integration. These are product questions, not kernel features.

A polished, performant terminal is an intended early Meadow integration target.
It exercises processes, pseudo-terminals, Unicode and font rendering, input,
clipboard, resizing, scheduling, IPC, and compositor behavior without making the
desktop a prerequisite for system recovery or development.

Meadow receives detailed milestones only when the graphical substrate is real.

## Phase 10 — Deep Self-Hosting And Release Engineering

**Objective:** Make Warren a credible environment for producing Warren.

The horizon includes:

- native C++ compiler suitable for the selected Warren C++ subset;
- native build, archive, link, debug, test, image, and package tools;
- source acquisition and dependency verification;
- building userland and meaningful Burrow components on Warren;
- reproducible or explainably non-reproducible release images;
- bootstrap provenance documentation; and
- recovery media and upgrade/rollback strategy.

“Warren builds Warren” will be defined by an exact artifact graph, not a slogan.

## Physical Hardware Horizon

Physical hardware is selected based on documentation quality, device simplicity,
availability, and similarity to the proven emulated contracts. “ARM64” alone is
not a hardware platform. A board port requires named firmware, interrupt,
timer, storage, input, display, and debug paths.

The physical-hardware goal is credible support for named modern systems, not an
unsupported claim that Warren runs on modern hardware in general. Breadth grows
only from maintained drivers and repeatable hardware tests.

Apple Silicon remains a possible distant platform, not the assumed destination
of the initial AArch64 work.

## Versioning Guidance

Before useful userspace, milestone names communicate more than arbitrary version
numbers. When numbered releases begin:

- `0.x` denotes an intentionally unstable developer system;
- release notes state supported machine targets and capabilities exactly;
- no release claims general UEFI or architecture support from one QEMU machine;
  and
- compatibility promises are scoped to named ABIs and formats.
