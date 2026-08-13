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

## Phase 0 — Foundation

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
- hash-pinned, 25-header EDK2 ABI snapshot behind one Warren wrapper;
- Clang/`lld-link` AArch64 UEFI application build;
- deterministic FAT32 ESP construction; and
- bounded QEMU UEFI smoke boot in debug and release profiles.

Exit gates:

- every pre-implementation open decision in `ARCHITECTURE.md` has an accepted ADR;
- a new contributor can identify the first milestone and its non-goals;
- planned dependencies are pin-able and obtainable on an ARM Mac; and
- the First Light test protocol has explicit success and failure signals.

## Phase 1 — First Light

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
