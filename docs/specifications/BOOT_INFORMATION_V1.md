# Warren Boot-Information Protocol V1

**Status:** Accepted normative specification for protocol 1.0

**Governing decisions:** ADR-0010 and ADR-0015

## 1. Purpose And Terminology

The Warren boot-information object is the complete loader-to-Burrow resource
contract. It is one contiguous physical-memory object. On AArch64, `x0` contains
its physical base address at handoff.

In this specification:

- **object base** is the physical address supplied to Burrow;
- **contained region** is data stored within the object and located by a relative
  section descriptor;
- **external resource** is separately allocated physical memory described by an
  absolute physical address and size;
- **producer** is the Warren bootloader or a compatible future loader; and
- **consumer** is Burrow's early architecture and kernel-entry code.

All offsets and sizes are byte quantities unless explicitly named as page counts.
All table offsets are relative to object base.

## 2. Encoding And Global Invariants

- Integer fields are unsigned little-endian values of their stated width.
- The object base is aligned to 8 bytes.
- `header_size`, `total_size`, and every present section offset are multiples of
  8 bytes.
- The protocol page unit is 4096 bytes in version 1.
- Physical address `0` denotes absence only for fields controlled by an absent
  optional feature. Mandatory physical resources may not begin at address `0`.
- Every reserved field and byte defined by the consumer's supported version is
  zero.
- For a protocol 1.0 producer, the bytes after `header_size` through `total_size`
  contain only the four described sections and zero-valued alignment padding.
  A later minor version may add ignorable present-only sections.
- The maximum representable object is bounded by 32-bit `total_size`; version 1
  defines no practical reason to approach that limit.
- Physical range calculations and `count * stride` calculations reject overflow
  before comparing bounds.

The magic bytes are the ASCII sequence `WARRENBI`:

```text
57 41 52 52 45 4e 42 49
```

Version 1.0 uses `major = 1` and `minor = 0`.

## 3. Core Header

The protocol 1.0 header is exactly 256 bytes and aligned to 8 bytes.

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| `0x000` | 8 | `magic` | ASCII `WARRENBI` |
| `0x008` | 2 | `major` | Incompatible protocol generation; `1` |
| `0x00A` | 2 | `minor` | Compatible extension level; `0` |
| `0x00C` | 4 | `header_size` | Bytes occupied by header; `256` for 1.0 |
| `0x010` | 4 | `total_size` | Entire contiguous object including header |
| `0x014` | 4 | `page_size` | Protocol page unit; `4096` for v1 |
| `0x018` | 8 | `present_features` | Features represented by this object |
| `0x020` | 8 | `required_features` | Present features producer requires consumer to understand |
| `0x028` | 8 | `self_physical_address` | Must equal object base supplied at handoff |
| `0x030` | 8 | `kernel_physical_start` | Start of contiguous Burrow allocation |
| `0x038` | 8 | `kernel_physical_size` | Size of Burrow allocation including segment gaps |
| `0x040` | 8 | `kernel_load_bias` | Runtime physical address minus ELF virtual address |
| `0x048` | 8 | `kernel_entry_physical_address` | Relocated executable entry address |
| `0x050` | 8 | `bootstrap_stack_physical_start` | Lowest byte of bootstrap-stack allocation |
| `0x058` | 8 | `bootstrap_stack_size` | Stack allocation size in bytes |
| `0x060` | 8 | `initial_image_physical_start` | Optional initial-image address |
| `0x068` | 8 | `initial_image_size` | Optional initial-image bytes |
| `0x070` | 8 | `acpi_rsdp_physical_address` | Optional ACPI RSDP address |
| `0x078` | 8 | `device_tree_physical_address` | Optional flattened device-tree address |
| `0x080` | 8 | `device_tree_size` | Optional flattened device-tree bytes |
| `0x088` | 16 | `memory_map` | Contained normalized-memory-entry section |
| `0x098` | 16 | `command_line` | Contained UTF-8 byte section |
| `0x0A8` | 16 | `early_console` | Contained early-console-record section |
| `0x0B8` | 16 | `framebuffer` | Contained framebuffer-record section |
| `0x0C8` | 56 | `reserved` | Zero in protocol 1.0 |

The initial stack pointer is
`bootstrap_stack_physical_start + bootstrap_stack_size`. The sum must not
overflow and must be 16-byte aligned. The stack grows toward lower addresses.

`kernel_entry_physical_address` lies inside the kernel allocation and inside an
executable loaded segment. `kernel_load_bias` is one image-wide bias satisfying
`runtime physical address = ELF virtual address + kernel_load_bias` for every
loaded segment and the entry point. Protocol 1 uses an unsigned load bias and
therefore requires each ELF virtual address to be no greater than its runtime
physical address.

