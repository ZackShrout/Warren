# Warren AArch64 Normalized Entry

**Status:** Branch implementation contract under ADR-0002, ADR-0007,
ADR-0011, and ADR-0016; implemented through owned-table activation,
higher-half transfer, and identity removal; generic C++ entry remains

**Target:** ARMv8.0-A, non-secure AArch64 EL1 or EL2, 4 KiB pages

## 1. Scope And Invariants

This specification owns the production boundary from
`burrow_aarch64_entry` through the first architecture-neutral C++ call. It fixes
the exception-level transition, emergency and stable vectors, early storage,
translation regime, mapping attributes, higher-half transfer, identity
removal, and C ABI presented to `burrow_kernel_entry`.

The loader contract in `AARCH64_LOADER_HANDOFF.md` remains unchanged. Incoming
addresses are physical values reached through firmware's identity mapping.
Burrow accepts only non-secure AArch64 EL1 and EL2, keeps DAIF masked throughout
this boundary, and never returns to firmware or to an earlier transition stage.

The boundary has these non-negotiable invariants:

- an aligned emergency vector base is installed before any variable-size read;
- the production boot-information validator is the only complete-object gate;
- only validated `usable` pages can back the transition arena;
- both initial EL routes reach the same MMU-off, cache-off common EL1 state;
- page tables are complete and audited before any translation register changes;
- the upper image has no writable alias and MMIO is never Normal memory;
- the stable vector table and guarded stack are active before TTBR0 replacement;
- TTBR0 names an empty root before generic C++ runs; and
- no address-valued C++ context field is a live lower-half pointer.

## 2. Stable Transition State

The implementation records the last completed stage as one byte-sized value.
These values and names are stable diagnostic and test vocabulary:

| Value | Stage | Completion condition |
| ---: | --- | --- |
| 0 | `captured` | Physical boot address, incoming SP, `CurrentEL`, and DAIF are held without an external dereference |
| 1 | `emergency-vectors` | `VBAR_EL1` or `VBAR_EL2` names the inherited physical vector alias and an `ISB` has completed |
| 2 | `validated` | The complete declared boot-information object has passed `validate_boot_information()` |
| 3 | `common-el1` | Execution is EL1h with DAIF masked and `SCTLR_EL1.M/C/I` clear |
| 4 | `planned` | The immutable mapping and transition-storage plan is complete |
| 5 | `owned-tables` | Every table descriptor is materialized and the independent preflight has passed |
| 6 | `activated` | Owned TTBRs, MAIR, TCR, SCTLR, and TLB state are active |
| 7 | `higher-half` | PC, VBAR, SP, and every retained virtual reference use owned upper mappings |
| 8 | `identity-removed` | TTBR0 names the empty root and the final EL1 invalidation sequence is complete |
| 9 | `kernel-cpp` | `burrow_kernel_entry` returned the exact witness result |

The stage is advanced only after the condition in the table is observable. A
terminal exception reports the last completed value; it does not claim the
stage whose operation faulted.

## 3. Capture, DAIF, And Emergency Vectors

The first instructions copy `x0`, `sp`, `CurrentEL`, and `DAIF` to reserved
general registers before using the stack or external memory. `x1` through `x3`
must still be zero, `x0` must be nonzero and eight-byte aligned, SP must be
16-byte aligned, and all four DAIF masks must be set. `CurrentEL` must be
`0x4` or `0x8`.

The implementation then obtains the physical address of the matching emergency
table with PC-relative instructions, requires 2 KiB alignment, writes
`VBAR_EL1` or `VBAR_EL2`, and executes `ISB`. The EL1 route never accesses an
EL2 register. The 16 slots are 128 bytes each in architected order and every
slot branches without a call to one common reporter for its vector number.

Until the complete early-console record is valid, the reporter stores only its
bounded internal capture and enters a DAIF-masked `WFE` loop. Afterwards it may
emit one bounded line containing stage, vector number, current EL, `ESR_ELx`,
`ELR_ELx`, `FAR_ELx`, and `SPSR_ELx`. A reporter-active flag makes a nested
exception enter a separate silent wait instead of recursing. Exceptions never
resume during normalized entry.

The stable table has the same 16-slot geometry and reporting contract. Its
address is the stable image alias and `VBAR_EL1` is changed to that alias only
after the high branch succeeds.

## 4. Complete Object Validation

Assembly performs only the existing fixed-prefix checks needed to bound the
declared `total_size`. A fault while touching that declared span is contained by
the emergency vectors. The exact function
`warren::boot::validate_boot_information(object, total_size,
self_physical_address)` then validates the complete object. No memory-map entry,
feature record, or external physical range is used for planning before it
returns `success`.

