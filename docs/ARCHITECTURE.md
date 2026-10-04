# Warren Architecture

**Status:** Phase 0 and Phase 1 complete; First Light boots, reports, interacts,
fails deterministically, and supports audited host-side symbolized debugging

**Primary target:** AArch64, QEMU `virt-11.0`, little-endian, one virtual CPU

**Future target:** x86-64, selected only after the shared boundaries are proven

This document describes the direction in which Warren begins. The UEFI
bootloader now loads and enters the audited Burrow ELF image through the
accepted physical handoff. Burrow's assembly witness now installs a terminal
emergency vector table at inherited EL1 or EL2. Its first freestanding C++
boundary performs complete boot-information validation and immutable transition
planning. Architecture assembly then removes inherited firmware translation
and cache state and converges both inherited EL1 and EL2 routes at one physical,
MMU-off EL1h label. The common path builds and independently audits the bounded
owned translation hierarchy, activates it, transfers to the stable image,
installs the owned vectors and guarded stack, and removes every TTBR0 identity
mapping. It constructs the fixed Core entry context, revalidates the aliased
boot object in architecture-neutral C++, and retains the successful witness.
QEMU-virt platform code selects the validated PL011 record and gives Core a
bounded polling writer over the explicit stable MMIO alias. Core publishes the
first reusable console diagnostic before the witness can succeed.
Stable decisions are
recorded in `docs/adr/`, and exact subordinate formats live in
`docs/specifications/`.

## Architectural Shape

Burrow begins as a **modular monolithic kernel**. Core services and initial
drivers share one privileged address space but are divided by explicit source
and interface boundaries. This minimizes bring-up complexity without requiring
the project to confuse physical colocation with architectural coupling.

User-facing services, Hare, and eventually Meadow run in user space. A
microkernel conversion, loadable kernel modules, and in-kernel C++ plugin ABI are
not current goals.

```text
User space
  Hare | warrend | system services | applications | Meadow
    Warren SDK libraries and C runtime
  ---------------- syscall ABI ----------------
Kernel space
  process | virtual memory | VFS | IPC | scheduler | object/handle model
  ---------------- internal interfaces ---------
  drivers | platform support | architecture support
  ---------------- hardware/firmware ------------
  QEMU virt (initially) | future physical platforms
```

## Initial Execution Environment

The reference machine is QEMU's generic AArch64 `virt` platform. The expected
development environment is an ARM Mac, but the host architecture is not part of
the target contract.

Initial machine assumptions:

| Concern | Initial choice |
| --- | --- |
| CPU architecture | AArch64 / ARMv8.0-A, Cortex-A57 reference CPU |
| Privilege | Loader may hand off at EL1 or EL2; Burrow normalizes both to EL1h |
| CPU count | One |
| Memory | 512 MiB reference configuration |
| Base page size | 4 KiB |
| Interrupt controller | GICv3; MSI/ITS initially disabled |
| Timer | ARM generic timer |
| Early console | PL011 UART |
| Firmware path | QEMU EDK2 AArch64 firmware with Warren-owned UEFI bootloader |
| Reference accelerator | TCG; optional HVF fast profile |
| Kernel object | Position-independent ELF64 `ET_DYN` |
| Data model | LP64, little-endian |

Burrow adopts a 48-bit canonical virtual-address layout after its bounded
identity-mapped transition. The upper half contains explicit direct-map, MMIO,
dynamic, reserved, guard, and stable image regions under ADR-0016. The exact
boot protocol and integrity-pinned EDK2/firmware inputs are also settled.

## Boot Architecture

The boot path has three conceptual participants:

1. Firmware establishes a standards-based machine environment.
2. Warren's bootloader loads Burrow and translates firmware details into
   a Warren boot-information structure.
3. Burrow takes ownership, validates the structure, establishes its own memory
   and exception environment, and never relies on live firmware services again.

Boot-information protocol 1.0 is one contiguous, versioned physical-memory
object with a fixed 256-byte header and bounded relative sections. Its required
normalized memory map preserves Warren ownership kinds and namespaced firmware
source metadata. The header describes Burrow's physical allocation, physical
load bias and entry, bootstrap stack, and optional initial image, ACPI, and
device-tree resources. Optional contained sections describe command-line bytes,
PL011 early-console state, and a framebuffer.

