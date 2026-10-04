# Warren Test-Result Protocol V1

**Status:** Accepted normative specification under ADR-0012

**Protocol version:** 1

## 1. Purpose And Scope

This protocol lets one trusted Warren-built QEMU system-test image report one
unambiguous terminal result to its host harness. It coordinates:

- an ASCII PL011 serial record for diagnosis and machine parsing; and
- an AArch64 semihosting exit status that terminates the reference TCG process.

Neither channel is sufficient by itself. A serial pass followed by a hang is a
timeout, not a pass. A nonzero QEMU process status without a matching terminal
record is an emulator/process failure, not proof that a Warren test deliberately
failed.

Version 1 governs Phase 1 and later reference TCG system tests. It does not
define ordinary Warren shutdown, a physical-machine interface, a syscall, or an
SDK service.

## 2. Serial Encoding

Every protocol record:

- begins at column zero with the bytes `WARREN_TEST:`;
- contains only 7-bit ASCII;
- contains no spaces, tabs, escaping, quoting, or NUL byte;
- ends with LF; a producer may emit CR immediately before that LF;
- is no more than 96 bytes including CR/LF; and
- is emitted as one complete line.

Other serial lines are diagnostic text and are ignored by the protocol parser.
A line beginning with `WARREN_TEST:` but not matching the version 1 grammar is a
protocol error rather than ordinary diagnostic text.

The test identifier contains 1–64 characters. Its first character and every
subsequent character belong to:

```text
lowercase ASCII letter  a-z
ASCII digit             0-9
hyphen                  -
underscore              _
period                  .
```

Colons are forbidden in identifiers, so version 1 requires no escaping. Test
identifiers are case-sensitive. The canonical project style uses lowercase
hyphen-separated names such as `burrow-first-light`.

## 3. Record Grammar

The four record forms are:

```text
WARREN_TEST:1:BEGIN:<test-id>
WARREN_TEST:1:PASS:<test-id>
WARREN_TEST:1:FAIL:<test-id>:<result-code>
WARREN_TEST:1:PANIC:<test-id>:<result-code>
```

Equivalent anchored regular expressions, excluding the line terminator, are:

```text
^WARREN_TEST:1:(BEGIN|PASS):([a-z0-9._-]{1,64})$
^WARREN_TEST:1:(FAIL|PANIC):([a-z0-9._-]{1,64}):([1-9][0-9]{0,2})$
```

The decimal result code is canonical: it has no sign, prefix, whitespace, or
leading zero. The parsed value must be between 1 and 119 inclusive and must be
valid for the selected terminal record.

`BEGIN` announces the logical test invocation. `PASS`, `FAIL`, and `PANIC` are
terminal records. Detailed human-readable failure text is emitted on ordinary
serial lines before the terminal record; it is not added as an unbounded field
to the machine grammar.

## 4. Result-Code Allocation

The semihosting application status and serial terminal result describe the same
numeric result:

| Code or range | Serial terminal | Meaning | Host class |
| ---: | --- | --- | --- |
| `0` | `PASS` | Test completed successfully | pass |
| `1` | `FAIL` | Generic explicit test failure | explicit failure |
| `2` | `PANIC` | Assertion or invariant failure | panic/assertion |
| `3` | `PANIC` | Kernel panic | panic/assertion |
| `4` | `PANIC` | Unhandled architectural exception | panic/assertion |
| `5` | `FAIL` | Guest test infrastructure failed | explicit failure |
| `6`–`63` | — | Reserved for future protocol-wide results | reserved |
| `64`–`95` | `FAIL` | Test-specific explicit failure | explicit failure |
| `96`–`119` | — | Reserved for future guest results | reserved |
| `120`–`255` | — | Forbidden to version 1 guests | host/process space |

`PASS` omits the redundant textual code and always means code 0. A version 1
producer never emits a reserved or forbidden code. Assigning a new common code
requires a compatible protocol specification update; individual tests may
allocate meanings within 64–95 and document them beside the test.

The 0–119 guest range avoids collision with Warren harness statuses and common
command-runner statuses. It also remains fully observable through the process
status behavior used by the supported macOS host environment.

## 5. Record Ordering

One QEMU process runs one logical protocol invocation:

1. `BEGIN` appears exactly once before ordinary test work.
2. `PASS` or `FAIL` appears exactly once after that `BEGIN`, with the identical
   test identifier.
3. `PANIC` normally follows a matching `BEGIN`, but may be the only record when
   an emergency path fails before the normal begin point.
4. A terminal record is the final Warren test record. No later `BEGIN`, `PASS`,
   `FAIL`, or `PANIC` is valid.
