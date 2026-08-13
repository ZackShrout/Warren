# BunnySoft Warren Coding Standards

**BunnySoft**  
**Warren Project Coding Standards**  
**Effective: August 2026**

---

## Purpose And Scope

This document adapts the BunnySoft official coding standards for freestanding
operating-system development. It applies to Burrow, Warren userspace, Warren SDK,
boot code, architecture assembly, and Warren-owned host tools unless a narrower
documented exception exists.

The parent standard remains the source of the project's naming character. This
file takes precedence where hosted C++ assumptions conflict with a kernel,
firmware, ABI, or future self-hosting requirement.

Language baselines:

- Burrow and Warren-owned C++ target code: **freestanding C++20 subset**
- Deliberate C target code and bootstrap/runtime components: **C17**
- AArch64 assembly: GNU/Clang integrated-assembler syntax
- Future x86-64 assembly: GNU/Clang integrated assembler in explicitly declared
  Intel syntax unless an ADR selects otherwise
- Host tools: C++20 by default; a newer standard requires a documented reason

Project root namespaces:

- Burrow kernel: `burrow`
- Warren system libraries and SDK: `warren`
- Product components use a component namespace under the appropriate root unless
  their public API justifies a separate root.

## Core Principles

- Optimize for readability and auditable behavior first.
- Prefer consistency over cleverness.
- Make ownership, units, privilege, and address-space meaning explicit.
- Keep public and ABI-facing interfaces polished, small, and predictable.
- Favor simple C++ and visible mechanisms over abstraction-heavy style.
- Treat undefined behavior as a correctness defect, not an optimization tool.
- Do not hide architecture dependencies behind vague names or preprocessor fog.
- Every early-boot facility must state which prerequisites it needs.
- Code that cannot fail safely must fail observably.

## File And Folder Naming

### C++ Folder Naming

C++ source folders use `UpperCamelCase`:

- `Core/`, `Memory/`, `Process/`, `Filesystem/`
- `Arch/AArch64/`, `Arch/X86_64/`
- `Platform/QemuVirt/`

Non-code and repository-organization folders may use conventional lowercase:

- `docs/`, `tools/`, `tests/`, `assets/`, `config/`

`detail/` is the one intentional lowercase implementation-detail folder inside
a public include tree. Do not mix `internal/` and `detail/` for the same role.

### File Names

- C++ headers: `UpperCamelCase.h`
- C++ sources: `UpperCamelCase.cpp`
- C headers and sources, when used: `UpperCamelCase.h`, `UpperCamelCase.c`
- Preprocessed assembly: `UpperCamelCase.S`
- Non-preprocessed assembly: `UpperCamelCase.s`
- Linker scripts: descriptive lowercase names ending in `.ld`
- Build/tool scripts: descriptive `snake_case`
- Markdown documents: `UPPER_SNAKE_CASE.md` for project policy and
  `kebab-case.md` where an external convention applies, such as ADRs

Use `.S` only when the C preprocessor is intentionally required. Assembly that
does not need preprocessing uses `.s` so its true inputs remain visible.

### Source File Header

Every Warren-owned C, C++, and assembly source or header begins with the
following ownership header, using the file's actual creation date:

```cpp
//
// Created by Zack Shrout on M/D/YY.
// Copyright (c) YYYY BunnySoft. All rights reserved.
//
```

The copyright year matches the creation-date year. Assembly files use the
appropriate assembler comment delimiter while preserving the same text and
four-line shape. Generated files identify their generator and provenance
instead; third-party files retain their upstream headers and are not rewritten.

## Naming Conventions

- User-defined C++ types use `snake_case_t`.
- Functions, variables, parameters, and namespaces use `snake_case`.
- Private and protected data members use `_snake_case`.
- Compile-time constants use `k_snake_case`.
- File-scope mutable globals, when unavoidable, use `g_snake_case`.
- Macros use `UPPER_SNAKE_CASE` and normally include a project/component prefix.
- Assembly local labels use descriptive `snake_case` where the assembler permits.
- Assembly symbols visible outside one file use a qualified prefix such as
  `burrow_aarch64_` or `warren_boot_`.

Examples:

```cpp
namespace burrow::memory {

using page_count_t = strong_count_t<page_tag_t>;

struct address_space_t;

[[nodiscard]] result_t map_pages(address_space_t& space,
                                 virtual_address_t address,
                                 page_count_t count) noexcept;

} // namespace burrow::memory
```