The architecture-neutral C-compatible declaration contains no pointer,
`size_t`, reference, `bool`, native enum, bit-field, or implicit padding. C and
C++ compile-time checks prove every size, alignment, and offset on the host and
bare AArch64 target. An allocation-free consumer validates independent raw-byte
fixtures according to the ordered rules in
`specifications/BOOT_INFORMATION_V1.md`.

The bootloader is freestanding C++20 and uses a curated, pinned EDK2 UEFI ABI
header snapshot behind Warren-owned wrappers. No EDK2 build system, runtime,
library, or driver participates in Warren's build. Clang and `lld-link` produce
the AArch64 PE32+ `BOOTAA64.EFI` application.

The focused UEFI product packages that application at the standard
removable-media path `EFI/BOOT/BOOTAA64.EFI` in a deterministic FAT32 image. It
contains no Burrow payload and is a structural and reproducibility surface, not
a successful system boot. The combined system image additionally packages
Burrow at `EFI/WARREN/BURROW.ELF`. Only that combined image owns the current
`aarch64-normalized-entry` QEMU result. Burrow emits the terminal serial record and
uses the test-only semihosting exit; the host requires both channels to agree.
Dedicated pass, explicit-failure, and panic Burrow children prove the transport
without adding it to the ordinary kernel image. Build-time-only Burrow children
also prove exact failures 75–83 and take real stable-vector faults for both
stack guards, text writes, data execution, and removed identity aliases.
Separate UEFI fault fixtures prove Burrow rejects finalized header and console
corruption and prove the loader's post-exit failure containment without
entering Burrow.

Loader diagnostics use the UEFI console only before the final memory-map
transaction. After successful exit, the loader and Burrow witness use bounded
direct PL011 writes under firmware's inherited identity mapping. This is not a
reusable kernel console. After normalized entry, the production driver and
device-class reader/writer provide allocation-free polling I/O through the
checked upper PL011 alias. A fixed monitor accepts only `help`, `status`, and
`exit`, with 15-byte command and eight-command session bounds. The earlier
assembly reporters remain independent so console
construction failures stay observable. The test-only semihosting path is absent
from ordinary Burrow and UEFI products and is not a physical-machine interface.

Burrow is built as a static position-independent ELF64 `ET_DYN` image with
separate read-only, executable, and writable load pages. The current minimal
image has zero runtime relocations; later images may use only the audited
`R_AARCH64_RELATIVE` subset. The symbol and runtime copies, program headers,
sections, dynamic metadata, symbols, relocation policy, and packaging rules are
specified in `specifications/AARCH64_BURROW_IMAGE.md` and enforced by an
independent byte-level host verifier.

Host-only debug tooling treats `burrow.elf` as the canonical symbol image and
the LLD map as an audited address-extent input. It normalizes captured addresses
from the ELF-relative image, the fixed `0xFFFFFFFF80000000` stable alias, or an
explicit loader-reported physical bias before invoking `llvm-symbolizer`.
Debug builds prove both assembly and C++ source resolution and generate LLDB
commands for the stable slide. Opt-in system targets expose QEMU's GDB stub only
on loopback and start the guest paused; none of this tooling or metadata enters
the packaged runtime image.

The bootloader locates the packaged ELF only through the loaded-image device and
fixed Warren path. An EFI-neutral bounded-byte reader validates the ELF64
program headers, load classes, dynamic table, and optional relative-relocation
table. Firmware chooses one contiguous page extent; the shared materializer
zeroes it, copies the three loads, applies `R_AARCH64_RELATIVE`, and reports the
physical extent, load bias, and relocated entry. The loader then allocates the
bootstrap stack and handoff storage, normalizes the final UEFI memory map,
constructs and validates boot information, exits boot services with bounded
stale-key retry, synchronizes executable bytes, and transfers with the accepted
AArch64 register and stack state. Burrow's first-entry assembly installs the
matching inherited-EL emergency vectors before variable-size parsing, checks
the fixed header prefix and PL011 record, records the observed EL and handoff
state, then continues through normalized EL1, owned translation, higher-half
transfer, identity removal, and architecture-neutral C++. The inherited
terminal reporter captures EL1/EL2 architectural state without a stack. After
the owned stack and stable EL1 table are active, the stable entry preserves all
31 GPRs, SP, and syndrome state in a fixed frame. Synchronous exceptions and
unhandled IRQs emit the versioned record through reusable console code and are
terminal. The one handled case is CPU 0 physical-timer PPI 30: QEMU-virt GICv3
dispatch disables the one-shot source, the assembly path restores the complete
frame, and `ERET` resumes the interrupted code.