5. The complete terminal line is synchronously emitted and drained before the
   guest requests semihosting exit.

Nested, concurrent, or repeated invocations are not represented in version 1.
A system-test executable may run many internal checks, but it aggregates them
under one protocol identifier and one terminal result. Separate protocol-level
tests use separate QEMU processes.

## 6. AArch64 Semihosting Exit

Reference TCG test images are built with test-only QEMU platform support and run
with native semihosting explicitly enabled. The terminal path performs the Arm
semihosting `SYS_EXIT_EXTENDED` operation:

```text
x0 = 0x20                         SYS_EXIT_EXTENDED
x1 = address of argument block

argument_block[0] = 0x0000000000020026  ADP_Stopped_ApplicationExit
argument_block[1] = result code         0 through 119

HLT #0xF000
```

The argument block contains two naturally aligned 64-bit little-endian words
and remains readable until QEMU handles the trap. The operation is not expected
to return. A return enters the panic path without attempting a second
semihosting exit.

Warren uses no semihosting file, console, command-line, clock, or host-system
operation. The QEMU profile uses native handling rather than redirecting the
call through a debugger. Semihosting is enabled only for a trusted, explicitly
test-enabled image under TCG.

## 7. Channel Agreement

For a terminal result to be authoritative:

- the serial stream contains one valid terminal record;
- its record kind permits its result code;
- the QEMU process terminates before the timeout; and
- the QEMU process status equals the serial result code.

`PASS` therefore requires both a valid serial pass record and process status 0.
`FAIL` and `PANIC` require their exact nonzero code to be observed as the process
status. A valid record whose code disagrees with the process status is a protocol
error, never a partial success.

The host never infers a Warren result solely from a QEMU status. This prevents a
QEMU initialization failure, signal, abort, or unrelated process error from
masquerading as an intentional guest result.

## 8. Host Classification And Precedence

The harness produces one classification using this precedence:

1. failure to launch the requested process is `launch error`;
2. exceeding the deadline is `timeout/hang`, even if a pass line appeared;
3. an overlong, non-ASCII, malformed, misordered, duplicate, or mismatched
   Warren record is `protocol error`;
4. a valid terminal record whose result disagrees with QEMU is `protocol error`;
5. a matching `PASS`/0 pair is `pass`;
6. a matching `FAIL`/permitted-code pair is `explicit failure`;
7. a matching `PANIC`/2–4 pair is `panic/assertion`;
8. a nonzero or signaled QEMU termination without a valid terminal record is
   `QEMU process failure`; and
9. a clean QEMU termination without a terminal record is `protocol error`.

The harness command uses these stable statuses:

| Harness status | Classification |
| ---: | --- |
| `0` | pass |
| `1` | explicit test failure |
| `2` | panic/assertion |
| `3` | protocol error or channel disagreement |
| `4` | QEMU process failure |
| `124` | timeout/hang |
| `125` | launch or host-harness error |

The harness prints captured output and its classification on every non-pass
path. It preserves the guest result code as diagnostic metadata even though the
harness status reports the broader class.

## 9. AArch64 Normalized-Entry System Test

The combined UEFI/Burrow system image emits:

```text
WARREN_TEST:1:BEGIN:aarch64-normalized-entry
WARREN_TEST:1:PASS:aarch64-normalized-entry
```

The loader emits `BEGIN` before opening the packaged runtime ELF. Burrow emits
`PASS` only after the image has been validated and materialized, the final boot
information object has been built and validated, UEFI boot services have ended,
the AArch64 handoff has installed the declared stack and registers, and the
first-entry witness has checked the directly observable contract. Burrow must
then remove inherited firmware translation, reach common MMU-off EL1h, build
and audit its fixed-capacity tables, activate the owned regime, transfer to the
higher half, remove identity mappings, and receive the exact retained witness
result from architecture-neutral C++. Before publishing that witness, Core must
emit `BURROW_CONSOLE:driver=pl011:mode=polling:output=ready` through the
platform-selected production writer. Burrow then
requests `SYS_EXIT_EXTENDED` status zero. The host accepts the test only when the
serial terminal record and QEMU status agree. Focused non-test Burrow artifacts
contain neither the terminal marker, semihosting argument block, nor trap.

The retained `burrow-first-entry` negative fixtures own these test-specific
failure codes:

| Code | Failure class |
| ---: | --- |
| `64` | Selected result-transport failure fixture |
| `65` | Entry-register contract |
| `66` | Unsupported exception level |
| `67` | Stack alignment or DAIF machine state |
| `68` | Fixed boot-information header |
| `69` | Loaded-image extent, bias, or entry |
| `70` | Bootstrap-stack description |
| `71` | Early-console section bounds or shape |
| `72` | Early-console record |
| `73` | Loader post-exit failure containment fixture |