Names must communicate units and address domains. Prefer
`physical_address`, `virtual_address`, `byte_count`, and `page_count` over
generic names such as `address`, `size`, or `count` at low-level boundaries.

## Namespace Rules

- All project-owned C++ lives in an approved root namespace.
- Sub-namespaces express ownership without excessive nesting.
- Anonymous namespaces are encouraged in `.cpp` files for file-local symbols.
- Never use `using namespace` in a header.
- `using` declarations in `.cpp` files are allowed when narrow and unambiguous.
- Do not mirror every folder with a namespace mechanically when it adds no
  useful ownership information.
- No C++ namespace or mangled symbol crosses an assembly, firmware, syscall,
  persistent-format, or public C ABI boundary.

## Core C++ Style

- Use uniform/braced initialization everywhere practical.
- Float literals always carry an appropriate suffix: `0.f`, `1.f`.
- Use `auto` only when the type is long and obvious, when iterating containers,
  or when it avoids repeating a template expression without hiding pointer,
  reference, unit, signedness, or ownership semantics.
- Single-statement `if`, `for`, and `while` bodies do not use braces.
- Multi-statement blocks always use braces.
- Braces use Allman style.
- Apply `constexpr`, `const`, `noexcept`, and `[[nodiscard]]` wherever they make a
  real contract visible.
- `goto` is forbidden except for a deliberate low-level cleanup/unwind path that
  is clearer than duplicating cleanup.
- Use `nullptr`, never `0` or `NULL`, for null pointers.
- Prefer `static_cast`, `const_cast`, and `reinterpret_cast` over C-style casts.
  Every `reinterpret_cast` deserves local scrutiny.
- Avoid comma expressions, implicit fallthrough, and assignment in conditions.
- Mark deliberate switch fallthrough with `[[fallthrough]]`.

Examples:

```cpp
if (is_ready) return success();

for (uint32_t index{ 0 }; index < entry_count; ++index)
    validate_entry(entries[index]);
```

```cpp
if (!mapping.is_valid())
{
    log_error("vm", "mapping validation failed");
    return error_t::invalid_mapping;
}
```

## Freestanding C++ Subset

The compiler accepting a feature does not make that feature available in
Burrow. Kernel code must not depend on invisible hosted runtime behavior.

### Required Compiler Configuration

Burrow is compiled as freestanding and without:

- C++ exceptions;
- RTTI;
- host startup files;
- host C or C++ standard libraries; and
- platform-default target, SDK, or deployment settings.

The selected flags are encoded in the cross-toolchain configuration and checked
by the link. Source code does not attempt to support both exception and
non-exception kernel modes.

### Permitted With Normal Review

- namespaces, scoped enumerations, references, and function overloading;
- constructors and destructors with explicit, local lifetime;
- `constexpr`, `consteval`, and compile-time assertions;
- bounded templates that make types or units safer;
- non-capturing lambdas where they remain clearer than a named helper;
- RAII for resources whose cleanup is valid in the current execution context;
- placement construction through a Warren-owned facility; and
- virtual dispatch only when a concrete interface benefits and its object
  lifetime, vtable emission, and failure behavior are understood.

### Forbidden In Burrow Until Explicitly Enabled

- `throw`, `try`, and `catch`;
- `dynamic_cast` and `typeid`;
- C++20 coroutines;
- C++ modules;
- dynamically initialized namespace-scope objects;
- function-local static initialization requiring guard functions;
- `thread_local` before Burrow owns a tested TLS runtime;
- raw `new` and `delete`;
- iostreams, locale, filesystem, or other hosted-library facilities;
- implicit allocation in foundational containers; and
- compiler extensions that alter ABI or semantics without an accepted decision.

The standard library is unavailable in Burrow unless a specific header/facility
has been deliberately supplied and documented by Warren. Do not include a host
header because it “happens to be header-only.”

Userspace and host tools may adopt broader facilities within the runtime they
actually possess. Code shared with Burrow follows the stricter rule.

## Type Discipline

- Prefer `struct` over `class`; use `class` when private invariants or inheritance
  actually justify it.
- Prefer free functions when behavior does not require private object state.
- Use fixed-width integer types for registers, protocols, persistent formats,
  firmware structures, and ABI data.
- Use explicit strong types for physical addresses, virtual addresses, byte
  counts, page counts, object handles, and identifiers when conversion would be
  dangerous.
- Plain `int` is acceptable for local arithmetic only when its range and ABI do
  not matter.