Kernel and bootstrap-stack starts and sizes are nonzero multiples of the protocol
page size. The self object, kernel allocation, bootstrap-stack allocation, and
present initial-image allocation are mutually disjoint physical ranges.

## 4. Section Descriptor

Every contained section descriptor is 16 bytes:

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| `0x00` | 4 | `offset` | Relative byte offset from object base |
| `0x04` | 4 | `count` | Number of elements |
| `0x08` | 4 | `stride` | Bytes per element |
| `0x0C` | 4 | `reserved` | Must be zero |

An absent section has all four fields zero. A present section has nonzero
`count`, the exact stride required by its record type, and an aligned offset not
less than `header_size`. The half-open range
`[offset, offset + count * stride)` lies within `total_size`.

Contained sections may not overlap one another. Alignment padding between them
is zero. Version 1.0 does not require a particular section order, although the
canonical producer emits them in header-descriptor order.

## 5. Feature Bits

| Bit | Mask | Feature | Representation |
| ---: | ---: | --- | --- |
| 0 | `0x0001` | normalized memory map | `memory_map` section |
| 1 | `0x0002` | command line | `command_line` section |
| 2 | `0x0004` | initial image | header external-resource fields |
| 3 | `0x0008` | early console | `early_console` section |
| 4 | `0x0010` | framebuffer | `framebuffer` section |
| 5 | `0x0020` | ACPI RSDP | header physical-address field |
| 6 | `0x0040` | device tree | header external-resource fields |

Bits 7–63 are unassigned in protocol 1.0.

`required_features` is a subset of `present_features`. Memory-map bit 0 is set in
both masks for every valid protocol 1.0 object. A consumer rejects any required
bit it does not understand. It may ignore an unknown present bit only when that
bit is not required and the object's major/header/bounds rules remain compatible.

For each known feature, its bit and representation agree exactly. An absent
feature has a zero descriptor or zero external-resource fields. A present
feature has a valid nonzero representation.

## 6. Normalized Memory Map

The memory-map section contains 40-byte entries with stride 40:

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| `0x00` | 8 | `physical_start` | 4096-byte-aligned first physical byte |
| `0x08` | 8 | `page_count` | Nonzero number of 4096-byte pages |
| `0x10` | 4 | `memory_kind` | Warren normalized kind |
| `0x14` | 4 | `source_kind` | Namespace of retained source metadata |
| `0x18` | 4 | `source_type` | Type value in the selected source namespace |
| `0x1C` | 4 | `reserved` | Zero |
| `0x20` | 8 | `source_attributes` | Attributes in the selected source namespace |

Version 1.0 memory kinds are:

| Value | Name | Initial ownership |
| ---: | --- | --- |
| 0 | reserved | Never allocate without later platform-specific proof |
| 1 | usable | Available to Burrow after protocol/resource reservation |
| 2 | loader reclaimable | Loader-owned memory reclaimable after handoff consumption |
| 3 | boot information | This object; reserve until copied or no longer referenced |
| 4 | kernel image | Burrow loaded allocation; permanent kernel ownership |
| 5 | bootstrap stack | Reserve until Burrow switches stacks |
| 6 | initial image | Reserve until its consumer releases it |
| 7 | firmware reclaimable | Firmware boot-services memory available after successful exit |
| 8 | firmware runtime | Reserved even though Warren initially ignores runtime services |
| 9 | ACPI reclaimable | Reserve until ACPI tables are consumed/copied |
| 10 | ACPI NVS | Preserve across sleep/power semantics |
| 11 | MMIO | Device memory; never general-purpose allocation |
| 12 | persistent | Nonvolatile or persistent platform memory |
| 13 | unusable | Known bad or otherwise unsafe memory |

Entries are sorted by ascending `physical_start`, have nonoverlapping half-open
ranges, and reject address/page multiplication overflow. Adjacent entries may be
coalesced only when normalized kind, source type, and attributes all match.

The producer splits/overlays source descriptors so the exact boot-information,
kernel, bootstrap-stack, and optional initial-image physical extents have their
dedicated normalized kinds. The map covers every such live resource completely.

Source kind `0` means no retained source metadata and requires `source_type` and
`source_attributes` to be zero. Source kind `1` means UEFI and preserves the
original `EFI_MEMORY_TYPE` and attribute mask. Other source kinds are unassigned
in protocol 1.0 and require a compatible minor extension before use.

The source namespace is diagnostic and preserves mapping/reservation evidence;
it does not override `memory_kind`. Burrow allocation policy consumes the
normalized Warren kind and only interprets source attributes through a handler
for the declared source kind.

### 6.1 UEFI Producer Translation

The Warren UEFI producer translates every memory type in the pinned header
baseline as follows. It retains the original numeric UEFI type and complete
attribute mask in every resulting entry, including entries split by a live
resource overlay.