## Source Layout

The layout is established before implementation so dependency rules can be
enforced from the first build:

```text
boot/
  Loader/               EFI-neutral Burrow image parser and materializer
  uefi/                 UEFI file, allocation, diagnostic, and entry stages
kernel/
  include/burrow/       kernel-owned interfaces shared across modules
  src/Core/             kernel entry, panic, logging, fundamental support
  src/Memory/           physical and virtual memory
  src/Process/          tasks, scheduler, processes, syscalls
  src/Filesystem/       VFS and kernel-facing filesystem support
  src/Ipc/              inter-process primitives
  src/Drivers/          device-class logic and concrete drivers
  src/Arch/AArch64/     CPU, exception, MMU, context-switch implementation
  src/Arch/X86_64/      future architecture implementation
  src/Platform/QemuVirt/ machine description and platform wiring
user/
  lib/                  target runtime and system libraries
  services/             warrend and system services
  hare/                 shell
  apps/                 first-party command-line applications
sdk/                    public target definitions, headers, and build support
tools/                  host-side image, protocol, test, and developer tools
tests/
  host/                 target-independent unit tests
  system/               QEMU integration and boot tests
docs/
  adr/                  architectural decision records
```

Directories appear only when their first owned artifact exists. Core,
AArch64, Drivers, and QEMU-virt Platform now contain the normalized entry and
allocation-free console implementation. Other planned directories remain
absent until their first owned artifact exists.

## Portability Layers

### Architecture support

Architecture support owns:

- exception-vector and trap entry/exit;
- CPU-local control registers and feature discovery;
- page-table encoding and TLB maintenance;
- context save/restore and context switching;
- atomics and memory-order primitives not supplied safely by the compiler;
- interrupt-enable state and low-power wait primitives; and
- architecture-specific boot normalization.

It exposes capabilities to kernel core. Kernel core must not know an AArch64
system-register name or an x86 descriptor-table layout.

### Platform support

Platform support owns:

- discovery or declaration of memory-mapped devices;
- interrupt-controller and timer selection;
- initial console selection;
- platform reset and shutdown; and
- wiring generic driver interfaces to the machine.

QEMU addresses belong here, never in a generic UART or timer interface.

### Drivers

Drivers own device mechanisms. A device-class interface describes what kernel
core needs: bytes from a console, timer deadlines, block requests, input events,
or network packets. A concrete driver does not select process policy or define a
global singleton merely because only one device exists today.

## ABI And Binary Formats

The provisional ABI strategy is:

- ELF64 for Burrow and later Warren user executables;
- PE/COFF only where required by the UEFI boot environment;
- the platform's standard procedure-call ABI: AAPCS64 on AArch64 and the System
  V AMD64 ABI on a future x86-64 target;
- a C-compatible boundary for assembly entry points and exported low-level
  interfaces;
- a Warren-defined syscall ABI independent of C++ name mangling and object
  layout; and
- DWARF debug information in development artifacts where tooling supports it.

The syscall ABI is not designed until user mode and the kernel object model have
concrete requirements. When it is designed, syscall numbers, argument widths,
error representation, restart behavior, and structure-version rules must be
specified together.

## Memory Architecture: Staged Plan

Memory management grows through explicit ownership stages:

1. **Boot allocator:** monotonic physical allocation with no freeing, usable
   before the full memory map is normalized.
2. **Physical memory manager:** page-frame ownership and reserved-region
   tracking.
3. **Kernel virtual memory:** controlled mappings, guard pages, permissions, and
   explicit MMIO mapping.
4. **Kernel heap:** allocation suitable for kernel objects; not a substitute for
   page ownership.
5. **User address spaces:** independent mappings, validated user access, and
   executable/write permission policy.