The validated view retains the physical base, exact `total_size`, normalized
memory-map address/count, and validated PL011 aperture. It does not copy or
mutate the protocol object.

## 5. Common EL1 Normalization

### 5.1 Cache And Translation Teardown

Both routes abandon firmware translation rather than replacing it live. Before
clearing a current-level data-cache enable, the implementation walks every
implemented data or unified cache described by `CLIDR_EL1` and `CCSIDR_EL1`,
cleans and invalidates every set/way to the point of coherency, and completes
`DSB SY`. It then clears `SCTLR_ELx.M`, `.C`, and `.I`, executes `ISB`,
invalidates the instruction cache with `IC IALLU`, and completes `DSB SY; ISB`.
No firmware mapping is dereferenced after that point.

The operation is assembly-only. Its continuation address, current stack, boot
object, and vector address have been checked as physical identity addresses
before the first control-register write. There is no call or return while the
procedure-call ABI crosses this interval.

### 5.2 Initial EL1 Route

The EL1 route performs the teardown against `SCTLR_EL1`, preserving the
architecturally required RES1 bits while clearing M/C/I. It branches directly
to `burrow_aarch64_common_el1` with the captured physical values in registers.
It does not read or write any `_EL2` register.

### 5.3 Initial EL2 Route

The EL2 route retains the EL2 emergency table through the descent and programs:

| Register | Exact policy |
| --- | --- |
| `HCR_EL2` | `0x0000000080000000`: `RW=1`; `VM`, `E2H`, `TGE`, `IMO`, `FMO`, and `AMO` are zero |
| `CPTR_EL2` | `0`: no EL2 trap of FP/SIMD or CPACR access; Burrow itself still uses no FP/SIMD |
| `CNTHCTL_EL2` | `0x3`: EL1 physical counter and physical timer access enabled |
| `CNTVOFF_EL2` | `0`: virtual count is not offset |
| `SCTLR_EL1` | `0x0000000030D00800`: ARMv8.0-A RES1 baseline with M/C/I clear |
| `VBAR_EL1` | Physical address of the EL1 emergency table |
| `SP_EL1` | Captured 16-byte-aligned physical bootstrap-stack pointer |
| `ELR_EL2` | Physical address of `burrow_aarch64_common_el1` |
| `SPSR_EL2` | `0x3C5`: D/A/I/F masked and AArch64 EL1h selected |

`HCR_EL2.VM=0` ensures no stage-2 translation. `CPACR_EL1` and `CNTKCTL_EL1`
are cleared so FP/SIMD and EL0 timer access remain unavailable. After the
current EL2 cache/translation teardown, the route writes the table above,
executes `DSB SY; ISB`, and performs one `ERET`. An exception or return to EL2
is terminal.

At `burrow_aarch64_common_el1`, both routes prove `CurrentEL == 0x4`, all DAIF
masks set, SP unchanged, `SCTLR_EL1.M/C/I == 0`, and PC equal to the expected
physical common label. The later implementation may compare additional
registers, but it may not weaken these common-state checks.

## 6. Feature Discovery And Translation Registers

`ID_AA64MMFR0_EL1.TGran4` must use the base supported encoding `0`. Extension
encodings and `0xF` (unsupported) are rejected for this ARMv8.0-A slice.
`PARange` encodings 0 through 5 mean 32, 36, 40, 42, 44, and 48 output bits.
Encoding 6 may be present on a future machine, but this implementation caps the
configured output size at 48 bits and still rejects every required physical end
at or above `2^48`. Other encodings are unsupported.

`MAIR_EL1` is `0x00000000000000FF`:

| AttrIdx | Byte | Meaning |
| ---: | ---: | --- |
| 0 | `0xFF` | Normal, inner/outer write-back, read-allocate, write-allocate |
| 1 | `0x00` | Device-nGnRnE |

`TCR_EL1` has base value `0x00000000B5103510`, plus `IPS` in bits 34:32. IPS is
the lesser of the implemented `PARange` and encoding 5 and must cover every
planned output address. This fixes:

- `T0SZ=T1SZ=16`, four levels, 48-bit lower and upper regions;
- `TG0=0b00` and `TG1=0b10`, both 4 KiB;
- `IRGN0/ORGN0/IRGN1/ORGN1=0b01`, WBWA table walks;
- `SH0=SH1=0b11`, inner-shareable walks;
- `TBI0=TBI1=0`, `A1=0`, ASID 0, and 8-bit ASIDs;
- `EPD0=EPD1=0`; and
- `DS`, hardware access/dirty update, hierarchical-permission disable, and all
  extension fields clear.

