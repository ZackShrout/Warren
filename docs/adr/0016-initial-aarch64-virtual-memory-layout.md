# ADR-0016: Initial AArch64 Virtual-Memory Layout

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 1–2
- **Supersedes:** None
- **Superseded by:** None

## Context

ADR-0009 requires Burrow to enter through firmware's identity mapping and later
replace that environment with Burrow-owned virtual memory. ADR-0011 places the
transition in Burrow's AArch64 architecture layer and requires architecture-
neutral kernel entry to occur at normalized EL1. Neither decision assigns stable
virtual addresses or defines which aliases survive the transition.

The first owned tables must support a safe transition rather than merely make a
higher-half address reachable. They must prevent firmware mappings from becoming
an implicit kernel ABI, keep MMIO attributes explicit, avoid writable aliases of
executable kernel pages, reserve room for later kernel facilities, and leave a
credible lower-half userspace direction without prematurely defining a process
ABI.

The reference Cortex-A57 and the future x86-64 direction both support a
conventional 48-bit canonical split and 4 KiB base pages. Warren does not need to
trade that long-term shape for the slightly smaller initial page-table tree of a
39-bit AArch64 layout.

## Decision

Burrow's initial AArch64-owned address space uses a **48-bit canonical virtual-
address layout**, a **4 KiB translation granule**, and separate lower and upper
translation-table roots.

- TTBR0_EL1 owns the lower canonical half.
- TTBR1_EL1 owns the upper canonical half.
- Early kernel execution uses ASID 0.
- Top-byte-ignore is disabled for both halves; every pointer is canonical and
  untagged.
- The initial implementation uses four translation levels. It may use 2 MiB or
  1 GiB block descriptors only where ownership, attributes, and permissions are
  homogeneous across the complete block.
- Translation-table walks use normal, inner-shareable, write-back,
  write-allocate memory. The register-level TCR_EL1, MAIR_EL1, SCTLR_EL1,
  barrier, cache, and TLB program remains part of the focused AArch64 transition
  specification required by ADR-0011.

The lower canonical half is `0x0000000000000000` through
`0x00007FFFFFFFFFFF`. It is reserved for future user address spaces. The range
does not establish a userspace image base, stack location, shared-library
layout, allocation policy, or stable Warren ABI. TTBR0 is empty after early
identity mappings are removed and remains so until user-mode work gives the
lower half a real consumer.

Addresses `0x0000800000000000` through `0xFFFF7FFFFFFFFFFF` are noncanonical in
this 48-bit layout and must fault.

The upper canonical half is assigned as follows:

| Inclusive range | Size | Initial purpose |
| --- | ---: | --- |
| `0xFFFF800000000000`–`0xFFFFBFFFFFFFFFFF` | 64 TiB | Direct map of eligible normal physical memory |
| `0xFFFFC00000000000`–`0xFFFFCFFFFFFFFFFF` | 16 TiB | Explicit MMIO mappings |
| `0xFFFFD00000000000`–`0xFFFFEFFFFFFFFFFF` | 32 TiB | Kernel dynamic mappings, heaps, and guarded stacks |
| `0xFFFFF00000000000`–`0xFFFFFFFEFFFFFFFF` | 16 TiB minus 4 GiB | Reserved for later kernel facilities |
| `0xFFFFFFFF00000000`–`0xFFFFFFFF7FFFFFFF` | 2 GiB | Permanently unmapped kernel-image guard |
| `0xFFFFFFFF80000000`–`0xFFFFFFFFFFFFFFFF` | 2 GiB | Stable Burrow image slot |

The upper half is privileged and never accessible at EL0.

### Kernel Image And Load Biases

Burrow remains the static position-independent ELF64 `ET_DYN` image selected by
ADR-0009. The linker and image verifier constrain every loadable ELF virtual
address to the range `0 <= elf_virtual_address < 0x80000000`.

The loader-selected physical placement and the stable kernel virtual placement
are deliberately separate:

```text
runtime physical = elf virtual + boot_information.kernel_load_bias
runtime virtual  = elf virtual + 0xFFFFFFFF80000000
```