| UEFI type | Value | Warren memory kind |
| --- | ---: | --- |
| `EfiReservedMemoryType` | 0 | reserved |
| `EfiLoaderCode` | 1 | loader reclaimable |
| `EfiLoaderData` | 2 | loader reclaimable |
| `EfiBootServicesCode` | 3 | firmware reclaimable |
| `EfiBootServicesData` | 4 | firmware reclaimable |
| `EfiRuntimeServicesCode` | 5 | firmware runtime |
| `EfiRuntimeServicesData` | 6 | firmware runtime |
| `EfiConventionalMemory` | 7 | usable |
| `EfiUnusableMemory` | 8 | unusable |
| `EfiACPIReclaimMemory` | 9 | ACPI reclaimable |
| `EfiACPIMemoryNVS` | 10 | ACPI NVS |
| `EfiMemoryMappedIO` | 11 | MMIO |
| `EfiMemoryMappedIOPortSpace` | 12 | MMIO |
| `EfiPalCode` | 13 | reserved |
| `EfiPersistentMemory` | 14 | persistent |
| `EfiUnacceptedMemoryType` | 15 | reserved |

An unrecognized, OEM-reserved, or OS-reserved numeric source type causes
production to fail rather than becoming usable memory. A later platform policy
may explicitly classify such a type only after documenting its ownership and
updating producer tests. Unaccepted memory remains reserved until a future
owner implements and proves the platform's acceptance operation.

The producer overlays the complete allocated page extents for the boot-
information object, Burrow image, and bootstrap stack with their dedicated
Warren kinds. An overlay may cross source-descriptor boundaries, but it must be
fully covered without a gap and must not overlap another live resource. Splits
are deterministic, and adjacent results coalesce only when the Warren kind,
UEFI source type, and UEFI attributes all agree.

The reference loader's storage plan is boot policy rather than protocol ABI. It
reserves a zero-filled 64 KiB bootstrap stack and adds capacity for 32 UEFI
descriptors beyond the first reported map size. Page rounding of the map buffer
is included in the usable descriptor capacity. Producer work storage then
allows two additional normalized entries for each of the three live resource
overlays, and the contiguous protocol object is sized for that complete maximum
plus the reference early-console record. Every size, count, multiplication,
rounding operation, and conversion to a protocol-width field is checked before
firmware allocation.

The stack, memory-map buffer, decoded source descriptors, producer work entries,
and protocol object use separate loader-owned page allocations. They are zeroed
before use and must be pairwise disjoint. Before the first
`ExitBootServices()` attempt, partial allocation failure unwinds in reverse
order. A failed free remains recorded so cleanup can be retried or the retained
allocation can be reported before firmware-controlled termination; no such
cleanup is permitted after the first exit attempt.

The reference finalization transaction is bounded to eight storage resizes and
eight `ExitBootServices()` attempts. Each successful `GetMemoryMap()` snapshot
must use descriptor version 1, the planned descriptor stride, a whole number of
descriptors, and checked iteration bounds. The loader decodes that exact raw
snapshot into its owned source-descriptor storage, rebuilds the canonical
object, and independently validates the result before using the snapshot's map
key. No firmware callback occurs between that successful capture and the exit
attempt.

`EFI_BUFFER_TOO_SMALL` causes checked replacement map, decode, work, and object
storage to be allocated while reusing the fixed bootstrap stack, then the
transaction restarts from a new capture. Before the first exit attempt, the
superseded capacity-dependent storage is released; afterward it is deliberately
retained and becomes loader-reclaimable memory after successful exit, avoiding
a forbidden general cleanup path in the restricted phase. `EFI_INVALID_PARAMETER` from
`ExitBootServices()` is treated as a stale key: the old key is discarded, the
map and object are rebuilt, and only the new key is retried. Any other exit
failure is terminal.

## 7. Command Line

The command-line section has stride 1 and `count` equal to its byte length. Its
bytes are well-formed UTF-8 without an embedded or trailing NUL. The command line
is not implicitly whitespace-tokenized by the protocol; later Warren policy
defines argument parsing.

An empty command line is represented by the feature being absent, not a present
zero-length section.

## 8. Early Console

The early-console section contains exactly one 64-byte record with stride 64:

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| `0x00` | 4 | `kind` | Console mechanism; `1` means PL011 MMIO |
| `0x04` | 4 | `flags` | Capability flags |
| `0x08` | 8 | `physical_address` | MMIO base |
| `0x10` | 4 | `register_stride` | Byte distance between logical registers |
| `0x14` | 4 | `register_width` | Access width in bits |
| `0x18` | 8 | `input_clock_hz` | UART input clock, or zero when undisclosed |
| `0x20` | 4 | `baud_rate` | Configured/desired baud, or zero when undisclosed |
| `0x24` | 4 | `reserved_0` | Zero |
| `0x28` | 24 | `reserved_1` | Zero |