`TTBR0_EL1` and `TTBR1_EL1` contain only a 4 KiB-aligned physical BADDR, ASID
zero, and `CnP=0`. Initial `SCTLR_EL1` is `0x0000000030D8181D`: the ARMv8.0-A
RES1 baseline plus `M`, `C`, `I`, `SA`, `SA0`, and `WXN`; EE/E0E and every
unselected control are zero. WXN is a second hardware backstop, not a substitute
for descriptor permissions.

## 7. Transition Storage And Mapping Plan

The one-shot planner selects the lowest physical 128-page contiguous extent
that remains wholly inside validated `usable` memory after subtracting every
live physical resource. Selection is 4 KiB aligned and deterministic. A split
across memory-map entries is allowed only when the entries are adjacent,
identically `usable`, and their retained source attributes agree.

The arena has this immutable ownership:

| Arena pages | Use |
| --- | --- |
| 0 | Empty lower-half root installed after identity removal |
| 1–16 | 64 KiB owned early-stack backing |
| 17–127 | At most 111 active page-table pages, including initial TTBR roots |

The fixed mapping plan has capacity 16. It records physical start, virtual
start, page count, memory type, read/write/execute permissions, and whether the
range is a temporary identity alias. Checked arithmetic rejects every rounded
end, bias, direct-map conversion, and page-count overflow. Exact-fit arena and
record capacities succeed; one additional required page or record fails before
translation state changes.

The owned stack uses:

```text
lower guard:  0xFFFFD00000000000 (one unmapped page)
usable stack: 0xFFFFD00000001000 through 0xFFFFD00000010FFF
initial SP:   0xFFFFD00000011000
upper guard:  0xFFFFD00000011000 (one unmapped page)
```

The PL011 page uses virtual address `0xFFFFC00009000000` for the reference
physical aperture `0x0000000009000000`. The planner still obtains and validates
the physical address from boot information and rejects a different aperture in
the reference profile; architecture-neutral code never embeds either value.

## 8. Table Descriptors And Required Mappings

Every table and page descriptor has AF set, nG clear, DBM clear, contiguous
clear, EL0 access disabled, and no unselected software or extension bit. Table
descriptors set UXNTable and leave PXNTable clear so the privileged image can
execute while no descendant is executable at EL0. Leaf policy is:

| Mapping class | AttrIdx | AP | SH | PXN | UXN |
| --- | ---: | ---: | ---: | ---: | ---: |
| Image text | 0 | EL1 read-only | Inner | 0 | 1 |
| Image read-only data | 0 | EL1 read-only | Inner | 1 | 1 |
| Image writable data | 0 | EL1 read-write | Inner | 1 | 1 |
| Boot information | 0 | EL1 read-only | Inner | 1 | 1 |
| Arena and stack backing | 0 | EL1 read-write | Inner | 1 | 1 |
| PL011 | 1 | EL1 read-write | Outer | 1 | 1 |

The TTBR0 hierarchy contains only temporary page mappings for executing image
pages, the bootstrap stack pages still in use, the complete boot-information
object, the complete transition arena, and the one PL011 page. Identity image
aliases use the same permissions as their stable aliases. Gaps between image
segments remain unmapped.

TTBR1 maps image pages at ELF virtual address plus `0xFFFFFFFF80000000`, boot
information and the arena at `0xFFFF800000000000 + physical`, PL011 at the fixed
MMIO address above, and the 16 stack pages in the dynamic range. It does not map
the Burrow image through the direct map or include unrelated usable memory.

The builder maps 4 KiB pages conservatively, rejects a valid descriptor at an
intermediate level, and never overwrites an existing leaf. A repeated identical
request is an error rather than hidden deduplication. Before activation an
independent walker proves every required source and target address, permission,
attribute, guard absence, arena bound, and lack of writable executable aliases.

## 9. Activation, High Transfer, And TTBR0 Removal

With MMU and caches off, table writes are completed with `DSB SY`. Activation
then executes this ordered boundary without a call or stack access:

1. write `MAIR_EL1`, `TCR_EL1`, `TTBR0_EL1`, and `TTBR1_EL1`;
2. `ISB`; `TLBI VMALLE1`; `DSB ISH`; `ISB`;
3. write `SCTLR_EL1 = 0x30D8181D`;
4. `ISB` and branch through a register to the corresponding stable image alias;
5. write the stable upper-half address to `VBAR_EL1` and execute `ISB`;
6. set SP to `0xFFFFD00000011000` without an intervening stack access;
7. replace every retained resource reference with its checked upper alias; and
8. resume the procedure-call ABI only after those changes complete.

