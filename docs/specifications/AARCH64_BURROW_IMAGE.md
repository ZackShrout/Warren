# AArch64 Burrow Image Specification

**Status:** Implemented subordinate specification under ADR-0002, ADR-0009, and
ADR-0016

**Target:** AArch64 ELF64, little-endian, 4 KiB load pages

## 1. Purpose And Scope

This specification records the build-time, packaging, and production-loading
contract for Burrow's initial AArch64 image. It governs the ELF emitted by Clang
and LLD, the independent Warren-owned artifact verifier, the production C++
reader and materializer, the distinction between symbol and runtime copies, and
the image's location in the combined EFI System Partition.

The image format remains independent of boot-information production, the final
firmware memory map, `ExitBootServices()`, and loader-side cache synchronization.
This specification does record the build isolation required by the current
first-entry witness and its QEMU-only result transport.

## 2. ELF Identity

Every Burrow image is a complete ELF file with:

| Field | Required value |
| --- | --- |
| Magic | `0x7F` followed by `ELF` |
| Class | ELF64 |
| Byte order | Little-endian |
| Identification/header version | Current, value 1 |
| OS ABI / ABI version | System V / 0 |
| Object type | `ET_DYN` |
| Machine | `EM_AARCH64`, value 183 |
| AArch64 flags | 0 |
| ELF header size | 64 bytes |
| Program-header size | 56 bytes |
| Section-header size | 64 bytes |

The image is statically self-contained: `ET_DYN` and `PT_DYNAMIC` describe the
position-independent image, not participation in a shared-library ecosystem.
There is no `PT_INTERP`, `DT_NEEDED`, imported symbol, PLT binding, target
runtime library, or unresolved target symbol.

The entry is the C-compatible architecture symbol
`burrow_aarch64_entry`. LLD localizes its hidden definition in the final symbol
table; it remains a nonempty `STT_FUNC` at ELF entry address `0x1000`. Its
AArch64 assembly body records the incoming handoff state, checks the fixed
boot-information prefix, stack/register state, DAIF masks, current EL, and
bounded PL011 descriptor, and then enters a masked wait. It imports no runtime
and does not call architecture-neutral C++.

## 3. Load Image

The program-header table contains exactly three `PT_LOAD` entries, in ascending
virtual-address order and with these permission classes:

| Class | Flags | Purpose |
| --- | --- | --- |
| Read-only | `PF_R` | ELF/program headers, retained dynamic metadata, and read-only data |
| Executable | `PF_R | PF_X` | Architecture entry and executable Burrow code |
| Writable | `PF_R | PF_W` | Initialized data, dynamic table, and zero-filled BSS |

Each load begins on a distinct 4 KiB file and virtual page and uses
`p_align = 0x1000`. It satisfies
`p_offset % p_align == p_vaddr % p_align`. Physical and virtual addresses in the
file are equal. File size does not exceed memory size, page-rounded ranges do
not overlap, and no loaded page is both writable and executable.

The first load begins at file offset and virtual address zero and contains the
ELF and complete program-header table. Every loadable byte and the entry are
below `0x80000000`. The loader may therefore compute every physical address by
adding one chosen load bias to an ELF-relative virtual address.

The current debug and release images share this loaded shape:

| Header | Offset | Virtual address | File size | Memory size | Flags |
| --- | ---: | ---: | ---: | ---: | --- |
| `PT_LOAD` | `0x0000` | `0x0000` | `0x01FF` | `0x01FF` | R |
| `PT_LOAD` | `0x1000` | `0x1000` | `0x02B0` | `0x02B0` | RX |
| `PT_LOAD` | `0x2000` | `0x2000` | `0x0088` | `0x00D0` | RW |
| `PT_DYNAMIC` | `0x2028` | `0x2028` | `0x0060` | `0x0060` | RW |
| `PT_GNU_RELRO` | `0x2028` | `0x2028` | `0x0060` | `0x0060` | R |
| `PT_GNU_STACK` | `0x0000` | `0x0000` | `0` | `0` | RW, non-executable |

The exact current sizes are evidence for the minimal image, not reserved ABI
addresses. Later owned content may change them while preserving every bound,
permission, alignment, and overlap rule above.

## 4. Sections And Permission Evidence

The runtime copy currently retains these sections:

| Section | Type | Loaded class | Current address |
| --- | --- | --- | ---: |
| `.dynsym` | `SHT_DYNSYM` | R | `0x0190` |
| `.gnu.hash` | GNU hash | R | `0x01A8` |
| `.dynstr` | `SHT_STRTAB` | R | `0x01C4` |
| `.rodata` | `SHT_PROGBITS`, A | R | `0x01D0` |
| `.text` | `SHT_PROGBITS`, AX | RX | `0x1000` |
| `.data` | `SHT_PROGBITS`, WA | RW | `0x2000` |
| `.dynamic` | `SHT_DYNAMIC`, WA | RW | `0x2008` |
| `.bss` | `SHT_NOBITS`, WA | RW | `0x2070` |
| `.symtab`, `.strtab`, `.shstrtab` | Symbol/strings | Not loaded | no runtime address |

