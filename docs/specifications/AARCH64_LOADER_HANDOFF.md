# Warren AArch64 Loader Handoff

**Status:** Implemented subordinate specification for ADR-0011

## 1. Scope

This boundary owns only the instruction-cache synchronization and final
loader-to-Burrow transfer that cannot safely be expressed as ordinary C++.
It does not exit boot services, validate the complete boot-information object,
install exception vectors, change exception level, or alter the inherited MMU
and cache configuration.

The production UEFI entry path invokes this boundary only after successful boot
information finalization and `ExitBootServices()`. Burrow's first-entry witness
receives the transfer; any loader-side failure after the firmware boundary uses
the separate containment path in section 5.

## 2. Checked Preparation

Before firmware exit, `prepare_aarch64_handoff()` derives the sole executable
range from the canonical executable load segment. The physical half-open range
is `[load_bias + virtual_address, begin + memory_size)`. Both additions are
checked, the complete range must lie inside the materialized Burrow allocation,
and the relocated entry must be four-byte aligned and contained by that range.

Preparation also validates the allocated handoff storage, checks the bootstrap
stack top without overflow and for 16-byte alignment, requires an eight-byte-
aligned boot-information address, and accepts only the reference PL011 record:
output support, a four-byte-aligned bounded MMIO base, register stride 4, and
register width 32. On success it produces the only argument records accepted by
the target cache and transfer wrappers.

## 3. Instruction-Cache Synchronization

`warren_aarch64_synchronize_instruction_range(begin, end)` reads `CTR_EL0` and
derives the data and instruction line sizes independently as
`4 << DminLine` and `4 << IminLine`. It aligns each walk down to its respective
line boundary. Every advancing add checks carry so a range ending at the top of
the address space cannot wrap and loop indefinitely.

The sequence is:

1. `DC CVAU` over every data-cache line intersecting the executable range;
2. `DSB ISH`;
3. `IC IVAU` over every instruction-cache line intersecting the range;
4. `DSB ISH`; and
5. `ISB`.

The maintenance operations are deliberately unconditional even when `CTR_EL0`
advertises IDC or DIC coherence. This keeps one ARMv8.0-A-compatible sequence
for the inherited firmware environment. The routine does not clean unrelated
loaded data, disable caches, or modify translation state.

## 4. Nonreturning Transfer

The assembly handoff receives the boot-information address, relocated entry,
checked stack top, and validated PL011 base. It reads `CurrentEL` and emits
exactly one of:

```text
WARREN_POST_EXIT:ExitBootServices:EL1\r\n
WARREN_POST_EXIT:ExitBootServices:EL2\r\n
WARREN_POST_EXIT:ExitBootServices:EL?\r\n
```

The primitive polls PL011 `FR.TXFF` at offset `0x18` and writes each ASCII byte
through the 32-bit data register. An unexpected exception level emits the `EL?`
record, masks DAIF, and remains in a `WFE` loop rather than entering Burrow.

For EL1 or EL2, the final uninterrupted sequence masks every DAIF class, sets
`sp` to the exact bootstrap-stack top, restores the boot-information address to
`x0`, clears `x1` through `x3`, and executes `BR` to the relocated entry. It has
no call, return, compiler epilogue, firmware access, or recovery path.

The UEFI image verifier disassembles both exported symbols from the exact
handoff object linked into every debug and release image. It checks the ordered
maintenance instructions, overflow guards, CurrentEL and PL011 operations,
exact final transfer sequence, and absence of calls/returns in the handoff. The
link force-retains both symbols while the verifier separately requires each
diagnostic string exactly once in the final EFI image.

## 5. Post-Exit Failure Containment

Any loader failure after the first `ExitBootServices()` attempt uses the
separate `warren_aarch64_post_exit_failure` boundary. It writes
`WARREN_POST_EXIT:FAIL` directly through the validated PL011, masks DAIF, and
cannot call or return to firmware. Ordinary images then remain in a `WFE` loop.

The assembly boundary accepts an internal optional nonreturning handler address.
Production always supplies zero. The dedicated QEMU fixture supplies a
platform-owned handler only after successful firmware exit; the boundary emits
its normal failure diagnostic, masks DAIF, and branches to that handler, which
reports `FAIL`/73 through the test protocol. Artifact verification requires the
handler, marker, semihosting block, and HLT to be absent from ordinary UEFI
images and present exactly once in the selected fixture.
