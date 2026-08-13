# ADR-0005: Warren-Owned Bootloader

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phases 0–1
- **Supersedes:** None
- **Superseded by:** None

## Context

Burrow needs a predictable initial environment, but firmware-facing details
should not shape kernel core. An existing general-purpose bootloader could load
the kernel quickly, but would make Warren's earliest machine contract depend on
another project's protocol and policies. Writing the bootloader is also a
deliberate part of the operating-system project.

The loader must remain bounded. It should normalize the firmware environment,
load Burrow, and get out of the way—not become a second kernel, hardware
abstraction layer, or permanent system service.

## Decision

Warren owns and builds its initial bootloader.

The first bootloader is a UEFI application for the AArch64 reference machine. It
is loaded from a FAT-formatted EFI System Partition using UEFI's standard file
facilities. It will:

1. establish diagnostic output suitable for loader failures;
2. locate and validate the Burrow ELF64 image;
3. allocate and load its declared segments;
4. gather and normalize required firmware information;
5. construct version 1 of Warren's boot-information structure;
6. obtain the final UEFI memory map and leave boot services correctly; and
7. transfer control under a documented architecture-specific entry contract.

After the handoff, Burrow does not call UEFI boot services. Firmware pointers and
UEFI structures do not enter kernel core; the bootloader translates them into a
versioned Warren-owned contract.

The bootloader and Burrow are separate build targets and binaries. Their shared
contract lives in an environment-neutral ABI header with layout tests. The
bootloader may contain architecture-specific front ends, but the protocol is
designed so a future x86-64 UEFI loader can produce the same logical handoff.

UEFI is the initial loader environment, not a permanent requirement for every
future Warren platform. A future loader may replace the UEFI front end without
redesigning Burrow's memory manager or kernel entry.

## Alternatives Considered

### Limine or another hobby-OS bootloader

This would accelerate first boot and provide a mature protocol. Warren would
delegate a deliberately desired part of the project and inherit an external
contract before defining its own kernel boundary.

### GRUB and Multiboot

GRUB is established and widely used. Its common workflows are strongest in the
x86 ecosystem, and its protocol would not remove the need to design Warren's
long-term architecture-neutral handoff.

### Direct QEMU kernel loading

This is convenient for experiments and may remain a diagnostic path. It is not
the reference boot path because it bypasses the firmware and boot-media contract
Warren intends to own.

### Burrow as a UEFI application

This removes a binary and can reach kernel code quickly. It merges firmware
lifecycle, PE/COFF entry, image loading, memory-map finalization, and kernel
ownership into one artifact, making the boundary harder to replace and test.

## Consequences

### Benefits

- Warren fully specifies the transition into Burrow;
- boot diagnostics and validation can serve Warren's exact needs;
- UEFI-specific details remain outside kernel core;
- future architecture and loader paths can target one logical handoff; and
- loading, ELF, memory-map, and ABI behavior become understood project assets.

### Costs And Risks

- First Light requires a correct UEFI application and image pipeline;
- leaving boot services has ordering and retry subtleties that require tests;
- the loader must handle malformed images and firmware data defensively;
- PE/COFF generation is an additional toolchain concern; and
- scope creep could turn the loader into an unnecessary policy engine.

### Follow-Up Work

- select the bootloader's freestanding language subset and UEFI headers;
- specify boot-information protocol version 1;
- specify Burrow's virtual and physical image layout;
- decide whether ELF relocation is initially supported or forbidden;
- build the EFI System Partition deterministically; and
- test bootloader failures independently from Burrow failures.

## Revisit When

- a supported non-UEFI platform requires another front end;
- the bootloader accumulates policy better owned by Burrow or userspace;
- UEFI behavior prevents a reliable reference boot; or
- an external protocol becomes a valuable compatibility path in addition to the
  Warren-native loader.
