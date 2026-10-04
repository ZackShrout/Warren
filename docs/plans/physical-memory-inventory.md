# Physical-Memory Inventory And Boot Allocator Branch Plan

**Branch:** `codex/physical-memory-inventory`

**Status:** Complete

## Observable Result

After stable higher-half entry, Burrow converts the validated boot memory map
into a fixed-capacity inventory of allocator-eligible physical extents below the
initial 64 TiB direct-map ceiling. It subtracts page zero and the complete
transition arena, reserves one aligned four-page boot allocation, and emits one
bounded `BURROW_MEMORY_V1` record before publishing the retained kernel-entry
witness. Both inherited EL1 and EL2 routes prove the same behavior.

## Included Work

- an architecture-neutral, allocation-free physical-memory state with explicit
  extent capacity, source provenance, totals, and a publication marker;
- checked ingestion of ordered `usable` boot-map entries, including direct-map
  ceiling truncation, page-zero exclusion, transition-arena subtraction, and
  conservative preservation of source descriptor boundaries;
- a deterministic lowest-address monotonic boot allocator with power-of-two
  page alignment, checked arithmetic, and explicit accounting for requested
  pages and alignment padding;
- retained production state and the first four-page, four-page-aligned physical
  ownership reservation after stable kernel entry;
- a bounded versioned serial record containing extent count, allocatable pages,
  reserved arena, allocation address, requested pages, and remaining pages;
- host tests for construction, splitting, provenance, ceiling behavior,
  capacity, corruption, alignment, exhaustion, atomic failure, and output;
- a build-time-only physical-memory failure fixture routed through the existing
  architecture-neutral C++ failure allocation; and
- synchronized architecture, development, roadmap, protocol, and image
  documentation.

## Non-Goals

- mapping, zeroing, or otherwise dereferencing newly reserved physical pages;
- freeing, coalescing after allocation, arbitrary-address reservation, or a
  page-frame allocator;
- a kernel virtual allocator, heap, object allocation, NUMA policy, or reclaim;
- changing the loader-owned normalized map or mutating the boot-information
  object after firmware exit;
- allocating memory at or above the initial 64 TiB direct-map ceiling; and
- exposing allocator state to user space or claiming general memory management.

## Verification Matrix

- Host tests prove page zero and all 128 transition-arena pages are unavailable,
  adjacent source boundaries remain visible, exact-capacity construction works,
  and one additional extent fails without publishing partial state.
- Allocation tests prove lowest-address selection, power-of-two alignment,
  padding accounting, exact fits, exhaustion, invalid requests, corrupted-state
  rejection, and unchanged outputs on failure.
- Console tests prove an exact bounded `BURROW_MEMORY_V1` record and failure on
  invalid state or interrupted output.
- Focused Debug and Release Burrow products retain the production inventory and
  allocator but contain no fault injection or test transport.
- Debug and Release system matrices prove the live record on EL1 and EL2 and
  prove the isolated physical-memory failure fixture returns `FAIL`/81 without
  publishing memory or normalized-success records.

## Merge Gates

- Only validated `usable` pages below 64 TiB can enter the inventory.
- The transition arena and physical page zero cannot be returned.
- Initialization and allocation failures leave their public outputs empty and
  do not publish a valid state marker or retained witness.
- The allocator returns physical ownership only; no unestablished virtual alias
  is manufactured or accessed.
- Ordinary images remain free of semihosting, fault fixtures, and host tooling.
- The completed branch is merged to `main` and pushed to the configured remote.