Allocated sections are wholly contained by a compatible load class. A
file-backed section's file offset agrees with its address relative to the load.
`SHT_NOBITS` storage begins at or after the writable load's file-backed end.
TLS, initialization/finalization arrays, constructors, destructors, exception
metadata, and PLT/interpreter sections are forbidden.

`ImageLayout.cpp` supplies explicitly named temporary sentinels in `.rodata`,
`.text`, `.data`, and `.bss`. Linker assertions and the verifier require real
content in every class. A sentinel is removed only after non-sentinel Burrow
content permanently exercises that same class in both build profiles.

## 5. Dynamic Metadata And Relocations

The current `.dynamic` table has exactly these entries:

| Tag | Current value |
| --- | ---: |
| `DT_SYMTAB` | `0x0190` |
| `DT_SYMENT` | 24 |
| `DT_STRTAB` | `0x01C4` |
| `DT_STRSZ` | 1 |
| `DT_GNU_HASH` | `0x01A8` |
| `DT_NULL` | 0 |

The current image has zero runtime relocations, which is valid. If later image
content requires runtime relocation, the complete table is described by one
`DT_RELA`/`DT_RELASZ`/`DT_RELAENT` set and one allocated `SHT_RELA`. Every entry:

- is 24-byte ELF64 `Rela`;
- has type `R_AARCH64_RELATIVE`, value 1027;
- has symbol index zero;
- writes one eight-byte value wholly inside writable loaded storage; and
- has a nonnegative addend naming loaded image storage below `0x80000000`.

`SHT_REL`, RELR, PLT relocations, text relocations, symbol-bearing relocations,
and any other AArch64 relocation type are rejected.

## 6. Production Reader And Load Plan

The production loader accepts a bounded random-access byte source containing an
exact 64-bit byte count and one exact-read callback. It includes no EFI or hosted
C++ declaration. Every multibyte field is decoded explicitly as little-endian;
the implementation does not cast file bytes to native ELF structures.

The reader accepts at most 16 program headers and produces one canonical,
fixed-capacity plan only after all validation succeeds. The plan records the
source size, the R/RX/RW segments in permission order, the page-rounded
allocation size, ELF-relative entry, writable dynamic range, and optional Rela
table. Section headers, debug sections, and symbols are deliberately not read by
production code.

In addition to the identity and load rules above, production validation
requires:

- exactly three nonempty `PT_LOAD` entries with exact R, RX, and RW flags,
  `p_align = 0x1000`, page-aligned and congruent file/virtual starts, equal
  physical and virtual addresses, page-disjoint memory and file ranges, and a
  first R load beginning at zero and containing the complete program table;
- exactly one nonempty `PT_DYNAMIC`, represented by identical file and memory
  sizes, aligned to at least eight bytes, and wholly contained by file-backed
  writable storage;
- no program type except `PT_NULL`, `PT_LOAD`, `PT_DYNAMIC`, `PT_PHDR`,
  `PT_GNU_RELRO`, and non-executable `PT_GNU_STACK`;
- a dynamic table terminated by `DT_NULL`, with duplicate tags rejected;
- only the benign `DT_HASH`, `DT_GNU_HASH`, `DT_STRTAB`, `DT_STRSZ`, `DT_SYMTAB`,
  and 24-byte `DT_SYMENT` metadata, optional zero-valued `DT_FLAGS`, and the
  complete Rela trio; and
- rejection of every interpreter, needed library, PLT, REL, RELR, text
  relocation, initialization, finalization, binding, search-path, or unknown
  dynamic request.

The relocation table must be nonempty when described, contain no more than 256
entries, use 24-byte entries, be wholly file-backed by one load, and contain
only symbol-zero `R_AARCH64_RELATIVE`. Targets are unique, eight-byte aligned,
and wholly inside writable storage. Addends are nonnegative and name bytes in
one of the three loaded memory ranges.

The loader reports narrow stable error values for identity, header, segment,
dynamic, relocation, source-read, destination, load-bias, and physical-overflow
failures. It never returns a partial plan or loaded-image result as success.

## 7. Allocation And Materialization

After planning succeeds, UEFI requests one `AllocateAnyPages` extent using
`EfiLoaderData`. The page count is derived from the checked page-rounded span;
the firmware-selected physical start is the load bias because the ELF-relative
image begins at zero. No fixed physical address is selected.

The shared materializer requires an exact 4 KiB-aligned destination span and an
aligned physical base. It zeroes the complete allocation, copies every
file-backed load to its virtual-address-relative offset, and then writes each
relocated value as `load_bias + addend` with explicit little-endian bytes. It
returns the physical start and size, load bias, and `load_bias + e_entry` only
after all reads and writes succeed. The permission classes are retained in the
plan for later page-table work; firmware mappings do not enforce the final
R/RX/RW policy in this slice.

