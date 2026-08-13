# Warren Self-Hosting Strategy

Self-hosting is one of Warren's north-star goals. It is also a collection of
dependencies that must be built in order, not a binary feature.

## Definitions

- **Cross-built:** a tool running on macOS produces a Warren target artifact.
- **Native tool:** a tool running on Warren produces or manipulates Warren
  artifacts.
- **Native development capable:** Warren can edit, compile, link, execute, and
  debug a supported application without host intervention.
- **Partial self-host:** Warren can build a named subset of its own system from
  source on Warren.
- **Full self-host:** a documented Warren environment can build the complete
  release artifact graph claimed by the project, including required native
  tools, without an opaque host-side build step.
- **Reproducible self-host:** the artifact relationship between bootstrap and
  rebuilt outputs is measured and satisfies a documented reproducibility rule.

These phrases must always name the achieved stage and artifact set.

## Bootstrap Ladder

### Stage 0 — macOS cross-toolchain

Clang/LLVM, the linker, image tools, and QEMU run on macOS. Warren supplies an
explicit target definition and sysroot rather than inheriting macOS defaults.

Evidence:

- freestanding Burrow image builds without host headers or libraries;
- trivial Warren user objects can be inspected as the intended ELF target; and
- every non-Warren symbol in a final image is accounted for.

### Stage 1 — Warren SDK sysroot

Warren publishes target headers, ABI definitions, startup objects, system
libraries, and toolchain configuration as a versioned sysroot.

Evidence:

- an application builds from macOS using only documented SDK inputs;
- accidental host includes fail; and
- ABI fixtures validate type sizes, alignments, calling convention, and startup.

### Stage 2 — Native tools that reduce the gap

Port small, separable tools before a compiler: file utilities, archive handling,
assembler or object inspection, linker as appropriate, and build-description
execution. A host-side tool may remain canonical while its native replacement
is tested.

Evidence names each native tool and the remaining host dependencies.

### Stage 3 — Small native C compiler

Port a compact, well-understood C compiler capable of building ordinary Warren C
programs. This proves the execution, filesystem, memory, process, and SDK layers
without immediately requiring Warren to host the compiler used for Burrow's C++
subset.

Evidence:

- multi-file compile and link on Warren;
- meaningful diagnostics;
- produced objects pass ABI tests; and
- the same source passes behavioral comparison against the cross-built form.

### Stage 4 — Native C development environment

Add the editor, build executor, debugger path, source-control or source-import
path, test runner, and package metadata needed for repeatable work rather than a
demo.

Evidence is the complete edit-build-debug-test loop documented in Phase 5.

### Stage 5 — Native C++ toolchain

Port or bootstrap a compiler that supports Warren's required freestanding C++20
subset, together with the assembler, linker, compiler runtime, and the subset of
the C++ runtime or library needed by target components.

This may be an LLVM/Clang port, a GCC port, another conforming compiler, or a
staged compiler chosen when Warren's userspace can support it. Warren does not
commit now to writing an optimizing C++ compiler from scratch.

Evidence includes compiler torture/ABI tests and comparison with cross-built
objects at semantic boundaries.

### Stage 6 — Partial Warren self-build

Build named userland libraries, Hare, services, tools, and selected Burrow
objects on Warren. Image assembly may initially remain cross-hosted if its exact
remaining role is disclosed.

### Stage 7 — Release self-host

Build the documented Warren release graph, run its target tests, assemble its
images and packages, and record provenance on Warren.

This stage requires a bootstrap story: which trusted binary seed is used, how it
is verified, and how successive compiler generations relate.

## Compatibility Surface Needed By Tools

Toolchain ports generally need more than `open`, `read`, and `write`. Warren
should expect requirements in these categories:

- hierarchical files, temporary files, atomic replacement, and metadata;
- processes, arguments, environment, exit status, pipes, and redirection;
- virtual memory and sufficiently capable allocation;
- clocks and monotonic timing;
- signals or a deliberate substitute for interruption and child notification;
- terminal behavior;
- thread-local storage and threading for larger tools;
- dynamic loading only if the chosen toolchain configuration requires it; and
- build-time scripting or a constrained portable replacement.

The project will maintain a **tool-port ledger** once ports begin. Each tool's
missing interfaces become evidence for userspace priorities; they do not
automatically become kernel system calls.

## libc And C++ Runtime Strategy

The initial target runtime is layered:

1. compiler runtime helpers explicitly required by generated code;
2. freestanding memory/string primitives;
3. syscall wrappers and startup/termination objects;
4. a small Warren C library surface;
5. expanded POSIX-shaped interfaces driven by real ports; and
6. C++ runtime and standard-library support driven by the native compiler plan.

Burrow does not link the userspace libc or C++ standard library. Shared source is
allowed only where its environmental assumptions are explicit.

Whether Warren adopts an existing libc, grows its own, or combines a thin Warren
system layer with an established libc is a future ADR. “Write all of libc” is not
assumed to be a virtue.

## Build-System Constraint

The macOS build may initially use tools unavailable on Warren. However:

- target descriptions and source lists must be data that can be consumed or
  translated without executing arbitrary macOS-only code;
- generated sources require checked-in generators or documented bootstrap
  outputs;
- essential generation steps cannot depend forever on an unportable proprietary
  application; and
- native self-build scope must state which build executor and scripting runtime
  it requires.

Early productivity is allowed. Hidden permanent host dependence is not.

## Avoiding The Bootstrap Trap

- Keep pre-generated target tables only when their generator and provenance are
  retained.
- Preserve a cross-build path even after native tools exist; it is a recovery and
  comparison mechanism.
- Separate compiler runtime, libc, C++ ABI runtime, and standard library so their
  bootstrap order remains visible.
- Avoid requiring dynamic linking until static tools can establish the system.
- Do not make the package manager a prerequisite for reconstructing the package
  manager's first trusted installation image.
- Keep release-image construction specified independently from one host shell.

## Self-Hosting Scorecard

At each release claiming native-development progress, report:

| Capability | Cross-hosted | Native | Reproducible | Notes |
| --- | --- | --- | --- | --- |
| Preprocessor/compiler |  |  |  |  |
| Assembler |  |  |  |  |
| Linker |  |  |  |  |
| Archiver/object tools |  |  |  |  |
| libc/runtime |  |  |  |  |
| Build executor |  |  |  |  |
| Editor |  |  |  |  |
| Debugger/symbolizer |  |  |  |  |
| Test runner |  |  |  |  |
| Package/image tools |  |  |  |  |
| Burrow |  |  |  |  |
| Warren userland |  |  |  |  |

Blank cells mean “not demonstrated,” not “probably works.”
