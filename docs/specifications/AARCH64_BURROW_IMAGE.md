# AArch64 Burrow Image Specification

**Status:** Implemented subordinate specification under ADR-0002, ADR-0009, and
ADR-0016

**Target:** AArch64 ELF64, little-endian, 4 KiB load pages

## 1. Purpose And Scope

This specification records the build-time and packaging contract for Burrow's
initial AArch64 image. It governs the ELF emitted by Clang and LLD, the
Warren-owned host verifier, the distinction between symbol and runtime copies,
and the image's location in the combined EFI System Partition.

It does not specify the production UEFI ELF parser, physical allocation,
loading, zero-filling, relocation application, boot information, or transfer of
control. The current bootloader does not open or execute the image.

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
table; it remains a four-byte `STT_FUNC` at the ELF entry. The current emitted
entry address is `0x1000`. This branch deliberately implements the body as
`brk #0`, and no passing test claims that the instruction executed.

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
| `PT_LOAD` | `0x0000` | `0x0000` | `0x01E0` | `0x01E0` | R |
| `PT_LOAD` | `0x1000` | `0x1000` | `0x0018` | `0x0018` | RX |
| `PT_LOAD` | `0x2000` | `0x2000` | `0x0068` | `0x00B0` | RW |
| `PT_DYNAMIC` | `0x2008` | `0x2008` | `0x0060` | `0x0060` | RW |
| `PT_GNU_RELRO` | `0x2008` | `0x2008` | `0x0060` | `0x0060` | R |
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

## 6. Symbols And Forbidden Runtime Surface

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

## 7. Build Artifacts

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

## 8. Host Verification

`tools/verify_burrow_image.py` reads little-endian structures directly from
bytes using only the Python standard library. Every header, table, segment,
section, string, symbol, dynamic entry, and relocation read is bounded before
use. The tool is an artifact gate and is not production loader code.

Generated host fixtures cover a valid relative-relocation image, a valid
zero-relocation image, truncation, arithmetic overflow, identity, entry, bounds,
alignment, overlap, permissions, dynamic metadata, forbidden sections and
symbols, relocation type/target/addend, and runtime-copy differences. The audit
runs as a build dependency for both symbol and runtime images.

## 9. Combined ESP Packaging

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
SHA-256 bytes, and boot it through the unchanged UEFI smoke path.

That QEMU pass proves only that packaging Burrow did not regress the existing
firmware pipeline. It is not evidence that the bootloader parsed, loaded,
relocated, or entered Burrow.