- Never store an address in a type that might be narrower than a pointer.
- Specify the underlying integer type of every ABI-facing `enum class`.
- Do not use `bool`, compiler enums, bit-fields, or C++ object layout directly in
  a binary protocol.
- Signed/unsigned conversions at boundaries are explicit and range-checked.
- Avoid packed structs. Decode external bytes field by field when practical.
- If exact layout is necessary, use explicit padding plus `static_assert` checks
  for size, alignment, and offsets.

## Struct And Class Layout

Use this order:

```cpp
struct foo_t
{
public:
    // public methods
    // public data only for deliberate plain data carriers

protected:
    // protected methods
    // protected data

private:
    // private methods
    // private data
};
```

Write `public:` explicitly even for `struct` unless the type is a tiny pure data
carrier. Omit unused sections. Keep invariants near the fields or operations
that enforce them.

## Pointers, References, And Const

- Pointers and references attach to the type: `page_t* page`, `frame_t& frame`.
- East-const style is not used.
- A non-null borrowed object normally uses a reference.
- A nullable borrowed object uses a pointer and documents the null meaning.
- Owning pointers use an ownership type or a clearly named subsystem primitive.
- Pointer arithmetic is confined to code whose byte/object bounds are explicit.
- Never cast away `const` to mutate originally constant storage.

## Error Handling

- Burrow uses explicit error returns, result types, or unambiguous fatal paths.
- No kernel interface relies on exceptions.
- `[[nodiscard]]` is required for error/result values whose loss would hide a
  failed operation.
- Error enums have stable meanings within their documented boundary.
- Do not collapse distinct hardware, validation, exhaustion, and permission
  failures into a generic failure before logging or returning useful context.
- Panics indicate violated kernel invariants or unrecoverable foundational state,
  not routine bad user input or ordinary resource exhaustion.
- Assertions never replace validation of firmware, devices, files, syscalls, or
  user pointers.
- A failure path must not allocate or acquire a lock unless its contract allows it.

## Memory And Resource Management

- Ownership is explicit in types, names, and subsystem contracts.
- RAII is encouraged only when destruction is valid at every scope-exit site,
  including interrupt and error paths.
- Early-boot code states which allocator stage it requires.
- Allocation failure is handled where exhaustion is recoverable.
- Destructors must not fail silently, block unexpectedly, or perform unbounded
  work.
- Resources owned by a process are reclaimable on process termination.
- MMIO pointers and ordinary memory pointers are not interchangeable abstractions.
- `volatile` expresses required accesses to an object; it is not a synchronization
  primitive or a substitute for atomics and memory barriers.

## Concurrency And Interrupt Context

- Shared mutable state has a named owner and synchronization strategy.
- Use Warren atomic and lock primitives with documented memory ordering.
- Do not introduce ad hoc compiler barriers or architecture instructions in
  shared code.
- Interrupt masking is limited to architecture primitives and narrowly justified
  critical sections.
- Functions callable in interrupt context are marked or documented and may not
  allocate, sleep, or take incompatible locks.
- Interrupt handlers perform bounded work and defer expensive processing.
- Lock ordering is documented before code can hold multiple locks.
- Never invoke unknown callbacks while holding a lock unless the interface
  explicitly requires and documents it.
- Per-CPU data must not be simulated with an unexplained global even while the
  reference machine has one CPU.

## Header Rules

- Use `#pragma once` for C++ headers.
- Headers include what they require and compile in isolation.
- Prefer forward declarations when they preserve clarity and do not duplicate an
  ABI-sensitive declaration.
- Never use `using namespace` in a header.
- Avoid inline implementation that expands dependency or rebuild scope without a
  compile-time or clarity benefit.
- Architecture-neutral headers may not include architecture or platform headers.
- Public SDK headers may not expose Burrow-private types.
- C ABI headers use appropriate `extern "C"` guards and remain valid for their
  claimed C language level.

Include order in C++ source files:

1. matching header;
2. Warren/Burrow public or subsystem headers;
3. architecture/platform headers when the file belongs to that layer;
4. approved runtime or third-party headers.

Groups are separated by one blank line. Includes are sorted consistently within
a group.

## ABI Boundary Rules

- Exported low-level symbols have one canonical declaration in a shared header.
- C++ declarations implemented in assembly use `extern "C"`.
- Assembly never guesses C++ mangled names.
- ABI structures use fixed-width fields, explicit alignment, and layout checks.
- Pass structured boot/syscall data using pointers plus explicit size/version
  information rather than a compiler-specific aggregate calling convention.