Flag bit 0 means output is supported; bit 1 means input is supported. Other flag
bits are zero in version 1.0. PL011 records require output support, a nonzero
physical address, register stride 4, and register width 32.

This record describes a diagnostic mechanism. It does not grant the generic
kernel ownership of QEMU addresses or make PL011 a universal architecture
property; platform code validates and consumes it.

## 9. Framebuffer

The framebuffer section contains exactly one 64-byte record with stride 64:

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| `0x00` | 8 | `physical_address` | First framebuffer byte |
| `0x08` | 8 | `size` | Mapped framebuffer bytes |
| `0x10` | 4 | `width` | Visible pixels per row |
| `0x14` | 4 | `height` | Visible rows |
| `0x18` | 4 | `pixels_per_scan_line` | Storage stride in pixels |
| `0x1C` | 4 | `pixel_format` | Format value below |
| `0x20` | 4 | `red_mask` | Bit mask in one pixel |
| `0x24` | 4 | `green_mask` | Bit mask in one pixel |
| `0x28` | 4 | `blue_mask` | Bit mask in one pixel |
| `0x2C` | 4 | `reserved_mask` | Remaining defined bits |
| `0x30` | 16 | `reserved` | Zero |

Pixel formats are `1` for red-green-blue-reserved 8-bit channels, `2` for
blue-green-red-reserved 8-bit channels, and `3` for explicit bit masks. A
firmware blit-only mode has no directly addressable framebuffer and is not
represented by this feature.

Width, height, size, and pixels per scan line are nonzero; scan-line pixels are
not less than width. Address/size arithmetic and the minimum required storage
for the declared geometry must not overflow or exceed `size`.

## 10. External Optional Resources

When the initial-image feature is present, address and size are both nonzero and
their sum does not overflow. The memory map covers the entire range as
`initial image`.

When ACPI is present, `acpi_rsdp_physical_address` is nonzero and aligned to 16
bytes for the RSDP structure. Its signature and checksum are
validated by the ACPI consumer before use; this protocol does not duplicate
those bytes.

When a device tree is present, address and size are nonzero and bounded. The
device-tree consumer validates the flattened-device-tree header and internal
offsets before use.

ACPI and device tree may both be present. Platform selection policy decides
which descriptions are authoritative; presence alone does not merge or prefer
one model.

## 11. Version Compatibility

A consumer implementing 1.0:

1. rejects any major other than 1;
2. accepts minor 0 and may accept a greater minor when all known header fields
   remain at their 1.0 offsets, `header_size >= 256`, and no unknown required
   feature bit is set;
3. validates the 1.0 reserved bytes through offset `0x0FF` as zero for minor 0;
4. does not require appended header bytes from a greater minor to be zero; and
5. ignores unknown present-only features by using `header_size`, `total_size`,
   and known section bounds rather than assuming all post-header bytes are known.

A minor version never reorders, resizes, or changes the meaning of an existing
field, feature bit, record field, or normalized memory kind. It may assign a
previously unassigned feature bit, append a record form selected by that bit, or
append header data by increasing `header_size`. An incompatible change requires
a new major version.

## 12. Required Validation Order

Burrow validates without allocation and without dereferencing an unchecked
external resource:

1. handoff pointer alignment and the readable fixed-prefix bound;
2. magic, major version, `header_size`, `total_size`, and page size;
3. self physical address and fixed header reserved fields;
4. feature-mask subset and supported-required-bit rules;
5. mandatory kernel and bootstrap-stack ranges and entry containment;
6. every known section descriptor's reserved value, feature consistency, exact
   stride/count rules, multiplication, alignment, and object containment;
7. pairwise nonoverlap of known contained sections and, for minor 0, zero
   alignment padding and no unaccounted post-header bytes;
8. optional external-resource field consistency and physical-range overflow;
9. memory-map ordering, kinds, page arithmetic, and nonoverlap;
10. complete memory-map coverage/kinds for the object and live external
    allocations;
11. command-line UTF-8 and record-specific console/framebuffer invariants; and
12. only then, the contents of separately validated ACPI, device-tree, or
    initial-image resources.

Failure returns a stable validation result and does not attempt to guess, repair,
or partially consume the object.

## 13. Ownership And Reclamation

At entry, Burrow owns or must preserve:

- its kernel image permanently;
- the bootstrap stack until it switches to an owned replacement;
- the entire boot-information object until all required fields are copied or its
  pages are reserved in the physical allocator;
- the initial image until its eventual owner releases it;
- firmware-runtime, ACPI NVS, MMIO, persistent, reserved, and unusable memory;
  and
- ACPI/device-tree source pages until their consumers establish longer-term
  ownership.

`loader reclaimable`, `firmware reclaimable`, and `ACPI reclaimable` do not mean
“immediately free.” Their named phase must finish before the physical allocator
may reclassify them as usable.