From the higher-half continuation, identity removal executes `DSB ISHST`,
writes the physical address of arena page 0 to `TTBR0_EL1`, executes `ISB`, then
executes `TLBI VMALLE1IS; DSB ISH; ISB`. No instruction after the TTBR write
uses a low branch target, stack address, vector, data address, or pointer.

The implementation immediately proves `CurrentEL`, TTBR0/TTBR1, VBAR, SP,
TCR, MAIR, SCTLR, and upper-half canonicality before constructing the generic
entry context. A failed proof is an identity-removal failure, never success.

## 10. Architecture-Neutral Entry ABI

The C-compatible `KernelEntryContext` is 64 bytes and eight-byte aligned:

| Offset | Type | Field |
| ---: | --- | --- |
| `0x00` | `uint32_t` | ABI major, value 1 |
| `0x04` | `uint32_t` | Structure size, value 64 |
| `0x08` | `uint64_t` | Flags, exact value `0x3`: bit 0 owned tables, bit 1 identity removed |
| `0x10` | `uint64_t` | Higher-half boot-information address |
| `0x18` | `uint64_t` | Boot-information physical address as a value, not a pointer |
| `0x20` | `uint32_t` | Exact boot-information byte count |
| `0x24` | `uint32_t` | Initial exception level, value 1 or 2 |
| `0x28` | `uint64_t` | Transition-arena physical address as a value, not a pointer |
| `0x30` | `uint64_t` | Transition-arena page count, value 128 |
| `0x38` | `uint64_t` | Higher-half retained-witness address |

`x0` points to this immutable higher-half context, `x1` through `x7` are zero,
SP is 16-byte aligned, DAIF remains masked, and FP/SIMD is unavailable. The
extern-C function includes only Core-owned context and boot-information
validation declarations. It revalidates the aliased complete object, validates
all context invariants, writes its retained witness, and returns the exact
`uint32_t` value `0x57415231`. Any other return is C++ context failure.

Only after that value is checked may the architecture continuation emit:

```text
BURROW_NORMALIZED_ENTRY:initial=EL1:normalized=EL1:tables=owned:identity=removed:cpp=arrived
```

or the matching `initial=EL2` form, followed by the versioned terminal success
record in a test image. Ordinary images enter a masked wait after the bounded
diagnostic.

## 11. Failure Allocation And Test Profiles

Normalized-entry failures use these test-only `FAIL` result codes; architectural
exceptions use common `PANIC` code 4:

| Code | Meaning |
| ---: | --- |
| 74 | Complete boot-information validation |
| 75 | Unsupported architectural state or feature |
| 76 | EL2 descent or common-EL1 proof |
| 77 | Transition storage planning |
| 78 | Table construction or descriptor audit |
| 79 | Table activation or higher-half transfer proof |
| 80 | Identity removal or surviving low reference |
| 81 | C++ entry context or witness result |

The authoritative QEMU machine arguments are exactly:

```text
EL1: virt-11.0,gic-version=3,virtualization=off
EL2: virt-11.0,gic-version=3,virtualization=on
```

Both use pinned TCG, Cortex-A57, one CPU, and 512 MiB. The harness selects the
machine argument through required `--initial-el el1|el2` input. A successful
route must contain matching loader and Burrow initial-EL observations and,
after implementation, the matching normalized diagnostic. Missing or contrary
evidence is a protocol failure; the harness never substitutes direct entry.

## 12. Required Disassembly Evidence

Debug and Release verification must locate and check, from the linked objects:

- capture before external access and both aligned VBAR writes;
- all 16 correctly spaced slots in both vector tables;
- an EL1 branch with no EL2 register access and the exact EL2 register program;
- cache teardown, `ERET`, and the common physical EL1 label;
- exact MAIR/TCR/SCTLR values and ordered TTBR/TLBI/barrier operations;
- the low-to-high branch, stable VBAR and SP writes, and empty TTBR0 install;
- the one call to `burrow_kernel_entry` and exact result comparison; and
- every test-only fault branch and semihosting trap, with both absent from the
  ordinary image.

Host tests independently exercise the validator consumer, arena planner, table
walker, activation preflight, context ABI, and generic entry witness. Live tests
cover both initial exception levels plus emergency/stable vector, both guard,
text-write, data-execute, and stale-identity faults.

## 13. Architecture References

- [Arm Architecture Reference Manual: 4 KiB VMSAv8-64 translation](https://developer.arm.com/documentation/ddi0487/latest/)
- [Armv8-A virtualization](https://developer.arm.com/documentation/102142/latest/)
- [Armv8-A memory management](https://developer.arm.com/documentation/101811/latest/)
- [QEMU Arm `virt` machine](https://www.qemu.org/docs/master/system/arm/virt.html)