The bounded dynamic reporter accepts codes 65–83. Codes 65–73 retain the
`burrow-first-entry` identifier; codes 74–83 select
`aarch64-normalized-entry`. The reporter uses the reference machine's fixed
PL011 independently of a rejected object before activation and the checked
upper MMIO alias after identity removal. Ordinary Burrow images retain the same
classifications but enter their masked wait because they contain no test
transport.

The emergency-vector fixtures use the normalized-entry invocation. After the
validated console record is published, a test-only `BRK #0x777` must enter
current-EL vector class 4. The
production reporter emits stage 2, vector 4, the requested EL, ESR
`0xF2000777`, ELR, FAR, and SPSR before the isolated transport emits
`PANIC:aarch64-normalized-entry:4`. EL1 and EL2 use separate QEMU processes. Focused
artifacts contain neither the injected `BRK`, the exception terminal marker,
nor the semihosting transport.

The `aarch64-normalized-entry` test owns the remaining adjacent failure classes:

| Code | Failure class |
| ---: | --- |
| `74` | Complete boot-information validation |
| `75` | Unsupported architectural state or feature |
| `76` | EL2 descent or common-EL1 proof |
| `77` | Transition storage planning |
| `78` | Table construction or descriptor audit |
| `79` | Table activation or higher-half transfer proof |
| `80` | Identity removal or surviving low reference |
| `81` | Architecture-neutral C++ context, physical-memory state, or witness |
| `82` | GICv3, physical-timer, handled-IRQ, or timer diagnostic proof |
| `83` | Diagnostic-monitor initialization, input, command bound, or output proof |

Architectural traps at emergency or stable vectors continue to use common
`PANIC` code 4. The implementation does not emit
`PASS:aarch64-normalized-entry` until the C++ witness returns its exact success
value; that witness cannot succeed until the complete reusable-console line has
been emitted.

Dedicated build-time-only target fixtures route each code 75–83 through the
same bounded reporter used by its production failure class. Separate stable-
vector fixtures access both unmapped guard pages, attempt a write to executable
read-only text, branch to writable execute-never data, and access a removed
physical identity alias. Those architectural probes require exact stage-8 ESR
evidence and `PANIC`/4 rather than accepting a timeout or generic failure.
A separate post-C++ `BRK #0x77a` fixture requires a versioned stage-9 complete
frame record, preserved x15 sentinel and x30 field, and the same `PANIC`/4
terminal pair.

## 10. Required Verification

Host tests cover at least:

- valid pass, generic failure, assertion, panic, exception, infrastructure
  failure, and test-specific failure streams;
- CRLF and chunk-split input;
- malformed prefixes, versions, fields, identifiers, codes, and line lengths;
- missing, duplicate, reordered, mismatched, and post-terminal records;
- every serial/process disagreement;
- pass-marker-then-timeout behavior;
- nonzero, signaled, and clean QEMU exits without terminal records; and
- launch-error and timeout classifications.

The target test matrix proves the exact AArch64 trap and argument block with
pass, fail, and panic images. The transport fixtures select `PASS`/0, `FAIL`/64,
or `PANIC`/2 at build time. Normalized-entry fixtures additionally prove every
assigned failure code and the stable protection faults described above. A
dedicated dynamic panic entry accepts only code 2 or 3, rewrites the shared
argument status, selects the matching terminal marker, and enters the same
single common semihosting trap. Live post-monitor fixtures reach that entry only
after the production `BURROW_PANIC_V1` reporter and require assertion code 2 or
kernel-panic code 3 on both channels. A separate UEFI fixture corrupts the
finalized magic
after successful `ExitBootServices()` and requires Burrow to reject it with
`FAIL`/68 without emitting its first-entry success diagnostic. A second clears
the finalized console output flag and requires complete-consumer `FAIL`/74
through the independent QEMU reporter. Another enters the loader's real
post-exit containment path and
requires its direct PL011 diagnostic plus `FAIL`/73 without entering Burrow.
Each is packaged into its own
ESP and accepted only when the common host harness observes the expected
serial/process pair and route-specific output. Focused non-test debug and
release images are inspected to ensure that semihosting, injected faults, and
test-only QEMU transport support are absent.

## 11. References

- [Arm semihosting specification](https://github.com/ARM-software/abi-aa/blob/main/semihosting/semihosting.rst)
- [QEMU semihosting documentation](https://www.qemu.org/docs/master/about/emulation.html#semihosting)
