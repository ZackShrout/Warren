# ADR-0015: Boot-Information Protocol V1 Layout

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0–1
- **Supersedes:** None
- **Superseded by:** None

## Context

ADR-0010 defines boot-information protocol v1 as one versioned, contiguous
physical-memory object with a fixed-width header, bounded contained tables and
strings, and explicit descriptions of external physical resources. It leaves
the exact byte layout open.

The representation must make Burrow's mandatory entry invariants directly
available before allocation, trusted virtual memory, or general parsing exists.
It must also allow optional platform information and compatible minor extensions
without embedding UEFI structures or native compiler types.

Three layout families are practical:

- one large fixed structure containing permanent fields for every anticipated
  resource;
- a fixed core header plus explicit descriptors for typed variable sections;
  or
- a generic directory of type-length-value records.

## Decision

Boot-information protocol v1 uses a **fixed core header with explicit typed
section descriptors**. The normative representation and validation rules are in
`docs/specifications/BOOT_INFORMATION_V1.md`.

The v1.0 header is 256 bytes and contains:

- an eight-byte magic value, major/minor versions, header size, total size, and
  fixed page-unit size;
- present-feature and required-feature masks;
- the object's own physical address;
- Burrow's allocated physical extent, ELF load bias, and physical entry address;
- the bootstrap stack's physical extent;
- direct descriptions of optional external initial-image, ACPI RSDP, and device
  tree resources;
- explicit offset/count/stride descriptors for the normalized memory map,
  command-line bytes, early-console record, and framebuffer record; and
- reserved zero space for compatible additions.

Mandatory entry fields remain direct header fields. Contained variable data uses
32-bit offsets relative to the object and explicit count/stride values. External
resources use 64-bit physical addresses and byte sizes. The memory-map section is
required in v1.0; every other described feature is optional.

All integer fields are little-endian. The object and contained sections are
eight-byte aligned. The protocol page unit is explicitly recorded and is 4096
bytes for v1. No native pointer, `size_t`, reference, `bool`, compiler enum,
bit-field, flexible array member, or implicit padding is part of the ABI.

Feature masks distinguish optional information from information the producer
requires the consumer to understand. Unknown required bits cause rejection.
Unknown present-only bits may be ignored by a compatible older consumer. Minor
versions append or activate previously reserved data without moving or changing
v1.0 fields; a major version is incompatible.

The v1 object has no checksum. It is an in-memory handoff constructed by the
trusted loader immediately before transfer, not a persistent or independently
transported artifact. Burrow instead performs ordered structural, arithmetic,
feature, section, resource, and memory-map validation before consumption.

## Alternatives Considered

### One Large Fixed Header

This makes every field directly accessible and minimizes descriptor parsing. It
would permanently reserve header space for speculative resources, make minor
extensions awkward, and encourage unrelated platform data to become mandatory
kernel-entry state.

### Generic TLV Record Directory

This provides excellent extensibility and lets future loaders preserve unknown
records naturally. It makes Burrow search and validate a generic record stream
before discovering core invariants such as its own loaded extent, entry address,
stack, and memory map. Those invariants benefit from fixed offsets and direct
compile-time layout checks.

### Raw Structure With Native Pointers

This is convenient within one compiler invocation but makes physical placement,
compiler ABI, pointer width, and object lifetime part of the contract. ADR-0010
already rejects this representation.

## Consequences

### Benefits

- entry-critical values have stable, directly checked offsets;
- optional variable data remains bounded and extensible;
- future non-UEFI loaders can construct the same object;
- host and target builds can assert every v1.0 layout constant;
- unknown optional minor-version features do not block boot; and
- early validation requires no allocation or native object graph.

### Costs And Risks

- the 256-byte header contains reserved space and several absent-feature fields;
- every defined section requires feature-consistency and overlap validation;
- 32-bit relative offsets limit one boot-information object to less than 4 GiB;
- retaining a source namespace makes each memory-map entry 40 bytes;
- producers must overlay their own allocations onto the normalized memory map;
  and
- a poorly disciplined minor extension could exhaust reserved bits or header
  space without earning a new major version.

## Follow-Up Work

- add the canonical C-compatible SDK ABI header;
- assert every specified size, alignment, and offset on host and AArch64 targets;
- implement allocation-free validation with independent valid and malformed
  fixtures;
- add a loader-side builder whose output passes the same validator;
- populate the object during the Burrow loader/handoff slices; and
- document reclamation after Burrow's physical allocator reserves every live
  boot resource.

## Revisit When

- a second loader cannot represent required information without abusing an
  existing field;
- a compatible minor extension cannot be safely skipped by a v1.0 consumer;
- contained machine descriptions plausibly approach the 32-bit offset limit;
- untrusted or persistent transport introduces an integrity requirement; or
- another architecture demonstrates that a supposedly universal core field is
  actually AArch64-specific.