- Never expose a kernel pointer, C++ vtable, reference, exception, or STL/Warren
  container representation across a syscall or persistent boundary.
- The stack is aligned according to the architecture ABI at every call boundary.
- Callee-saved registers are preserved exactly as required.
- Functions that intentionally diverge from the platform ABI are assembly-only
  transition mechanisms and document their entry and exit state completely.

## Assembly Standards

Assembly is reserved for code that cannot be expressed safely and clearly in
C++: initial entry, exception vectors, context switching, selected register and
barrier operations, and tightly justified primitives.

Assembly that implements a routine, transition, or non-trivial instruction
sequence lives in a dedicated architecture-owned `.s` or `.S` file. AArch64 and
x86-64 implementations are separate source files selected by the build; one
assembly file must not grow into a preprocessor-selected multi-architecture
implementation.

Ordinary algorithms remain in C++ even when Warren must supply them itself.
Allocators, object management, containers, parsers, and policy code do not
become assembly merely because the host runtime is unavailable. If later
profiling justifies an architecture-optimized primitive such as `memcpy`, the
portable implementation remains the behavioral reference and each optimized
implementation is isolated behind the same tested interface.

### File Structure

Each assembly file begins with comments stating:

- purpose and owning layer;
- expected execution/privilege mode;
- entry register and stack conditions;
- symbols exported or imported;
- registers clobbered beyond the standard ABI;
- whether interrupts may be enabled; and
- whether the code may access memory or call C++.

Then organize the file as applicable:

1. syntax/architecture directives;
2. constants and structure offsets;
3. section declarations and alignment;
4. exported symbols and type metadata;
5. implementation; and
6. non-executable data.

### Formatting And Naming

- Instructions, registers, and assembler directives use lowercase.
- Labels begin in column zero.
- Instructions and operands are indented by four spaces.
- One instruction appears per line.
- Use descriptive labels; numeric local labels are allowed only for tiny,
  visually local forward/backward branches.
- Exported symbols use a component and architecture prefix.
- Mark function symbols and sizes where the object format supports them.
- State section permissions explicitly; executable stacks are forbidden.
- Prefer named constants/offsets over unexplained literals.

AArch64 example:

```asm
/* EL1 helper. x0 contains the value to write. No stack access. */
.section .text.burrow_aarch64_write_vbar, "ax", %progbits
.align 2
.global burrow_aarch64_write_vbar
.type burrow_aarch64_write_vbar, %function
burrow_aarch64_write_vbar:
    msr vbar_el1, x0
    isb
    ret
.size burrow_aarch64_write_vbar, . - burrow_aarch64_write_vbar
```

### AArch64-Specific Rules

- Follow AAPCS64 at every ordinary call boundary.
- Keep the stack pointer 16-byte aligned whenever the ABI requires it; no
  temporary misalignment may cross a call.
- Preserve `x19`–`x29` and any other ABI-required state in callable functions.
- Treat `x18` as platform-reserved unless an accepted target ADR says otherwise.
- Use architecture-defined `dsb`, `dmb`, and `isb` scopes intentionally and
  comment why the chosen ordering/scope is sufficient.
- System-register writes that require synchronization include the required
  barrier sequence adjacent to the write.
- Exception entry saves a documented frame before calling C++ and never assumes
  the interrupted context already has a kernel-safe stack.
- Vector-table alignment and slot-size requirements are compile-time/link-time
  asserted where possible.

### Future x86-64 Rules

- Declare `.intel_syntax noprefix` explicitly in each Intel-syntax file.
- Follow the selected x86-64 ABI at C++ call boundaries.
- Kernel code disables reliance on the red zone.
- The direction flag is clear at ABI boundaries.
- Interrupt/trap stubs normalize error-code and non-error-code frames before
  shared handling.
- Stack alignment accounts for CPU-pushed frames before a C++ call.
- Control-register, MSR, invalidation, and barrier behavior remains confined to
  the architecture layer.

### Preprocessed Assembly

- `.S` files use the preprocessor only for target constants, generated offsets,
  or intentionally shared macros.
- Prefer `/* ... */` comments in `.S` files so preprocessor and assembler comment
  syntax cannot conflict.
- Preprocessor conditionals select architecture features or generated layouts;
  they do not create unreadable multi-architecture instruction bodies.
- Generated offsets are produced from canonical C++ layout definitions and
  validated, never duplicated manually without an assertion.

### Inline Assembly

Inline assembly is discouraged. Prefer an out-of-line architecture function when
it creates a testable boundary.