The UEFI boundary opens the loaded-image protocol on its own image handle, opens
the simple filesystem on that image's device, and opens only
`\\EFI\\WARREN\\BURROW.ELF`. It obtains regular-file size through `EFI_FILE_INFO`
and implements positioned exact reads with explicit bounds, EFI error,
short-progress, zero-progress, and `UINTN` handling. All files and protocols are
closed on every returning path. A failure after allocation clears the partial
extent and frees its pages. A successful extent remains allocated until the
firmware shutdown used by this branch.

The bootloader supplies its own minimal `memcpy` and `memset` definitions for
compiler-emitted aggregate operations and otherwise links with no imports. No
EDK2 library, allocator, formatting library, or hosted runtime participates.

## 8. Symbols And Forbidden Runtime Surface

At least one symbol table remains so the verifier can prove the architecture
entry definition. Symbol table entry zero is null. No non-null symbol is
undefined, common, or extended-indexed. `burrow_aarch64_entry` is retained as a
nonzero-sized function whose value equals the ELF entry and whose complete range
lies in executable `.text`.

The audit rejects compiler, C++, allocation, stack-check, unwind, atomic, and
host-runtime symbol families that Warren does not own. This includes C++ ABI
machinery, global constructor support, allocation operators, unwinding,
compiler arithmetic helpers, libc allocation and memory routines, and stack
protector helpers.

## 9. Build Artifacts

Each `aarch64-debug` and `aarch64-release` build emits:

```text
artifacts/burrow.elf          symbol-bearing inspection and debugger image
artifacts/burrow-runtime.elf  debug-stripped image selected for packaging
artifacts/burrow.map          human-reviewable LLD map
```

Debug compilation uses DWARF 5 with repository-relative paths. The debug symbol
image retains non-allocated `.debug_*` sections; the runtime copy rejects debug,
comment, and note metadata. Release may naturally contain no debug sections.
Both copies retain the entry, symbol surface required for audit, program
headers, dynamic metadata, and relocations.

The runtime-copy check allows only the ELF header fields that locate and count
the non-loaded section-header table to change during stripping. The program
header table and every other byte in every `PT_LOAD` must remain identical.
Disassembly is opt-in through the `BurrowDisassembly` target and is not a
generated source artifact.

## 10. Host Verification

`tools/verify_burrow_image.py` reads little-endian structures directly from
bytes using only the Python standard library. Every header, table, segment,
section, string, symbol, dynamic entry, and relocation read is bounded before
use. The tool is an artifact gate and is not production loader code.

Generated Python fixtures cover the build-time artifact surface, including
sections and symbols. Independent C++ fixtures exercise the actual production
reader and materializer with zero, one, and multiple relocations, injected
physical bases, exact copies, BSS and gap zeroing, malformed headers, segment
and dynamic metadata, relocation type/symbol/target/addend errors, duplicate
targets, failed reads, destination errors, and arithmetic overflow. The system
profiles also pass their generated runtime ELF through the production loader on
the host before QEMU boots the same packaged bytes.

## 11. Combined ESP Packaging

The system profiles build Burrow and UEFI as isolated child products:

```text
Burrow compiler target:  aarch64-none-elf
UEFI compiler target:    aarch64-pc-windows-msvc
```

The host-only orchestration layer composes their verified bytes at:

```text
EFI/BOOT/BOOTAA64.EFI
EFI/WARREN/BURROW.ELF
```

The ESP builder accepts each input explicitly, rejects missing or repeated
Burrow inputs, and normalizes every staged file and directory timestamp to
2000-01-01 00:00:00 UTC. Tests extract both files and require byte identity with
the selected inputs, build the combined image twice and require identical
SHA-256 bytes, exercise the generated ELF through production C++, and boot the
same ESP through UEFI.

The combined QEMU path emits `BEGIN:burrow-first-entry`, the live loaded-image
diagnostic, a direct post-`ExitBootServices()` loader line, Burrow's observed EL
and boot-information address, and `PASS:burrow-first-entry`. Burrow then uses
the exact test-only `SYS_EXIT_EXTENDED` operation with status zero. The host
requires serial/process agreement.

`WARREN_ENABLE_QEMU_TEST_RESULT` defaults off and is enabled only by combined
system-test orchestration. Its private `WARREN_QEMU_TEST_RESULT_MODE` selection
is one of `pass`, `fail`, or `panic`; each mode chooses one exact terminal line
and matching status block. The transport object lives under
`kernel/src/Platform/QemuVirt` and is not compiled into ordinary Burrow.

Focused debug and release artifacts must contain no semihosting HLT, terminal
test marker, exit argument block, or branch from first entry into the transport.
The byte-level and disassembly artifact verifier enforces absence in ordinary
products and the exact selected shape in every system-test child.