The physical load bias is per boot and remains part of boot-information v1. The
virtual bias is a Burrow build/layout constant. No physical placement becomes a
kernel pointer or stable ABI.

Loadable segment pages receive permissions from the validated ELF program
headers:

- executable text is read-only and executable;
- read-only data is read-only and execute-never;
- writable data and BSS are read-write and execute-never;
- unoccupied segment gaps remain unmapped; and
- no page is writable and executable at the same time.

The linker and verifier reject a segment arrangement that requires conflicting
permissions within one 4 KiB page.

### Direct Physical Map

For an eligible mapped physical byte below 64 TiB, the direct-map address is:

```text
direct virtual = 0xFFFF800000000000 + physical address
```

This formula reserves an address; it does not promise that every physical
address has a valid mapping. Conversion from a typed physical address to a
direct-map pointer checks that the complete requested range is currently mapped
with suitable ownership and attributes.

The direct map initially includes only validated normal-memory pages that the
current initialization phase is permitted to access. It excludes:

- the Burrow image allocation, preventing a writable alias from defeating its
  segment permissions;
- MMIO and framebuffer apertures;
- firmware-runtime, reserved, unusable, and otherwise unsupported memory; and
- any page whose retained source attributes require a mapping policy Burrow has
  not implemented.

Boot-information, initial-image, page-table, and bootstrap-stack pages may
receive direct-map aliases with permissions appropriate to their current
ownership. Reclamation changes their ownership before it changes their mapping.
Normal physical memory at or above 64 TiB remains reserved and unavailable to
the initial allocator until an explicit high-memory mapping mechanism is added.

### MMIO And Dynamic Mappings

MMIO is mapped only through the explicit MMIO window. Each mapping records its
physical extent, page-rounded virtual extent, device/cache attributes,
permissions, and lifetime. MMIO mappings are read-write, privileged, and
execute-never unless a later device-specific decision proves a stricter mapping.
Device mappings never appear in the normal-memory direct map.

Kernel heaps, owned stacks, temporary physical mappings, and general `vmalloc`-
style reservations use the dynamic window. Every stable kernel stack is
read-write and execute-never and has at least one unmapped guard page below and
above its usable range. Subdivision and allocation policy within the MMIO and
dynamic windows remain implementation choices until concrete allocators exist.

### Firmware-To-Burrow Transition

Burrow adopts the layout through explicit stages:

1. **Firmware stage.** The inherited identity mapping remains active. Early
   assembly treats addresses supplied by the handoff as physical addresses and
   accesses only the validated resources required to continue.
2. **Owned-table construction.** After supported EL2 state has been normalized
   and execution is at EL1, Burrow constructs an owned TTBR0 identity subset and
   the TTBR1 upper-half mappings. The identity subset contains only the active
   kernel pages, current stack, boot-information object, new page tables, early
   diagnostic MMIO, and any transition data actually in use.
3. **Owned-table activation.** Burrow activates its tables using the reviewed
   register, barrier, cache, and TLB sequence. From successful activation
   onward, no firmware mapping is trusted or dereferenced unless Burrow
   explicitly recreated it.
4. **Higher-half transfer.** Execution branches to the corresponding address in
   the stable image slot. Exception vectors and the stack move to owned
   higher-half mappings, and every surviving boot resource is copied or given a
   deliberate upper-half mapping.
5. **Identity removal.** After no instruction, stack reference, vector, or live
   pointer depends on a low alias, Burrow removes every TTBR0 identity mapping
   and performs the required invalidation sequence. TTBR0 then names an empty
   lower-half root until user address spaces are introduced.

Identity aliases use the same or stricter permissions as their higher-half
counterparts and exist only for the bounded transition. The complete firmware
map is never copied as a convenience.

### Address-Width Discovery

The architecture layer reads the implemented physical-address range and 4 KiB
granule support from AArch64 feature registers before constructing tables. The
initial implementation supports output addresses up to 48 bits and does not
enable LPA or LPA2. It rejects a machine that cannot represent every required
boot resource under the selected configuration.