Inline assembly is acceptable for a tiny, named architecture-layer wrapper
around an isolated instruction when an intrinsic is unavailable or less clear—for
example `wfe`, a system-register read, or a precisely scoped barrier. It is not
used to implement loops, allocation algorithms, exception entry, context
switches, calling-convention transitions, or multi-step machine-state changes.

When inline assembly is truly clearer:

- keep it minimal;
- use correct input, output, early-clobber, and tied constraints;
- list every clobbered register and condition-code effect;
- use a `memory` clobber only when a compiler memory barrier is actually part of
  the contract;
- never rely on surrounding C++ locals occupying particular registers; and
- wrap it in a named architecture-layer function that documents ordering and
  privilege requirements.

## Linker Script Standards

- Linker scripts are target code and receive review and tests.
- Every memory region, output section, permission, and alignment has a stated
  reason.
- Exported linker symbols use a qualified prefix and represent one unambiguous
  address or size unit.
- Use assertions for alignment, vector placement, image bounds, and forbidden
  dynamic/runtime sections.
- Keep debug sections in the unstripped ELF as appropriate without mapping them
  into runtime memory.
- Discard sections explicitly and narrowly; never hide an unexpected section
  with a broad wildcard merely to make the image link.
- Changes to image layout update the boot contract, debugger workflow, and map
  tests together.

## C Source Rules

Warren-owned C is used deliberately for bootstrap code, C runtime boundaries, or
third-party integration—not as an accidental fallback from C++.

- Compile as C17 with strict warnings.
- Use fixed-width types and explicit ownership rules identical to C++ boundaries.
- Public names use the appropriate `warren_` or `burrow_` prefix.
- Do not emulate C++ with opaque macro systems.
- Cleanup `goto` paths are allowed when they provide one clear reverse-order
  cleanup block.
- Headers shared with C++ include tested language guards.
- C and C++ implementations of the same ABI use layout assertions in both build
  environments where practical.

## Macros And Conditional Compilation

- Prefer typed `constexpr` values and functions in C++.
- Macros that evaluate arguments more than once are forbidden.
- Feature/configuration macros are centralized and include a project prefix.
- `#ifdef` architecture selection belongs at source/build boundaries, not
  scattered through kernel algorithms.
- Build configuration must not change public structure layout silently.
- Debug-only macros preserve expression side effects exactly or reject side-
  effecting arguments.

## Logging, Assertions, And Comments

- Logs identify subsystem and severity without requiring heap allocation.
- Never log secrets or arbitrary unvalidated user memory.
- Early logs avoid facilities not yet initialized.
- Assertions state invariants; messages include the values needed to investigate.
- Comments explain why, invariants, hardware rules, and non-obvious ordering.
- Do not translate an instruction into prose directly above the instruction.
- `TODO` and `FIXME` comments include the missing contract or removal gate; avoid
  ownerless wishes such as `TODO: improve`.
- Temporary architecture or privilege shortcuts cite the roadmap phase that
  removes them.

## Documentation And Review Requirements

Public or cross-subsystem interfaces document:

- ownership and lifetime;
- valid execution contexts;
- allocation and blocking behavior;
- synchronization requirements;
- units and address domains;
- errors and fatal conditions; and
- stability/ABI status.

A foundational change is incomplete without the smallest practical test and any
required ADR or architecture-document update.

## Example Kernel Style

```cpp
#pragma once

#include <burrow/Types.h>

namespace burrow::memory {

enum class map_error_t : uint8_t
{
    invalid_alignment,
    already_mapped,
    out_of_memory,
};

struct mapping_request_t
{
public:
    virtual_address_t virtual_address;
    physical_address_t physical_address;
    page_count_t page_count;
    page_permissions_t permissions;
};

[[nodiscard]] result_t<void, map_error_t>
map_pages(address_space_t& address_space,
          const mapping_request_t& request) noexcept;

} // namespace burrow::memory
```

The concrete Warren utility and result types in this example are illustrative;
their presence here does not authorize creating them before their requirements
and ownership are defined.

## Adoption Guidance

- New Warren code follows this standard from its first commit.
- Imported code remains visibly third-party and may retain upstream style.
- Do not mechanically restyle third-party code or mix it into Warren-owned
  directories.
- Exceptions require a local comment for a narrow case or an ADR for a recurring
  architectural rule.
- Avoid broad style churn unrelated to the current capability milestone.
- When this document conflicts with a required platform ABI, the ABI wins and the
  exception is documented beside the boundary.
