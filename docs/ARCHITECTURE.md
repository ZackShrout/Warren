# Warren Architecture

**Status:** Provisional architecture for Phase 0 planning  
**Primary target:** AArch64, QEMU `virt-11.0`, little-endian, one virtual CPU  
**Future target:** x86-64, selected only after the shared boundaries are proven

This document describes the direction in which Warren should begin. Only the
UEFI first-light bootloader scaffold currently exists; the Burrow components
below remain a plan. Stable decisions are recorded in `docs/adr/`; unresolved
questions remain labeled here instead of receiving accidental answers in code.

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
| Privilege | Loader may hand off at EL1 or EL2; Burrow normalizes to EL1 |
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

The later Burrow-owned virtual-address layout, exact boot-protocol fields, and
firmware/header provenance snapshot remain to be settled before their respective
implementation steps.

## Boot Architecture

The boot path has three conceptual participants:

1. Firmware establishes a standards-based machine environment.
2. Warren's bootloader loads Burrow and translates firmware details into
   a Warren boot-information structure.
3. Burrow takes ownership, validates the structure, establishes its own memory
   and exception environment, and never relies on live firmware services again.

The boot-information structure is one contiguous, versioned, physical-memory
object and contains no compiler-native pointers.
Its early design is expected to include:

- a magic value, size, and protocol version;
- physical memory-map entries with explicit types;
- kernel physical and virtual image extents;
- command-line bytes and length;
- initial-ramdisk location and length;
- early-console description;
- optional framebuffer description; and
- firmware/system-table data only when a later kernel service truly needs it.

All addresses declare whether they are physical or virtual. All arrays carry
element size and count. Burrow rejects incompatible versions rather than
guessing.

The bootloader is freestanding C++20 and uses a curated, pinned EDK2 UEFI ABI
header snapshot behind Warren-owned wrappers. No EDK2 build system, runtime,
library, or driver participates in Warren's build. Clang and `lld-link` produce
the AArch64 PE32+ `BOOTAA64.EFI` application.

The implemented scaffold packages that application at the standard removable
media path `EFI/BOOT/BOOTAA64.EFI` in a deterministic FAT32 image. It writes a
versioned marker through the UEFI console and requests firmware shutdown. This
is a pipeline proof, not Burrow's eventual PL011 console or semihosted Phase 1
test-result transport.

Burrow is loaded as a static position-independent ELF64 `ET_DYN` image. The
loader supports only the audited relative-relocation subset and rejects dynamic
linking. An early QEMU direct-load path is permissible for experimentation if it produces
the same handoff contract and does not leak into kernel core.

## Planned Source Layout

The layout is established before implementation so dependency rules can be
enforced from the first build:

```text
boot/
  uefi/                 Warren bootloader
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

This is a plan, not a demand to create empty directory trees. Directories appear
when their first owned artifact exists.

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

Virtual-address layout and higher-half placement remain open memory-management
decisions. Burrow initially enters through firmware's identity mapping and then
adopts Burrow-owned mappings; it does not begin as a higher-half kernel.
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

## Open Architectural Decisions

Before First Light implementation begins, Warren must decide and record:

- the exact boot-information protocol v1 field/table layout;
- the pinned EDK2 header revision and minimal header closure;
- the QEMU EDK2 firmware provenance/hash policy;
- the register-level EL2/EL1 normalization sequence; and
- the serial test-record grammar and result codes.

The higher-half virtual layout is required during the memory-management phase,
not before the first loader-to-kernel transfer. Later decisions include the
syscall ABI, kernel object model, scheduler policy, VFS semantics, libc strategy,
service model, package format, graphics stack, and first supported x86-64
platform. They should not be guessed prematurely.