The 64 TiB direct-map ceiling is independent of the page-table output-address
width. Physical MMIO outside that ceiling may still be mapped into the explicit
MMIO window when the reported architectural range and retained source
attributes permit it.

## Commitments And Provisional Reservations

The following are commitments of this decision:

- 48-bit lower/upper canonical separation and 4 KiB base pages;
- a lower TTBR0 half and privileged upper TTBR1 half;
- the six upper-half region boundaries;
- the direct-map translation formula and exclusion of MMIO;
- the stable Burrow virtual-image bias and 2 GiB image limit;
- removal of transition identity mappings before generic kernel operation; and
- permission, guard-page, and no-writable-executable-alias rules.

The following remain provisional:

- user executable, stack, shared-library, and ASLR policy;
- internal subdivision of the MMIO, dynamic, and reserved windows;
- virtual allocation algorithms and metadata;
- physical memory above the initial direct-map ceiling;
- kernel address randomization;
- ASID allocation and process-specific TTBR0 management;
- recursive page-table mappings or dedicated page-table inspection windows; and
- whether a future x86-64 port uses identical numeric boundaries rather than the
  same conceptual separation.

## Alternatives Considered

### 39-Bit, Three-Level Initial Layout

This removes one page-table level and reduces the amount of empty virtual space
during bring-up. It constrains the complete address space to 512 GiB, gives the
direct map and kernel arenas much less headroom, and knowingly requires a later
layout migration for little practical reduction in Phase 1 complexity.

### 48-Bit Layout Without Fixed Upper-Half Regions

This would choose only the higher-half image address and allocate every other
mapping top-down as needed. It preserves flexibility but allows direct-map,
MMIO, heap, stack, and future facility allocations to collide before their
ownership rules become visible. Phase 0 requires enough partitioning to prevent
that accidental policy.

### Loader-Enters-Directly-At-Higher-Half

The UEFI bootloader could build Burrow's tables and invoke its stable virtual
entry immediately. ADR-0009 and ADR-0011 intentionally keep CPU and page-table
normalization in Burrow so the architecture entry can be reused by a non-UEFI
loader.

### Permanent Universal Identity Or Direct Mapping

Mapping every physical page would make early physical access convenient. It
would preserve firmware-derived assumptions, map devices with unsafe normal-
memory attributes, and create writable aliases of executable kernel pages.

## Consequences

### Benefits

- kernel pointers become independent of firmware-selected physical placement;
- future user and kernel addresses occupy architecturally separate halves;
- direct normal-memory access remains efficient without making MMIO implicit;
- segment permissions and guard pages are meaningful rather than cosmetic;
- the bounded transition has an observable point where firmware mappings cease
  to be trusted; and
- the conceptual split carries cleanly to a later x86-64 audit.

### Costs And Risks

- four-level translation uses one more walk level than a 39-bit layout;
- the transition temporarily maintains carefully controlled low and high
  aliases of the same kernel pages;
- a sparse direct map requires checked conversion rather than pointer addition
  alone;
- normal memory above 64 TiB needs a later mapping strategy; and
- the stable 2 GiB image slot postpones kernel-address randomization.

## Follow-Up Work

- define and test the exact AArch64 system-register transition sequence required
  by ADR-0011;
- encode address constants and canonical/range checks in architecture-owned
  declarations;
- make the Burrow linker and image verifier enforce the 2 GiB ELF virtual span
  and per-page permission rules;
- test both EL1 and EL2 firmware handoff paths through higher-half transfer and
  identity removal;
- add deliberate permission, guard-page, and stale-identity-access failures;
- reserve live boot resources before constructing the direct map; and
- require the future x86-64 port to document every numerical or semantic
  difference rather than inheriting this table silently.

## Revisit When

- a supported machine exposes required normal memory above 64 TiB;
- a concrete kernel facility cannot fit its assigned region without artificial
  fragmentation;
- kernel address randomization becomes a security requirement;
- AArch64 LPA/LPA2 support becomes a target requirement;
- userspace requirements demonstrate that the 48-bit half split is unsuitable;
  or
- the x86-64 portability audit reveals a shared layout that materially reduces
  architecture-specific complexity.
