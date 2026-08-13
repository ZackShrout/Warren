# ADR-0010: Boot-Information Protocol V1 Principles

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 1
- **Supersedes:** None
- **Superseded by:** None

## Context

The Warren bootloader must convey firmware-derived resources without allowing
UEFI structures, compiler layout, or one machine's assumptions to become kernel
core interfaces. Variable-length data must also be validated before Burrow has a
general allocator or trusted memory subsystem.

## Decision

Boot-information protocol v1 is a versioned, contiguous, physical-memory object
with a fixed-width header followed by bounded tables and byte strings.

The AArch64 entry register `x0` contains the object's physical address. The
header contains, at minimum:

- magic, protocol major/minor, flags, and total byte size;
- kernel physical extents, load bias, and entry description;
- offsets/counts/strides for embedded tables;
- optional command-line bytes;
- optional initial-image physical location and size;
- optional early-console, framebuffer, ACPI, and device-tree descriptions; and
- reserved fields that must be zero.

Offsets are relative to the beginning of the boot-information object and locate
only contained data. External resources use explicitly named 64-bit physical
addresses and byte/page counts. No pointer, `size_t`, C++ reference, `bool`,
bit-field, native enum, or compiler-padded object crosses the boundary.

The memory map is normalized into Warren entries while retaining source
metadata. Each entry includes:

- physical start;
- page count;
- normalized Warren memory kind;
- original firmware memory type; and
- firmware attributes needed to preserve cacheability, permissions, or runtime
  reservations.

The final field layout receives a protocol-specific specification and layout
tests before implementation. Burrow validates magic, supported major version,
total bounds, offsets, strides, counts, overflow, overlaps, required flags, and
reserved zeros before consuming any table.

Protocol major changes are incompatible. A minor version may append data or add
optional capability flags without changing the meaning or location of existing
v1 fields.

## Alternatives Considered

### Raw UEFI structures

This preserves exact firmware data with minimal loader translation. It couples
Burrow to UEFI descriptor versions and prevents a non-UEFI loader from providing
the same logical contract cleanly.

### C++ object graph

Pointers and rich types would be convenient inside one build. They depend on
placement, compiler ABI, constructors, and implicit object lifetimes at the most
fragile system boundary.

### Independent allocations referenced by pointers

This avoids copying tables into one buffer. It expands the validation and
ownership problem and makes boot-information lifetime harder to reserve as one
unit.

### Fully normalized memory entries without source metadata

This maximizes firmware independence but may discard cacheability or runtime
attributes Burrow later needs for correct reservation and mapping.

## Consequences

### Benefits

- one allocation can be reserved and validated early;
- the same logical protocol can be produced by future loaders;
- malformed sizes and arithmetic can be host-tested;
- architecture and firmware details remain explicit optional sections; and
- 64-bit physical/resource descriptions do not inherit temporary backend limits.

### Costs And Risks

- the loader performs translation and copying;
- the format needs deliberate evolution rules from its first version;
- preserving source metadata adds fields; and
- optional platform descriptions can become an unstructured dumping ground if
  capability flags and ownership are not disciplined.

### Follow-Up Work

- write the exact v1 field/table specification;
- select magic and capability values;
- generate C++ and assembly-visible offsets from one canonical declaration;
- create round-trip, malformed, and version-compatibility host tests; and
- define ownership/reclamation rules once physical memory management starts.

## Revisit When

- a second loader cannot express its resources through the protocol;
- the contiguous allocation becomes impractical for real machine descriptions;
  or
- a compatible minor extension cannot represent a new optional resource cleanly.
