# ADR-0006: CMake And Ninja Build System

- **Status:** Accepted
- **Date:** 2026-08-13
- **Owners:** Warren maintainers
- **Roadmap gate:** Phase 0 onward
- **Supersedes:** None
- **Superseded by:** None

## Context

Warren needs to build host tools, a UEFI application, a freestanding ELF kernel,
userspace artifacts, disk images, and host/emulator tests from an Apple Silicon
Mac. The build must express multiple target environments without inheriting the
macOS SDK accidentally. It should also provide a productive CLion workflow and
remain usable from a terminal and automated verification.

CMake is already familiar within BunnySoft and is supported directly by CLion.
Its defaults are oriented toward hosted applications, so Warren must define
cross-toolchain behavior deliberately rather than treating one global flag list
as an OS build system.

## Decision

Warren uses **CMake** as its reference build-description and orchestration system
and **Ninja** as its reference build executor.

The repository will provide checked-in `CMakePresets.json` profiles and explicit
toolchain files for distinct build environments. At minimum these environments
are separate:

- macOS host tools and host tests;
- AArch64 UEFI bootloader;
- AArch64 freestanding Burrow;
- AArch64 Warren userspace when it exists; and
- future x86-64 equivalents.

The Burrow toolchain file sets a non-host target, explicit compiler target,
freestanding compile/link policy, and static-library try-compiles where ordinary
CMake compiler probes would attempt to link a hosted executable. CMake package,
include, and library discovery is constrained so target builds cannot consume
macOS headers or libraries.

Targets own their compile definitions, options, include paths, link options, and
generated files. Global `CMAKE_CXX_FLAGS`-style mutation is avoided. Custom
commands assemble boot images and run QEMU, but their inputs, outputs, and tool
dependencies remain explicit.

CLion is a supported development interface, not a build dependency. Every
supported preset works from the command line, and automated tests use the same
presets or equivalent CMake interfaces.

The initial compiler-tool family is upstream LLVM/Clang plus LLD and LLVM binary
utilities, installed separately from Apple's platform linker. Exact versions and
paths are pinned after the first complete toolchain probe. Apple Clang may remain
a tested fallback compiler if it can satisfy the same target and link contracts.

CMake itself is not required to be Warren's eventual native self-hosting build
executor. Build metadata and generation dependencies must remain understandable
enough to port, translate, or replace when the self-hosting stage demands it.

## Alternatives Considered

### Hand-written Makefiles

They provide complete transparency and a small bootstrap story. Maintaining
multiple toolchains, generated artifacts, dependency graphs, tests, and CLion
configuration manually would create growing project-specific machinery.

### Meson and Ninja

Meson has strong cross-compilation concepts and clear syntax. It is less familiar
to the project and provides less direct value for the desired CLion workflow.

### Bazel

Bazel offers strong dependency modeling and hermetic-build capabilities. Its
operational and bootstrap weight is disproportionate for Warren's initial scale.

### A custom build system

This could eventually become an interesting native Warren tool. Building it now
would delay the operating system and force build-model decisions before the
artifact graph is known.

## Consequences

### Benefits

- first-class CLion project understanding and navigation;
- familiar, cross-platform build description;
- fast incremental execution through Ninja;
- checked-in presets provide stable command-line and IDE entry points; and
- distinct toolchain files make host/firmware/kernel/user boundaries visible.

### Costs And Risks

- CMake may run hosted compiler checks unless carefully constrained;
- cross-target flag leakage is easy if global variables are used;
- firmware, kernel, and image targets require more explicit modeling than normal
  applications;
- CMake version creep could harm portability and eventual bootstrap work; and
- IDE success can hide a broken clean command-line build if it is not tested.

### Follow-Up Work

- install and probe Ninja, upstream LLVM, LLD, and QEMU;
- select a conservative minimum CMake version based on features actually used;
- create toolchain files and presets with no machine-local absolute paths;
- verify host-header and host-library contamination fails loudly;
- document CLion profile selection; and
- add configure/build/test commands to automated verification.

## Revisit When

- CMake cannot express an artifact or dependency boundary without fragile hacks;
- CLion or automated builds diverge despite using the same presets;
- native Warren development reaches the point where another build executor is
  necessary; or
- the cost of maintaining CMake exceeds the migration cost to a demonstrated
  alternative.