6. **Advanced VM:** demand paging, copy-on-write, mapped files, shared memory,
   and reclamation only after their policies can be tested.

Burrow initially enters through firmware's identity mapping and does not begin
as a higher-half kernel. At normalized EL1 it constructs a minimal owned
identity subset and a 48-bit upper-half layout, activates those tables, moves
execution, vectors, stack, and surviving resources to deliberate higher-half
mappings, then removes all TTBR0 identity aliases. The direct map covers only
eligible normal memory; MMIO is explicit; writable aliases of the Burrow image
are forbidden. ADR-0016 owns the complete ranges and transition gates.
Shared code speaks in distinct physical-address, virtual-address, byte-count,
and page-count types to prevent unit confusion.

## Execution And Scheduling: Staged Plan

1. A single boot execution context brings up the machine.
2. Kernel tasks validate saved contexts and cooperative switching.
3. Timer-driven preemption is introduced with explicit interrupt-context rules.
4. The first user process enters an unprivileged exception level.
5. Processes acquire address spaces and handle tables; threads become schedulable
   execution units within a process.
6. Multiprocessor scheduling is a separate future phase.

An interrupt handler performs bounded work and defers policy-heavy operations.
Blocking rules and lock ordering must be documented before multiple locks can be
held together.

## Kernel Object And Resource Model

The exact model is unresolved, but it must provide:

- explicit ownership and lifetime;
- opaque user-visible handles instead of kernel pointers;
- typed validation at syscall boundaries;
- a route to waiting on multiple asynchronous events; and
- inspectable permissions and state.

File descriptors may be one view of a broader handle model. This choice is kept
open until process, IPC, and VFS use cases can be evaluated together.

## Files And Initial Userspace

The first user program may be supplied through an initial ramdisk. A simple
read-only archive or filesystem is preferable to implementing writable storage
before process loading works.

The filesystem sequence is expected to be:

1. initial-ramdisk reader;
2. VFS naming and file-object interface;
3. a small read-only disk filesystem or deliberately selected existing format;
4. block cache and writable semantics; and
5. a Warren-native writable filesystem only if the project develops requirements
   that existing formats cannot satisfy.

Inventing a filesystem is not a prerequisite for running Hare.

## Userspace Policy

Mechanisms required for process isolation belong in Burrow. System policy that
can safely live in a process belongs in user space, including:

- service dependency and restart policy;
- device-management policy beyond minimal kernel binding;
- user sessions;
- networking configuration;
- package installation; and
- desktop behavior.

`warrend` should eventually be inspectable through ordinary tools and should not
be a hidden second kernel.

## Stability Policy

During early development:

- kernel-internal interfaces may change freely with their callers;
- boot protocol changes are versioned once more than one loader exists;
- persistent on-disk formats require explicit versioning from their first use;
- syscall compatibility begins when a userspace milestone is declared stable;
  and
- Warren SDK versioning begins when third-party source can reasonably target it.

Nothing becomes stable merely because it was committed once.

## Scheduled Architectural Decisions

The Phase 0 decision queue is empty. First Light's focused register-level
EL2/EL1 normalization sequence is implemented under ADR-0011 and proven through
both live firmware entry paths. Owned translation activation, higher-half
transfer, and identity removal are also implemented and proven through both
routes. The fixed architecture-neutral C++ boundary is also implemented and
proven. Platform-selected, allocation-free console input/output, one handled
timer interrupt, and the bounded diagnostic monitor are also implemented and
proven through both firmware routes. The retained QEMU-virt panic entry now
masks interrupts before touching the console, publishes one bounded
`BURROW_PANIC_V1` assertion or kernel-panic record, detects recursive entry
without re-entering the formatter, and ends in an architecture terminal wait.
Test-enabled images alone replace that wait with the agreed `PANIC`/2 or
`PANIC`/3 transport. The host debugger workflow now audits every generated map,
proves retained Debug symbols across all supported address aliases, and provides
paused EL1/EL2 QEMU attachment; later kernel initialization remains.

Later decisions include the syscall ABI, kernel object model, scheduler policy,
VFS semantics, libc strategy, service model, package format, graphics stack, and
first supported x86-64 platform. They should not be guessed prematurely.
