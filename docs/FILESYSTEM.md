# Warren Filesystem Strategy

**Status:** Planning constraints; no persistent system filesystem selected

Warren does not need to choose its lifetime filesystem before First Light. It
does need to prevent temporary early storage choices from constraining the
eventual 64-bit system.

## Three Different Problems

Early plans must not collapse these into one “filesystem decision”:

1. **Boot media:** UEFI loads Warren's bootloader from a FAT-formatted EFI System
   Partition. This is a firmware interoperability requirement, not Warren's
   system-filesystem design.
2. **Initial userspace image:** Burrow needs a simple, read-only initramfs-like
   archive to load the first user programs before block storage and writable
   filesystems are trustworthy.
3. **Persistent system storage:** Warren eventually needs directories, files,
   metadata, allocation, crash recovery, permissions, and large-volume support.
   This is the real filesystem choice and comes later.

The first two may be deliberately disposable formats. They do not receive
features merely to resemble the third.

## 64-Bit-Clean From The Public Boundary

The first backend may have small limits. Warren's VFS and SDK should not.

From their first stable versions:

- file offsets and logical file sizes use defined 64-bit unsigned domains;
- inode/object identifiers and block addresses do not assume 32-bit storage;
- byte counts, block counts, and addresses are distinct concepts;
- overflow is checked before addition, multiplication, narrowing, or alignment;
- directory iteration uses opaque continuation state, not an exposed byte offset;
- timestamps have an explicit epoch, signedness, unit, and resolution;
- APIs report backend limits instead of silently truncating values; and
- persistent structures never use `size_t`, pointers, native enums, or compiler
  padding.

This does not require the initial archive or disk backend to support enormous
files. It means their smaller limits are backend validation, not system ABI.

## Staged Storage Plan

### Stage A — EFI System Partition

Contains only the artifacts needed by firmware and the Warren bootloader. The
image builder creates it reproducibly on macOS. Burrow does not treat FAT as its
root filesystem merely because firmware can read it.

### Stage B — Read-Only Initial Image

Supply the earliest user executables and configuration through a memory-resident
archive. Its reader should be small, bounds-checked, fuzzable on the host, and
free of writable-media policy.

Before selecting the archive format, compare an established simple format with a
minimal Warren versioned container. A custom archive is justified only if it
makes validation or 64-bit target semantics materially clearer; it is not the
system filesystem in miniature.

### Stage C — VFS Semantics

Design the VFS around requirements exposed by processes and Hare:

- absolute and relative path lookup;
- directories and iteration;
- byte-stream reads and later writes;
- file and directory object lifetimes;
- mount points and filesystem instances;
- standard streams, pipes, devices, and pseudo-filesystems without pretending
  they all have identical storage behavior;
- permission checks at an explicit boundary; and
- cache/error behavior visible enough to diagnose.

The VFS should not simply copy one existing kernel's structures. It should use
familiar source-level semantics where they provide porting leverage.

### Stage D — Existing Read-Only Disk Filesystem

A small, documented existing filesystem is a strong candidate for the first
block-device reader. It lets Warren validate block I/O, caching, mounting, path
lookup, and corrupted-media handling without simultaneously debugging a new
allocator and recovery design.

Ext2 is worth evaluating for this role because its basic structures are widely
documented and do not require a journal. FAT is useful for interchange but is a
poor default model for Warren's long-term permissions and metadata. Selection
requires an ADR and corruption-test fixtures; neither is adopted yet.

### Stage E — Writable Storage

Writable support begins only after Warren has:

- a block interface with flush and ordering semantics;
- a cache with explicit ownership and writeback rules;
- fault injection for short, failed, reordered, and torn operations;
- a filesystem-independent test suite for VFS behavior; and
- recovery tooling that can inspect an image from the development host.

The first writable backend may be an existing format. “Warren wrote one file” is
not proof of allocation integrity or crash consistency.

### Stage F — Long-Term System Filesystem

Choose an existing filesystem or design a Warren-native one only from measured
requirements. If Warren invents one, it receives its own specification, host
inspection/repair tool, compatibility rules, image corpus, and multi-stage
recovery tests before it stores irreplaceable data.

## Decision Criteria For The Long-Term Filesystem

The decision must state desired behavior for:

- maximum volume, file, directory, and object counts;
- crash consistency and atomic operation boundaries;
- metadata and data checksums;
- repair, rollback, and damaged-volume behavior;
- allocation, fragmentation, sparse files, and free-space accounting;
- permissions, ownership, links, extended attributes, and device objects;
- timestamp semantics;
- case sensitivity and Unicode filename policy;
- removable-media and cross-platform access;
- snapshots, cloning, compression, and encryption, if actually required;
- online growth, shrinking, and background maintenance;
- memory requirements on small systems;
- bootloader access requirements; and
- licensing, tooling, and maintenance burden.

Features are not automatically virtues. Each one expands the correctness,
recovery, and self-hosting burden.

## If Warren Designs Its Own

“Based on” an existing filesystem must mean something precise:

- **compatible implementation:** Warren preserves the existing on-disk format;
- **compatible extension:** older tools see a defined subset or reject a feature
  flag safely; or
- **new design informed by another:** the format is Warren-owned and makes no
  compatibility claim.

Changing an existing format to fix its disadvantages often sacrifices its tools
and interoperability—the main reasons to adopt it. That may still be worthwhile,
but the trade must be explicit.

A Warren-native on-disk format must have from version 1:

- magic, format version, compatible/incompatible feature flags, and checksums;
- fixed-width little- or big-endian encoding selected explicitly;
- explicit block and structure sizes;
- redundant critical metadata or a documented recovery model;
- unknown-feature rejection rules;
- deterministic host-side image creation and inspection;
- byte-level specification independent of C++ structs; and
- an upgrade story that never relies on silently reinterpreting old bytes.

## Testing Strategy

Filesystem code is tested below and above Burrow:

- host fuzzing of parsers and directory/path logic;
- golden valid, edge-case, and corrupt images;
- in-memory block devices with deterministic failure injection;
- power-loss simulation at every durable write boundary;
- mount/unmount and remount verification;
- cross-checks with independent tools for compatible formats; and
- long-running allocation/free and cache-coherency tests under QEMU.

Recovery behavior is part of the format contract, not a utility added after the
first corruption report.

## Current Decision

No long-term persistent filesystem is selected.

The adopted sequence is:

```text
FAT EFI System Partition
        -> read-only initial image
        -> 64-bit-clean VFS
        -> existing read-only disk format
        -> proven writable-storage machinery
        -> evidence-based long-term filesystem decision
```

This sequence keeps both options open: adopt a mature filesystem outright, or
create a Warren-native design because Warren has concrete requirements worth its
substantial lifetime cost.
