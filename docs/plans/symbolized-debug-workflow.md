# Symbolized Debug Workflow Branch Plan

**Branch:** `codex/symbolized-debug-workflow`

**Status:** Complete

## Observable Result

A Debug build produces an audited linker map and a ready-to-source LLDB command
file for Burrow's fixed stable image alias. A host tool converts an ELF-relative,
stable virtual, or loader-biased physical runtime address to the symbol image,
then reports its function and repository-relative source location. Opt-in system
targets start the pinned EL1 or EL2 QEMU profile paused with a local GDB stub.

## Included Work

- structural verification of the generated LLD map against the linked ELF entry
  and bounded image extent;
- explicit bootstrap checks for the LLVM symbolizer and LLDB shipped with the
  pinned LLVM installation;
- strict runtime-address normalization for ELF, stable, and physical aliases;
- batched `llvm-symbolizer` use with repository-relative output;
- generated LLDB command files for stable or explicit physical symbol slides;
- opt-in paused QEMU EL1 and EL2 debug launch targets using disposable firmware
  variable storage;
- host tests for map parsing, address bounds, symbolizer output, LLDB generation,
  and QEMU command construction; and
- documentation of the exact attach, slide, symbolization, and failure workflow.

## Non-Goals

- an in-guest unwinder, backtrace engine, symbol table, or debugger server;
- debugging UEFI or dynamically discovering a firmware-selected physical bias;
- changing the boot protocol, virtual layout, exception record, or panic record;
- network-exposed debugger listeners, automatic port selection, or concurrent
  debugger sessions; and
- checking generated maps, command files, traces, or serial logs into source.

## Verification Matrix

- Host tests reject malformed maps, mismatched entries, invalid aliases,
  underflow, overflow, missing tools, malformed symbolizer output, and invalid
  debug ports.
- Debug Burrow builds verify their map, retain DWARF, and generate an LLDB file
  with the fixed `0xFFFFFFFF80000000` stable slide.
- Release Burrow builds verify their map without claiming source-level DWARF.
- A real Debug symbolization check resolves retained C++, assembly, stable, and
  physical addresses to the expected repository sources.
- Existing host and Debug/Release system matrices remain green.

## Merge Gates

- Every runtime address is normalized explicitly and checked against the map's
  image extent before invoking external tooling.
- LLDB and QEMU debug entry points are opt-in and bind the GDB stub to loopback.
- No debug helper, symbolizer, debugger, or map parser enters a target image.
- Architecture, development, roadmap, image, and bootstrap documentation match
  the implemented workflow.
- The completed branch is merged to `main` and pushed to the configured remote.
