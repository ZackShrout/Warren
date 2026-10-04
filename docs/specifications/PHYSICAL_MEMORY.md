# Burrow Physical-Memory Inventory And Boot Allocator

**Status:** Implemented Phase 2 subordinate contract under ADR-0010 and
ADR-0016

## 1. Scope

This contract owns Burrow's first post-handoff physical-memory state. It turns
the already validated boot-information memory map into a fixed-capacity list of
allocator-eligible extents and provides a monotonic physical boot allocator.
It does not establish mappings, dereference returned addresses, free pages, or
provide the general page-frame allocator.

All arithmetic is in 4 KiB pages or checked 64-bit byte addresses. The current
managed ceiling is the initial AArch64 direct-map physical limit,
`0x0000400000000000` (64 TiB).

## 2. Inventory Construction

`initialize_physical_memory()` accepts only a published
`validated_boot_information_t` view and the exact transition-arena range from
the stable kernel-entry context. It scans the ordered normalized map without
modifying the boot-information object.

Only entries whose normalized kind is `usable` are eligible. Construction:

1. excludes physical page zero;
2. truncates ranges at the 64 TiB managed ceiling and accounts higher pages as
   unmanaged;
3. subtracts the complete transition arena and rejects an arena not wholly
   covered by usable memory;
4. retains source kind, firmware type, and source attributes on every result;
5. preserves source-entry boundaries rather than coalescing them; and
6. publishes the state magic only after the whole inventory succeeds.

The inventory contains at most 64 nonempty, ascending, nonoverlapping extents.
Capacity exhaustion, malformed ordering or arithmetic, missing usable memory,
and invalid input leave the caller's state cleared and unpublished.

## 3. Boot Allocation

`allocate_boot_pages()` accepts a nonzero page count and a nonzero power-of-two
alignment measured in pages. It examines extents in ascending address order and
returns the lowest allocation that satisfies the request. Allocation advances
only that extent's monotonic cursor; alignment padding is consumed and
accounted separately from requested pages.

There is no freeing or coalescing. A successful result is physical ownership
only. Callers may not manufacture or access a virtual alias until the virtual-
memory subsystem establishes one. Invalid state, invalid alignment, and
exhaustion clear the allocation result and do not modify allocator state.

The initial live consumer reserves four pages aligned to four pages. It retains
the inventory and allocation in kernel writable storage for the next Phase 2
memory slice.

## 4. Diagnostic Record

After the reusable console-ready line and before the retained C++ witness,
Burrow emits exactly one bounded record:

```text
BURROW_MEMORY_V1:extents=<decimal>:pages=<decimal>:arena=0x<16 uppercase hex>:arena_pages=<decimal>:boot_alloc=0x<16 uppercase hex>:boot_pages=<decimal>:remaining=<decimal>\r\n
```

`pages` is the initial managed allocatable count after exclusions. `remaining`
subtracts allocation pages and alignment padding consumed so far. The live
record must report all 128 transition-arena pages and the four-page initial
allocation. Output failure prevents witness publication.

## 5. Verification

Host tests cover splitting, provenance, page-zero and arena exclusion, ceiling
truncation, exact capacity, atomic failure, alignment and padding, exhaustion,
state corruption, and exact output. Debug and Release QEMU system matrices
require the record for inherited EL1 and EL2. A test-only `physical-memory`
fixture returns `FAIL`/81 before the memory or normalized-success records;
focused production images contain neither that fixture nor test transport.
