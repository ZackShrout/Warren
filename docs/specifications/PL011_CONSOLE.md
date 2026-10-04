# Warren Allocation-Free PL011 Console

**Status:** Implemented Phase 1 subordinate contract

**Target:** QEMU `virt-11.0` reference PL011 after normalized AArch64 entry

## 1. Scope

This contract begins after Burrow has installed and audited its owned EL1 page
tables, transferred to the stable image and stack aliases, removed TTBR0
identity mappings, and revalidated the complete boot-information object in
architecture-neutral C++. It provides reusable, allocation-free polling input
and output without replacing the independent assembly emergency or QEMU-result
writers.

The console is polling-only. It does not configure the UART, buffer input or
output, allocate memory, take a lock, enable interrupts, or define log, panic,
assertion, and exception-reporting policy.

## 2. Device-Class Reader And Writer

`burrow::drivers::console_reader_t` contains an explicit borrowed context and a
`noexcept` one-byte function. `read_console_byte()` clears the destination byte,
validates the endpoint, and preserves distinct success, timeout,
invalid-reader, and input-failure results.

`burrow::drivers::console_writer_t` contains an explicit borrowed context and a
`noexcept` one-byte function. `write_console()` accepts an exact byte count,
performs no terminator search, writes bytes in order, and stops at the first
failed byte. Invalid writer state, invalid text, and output failure have
distinct results.

The writer owns no storage and has no global instance. Its caller owns the
context lifetime and any later serialization policy.

## 3. PL011 Driver

The driver accepts an aligned mapped register base and a nonzero polling limit.
For each output byte it reads the 32-bit flag register at byte offset `0x18`.
If `FR.TXFF` bit `0x20` is clear, it writes the byte as a 32-bit value to the
data register at offset `0`. If the bit remains set for the complete polling
budget, the write fails without modifying the data register.

For input it polls the same register until `FR.RXFE` bit `0x10` clears, then
returns the low eight bits of the data register. Exhausting the budget reports
timeout without inventing a byte. The general console budget is 1,000,000 flag
reads per byte; the interactive monitor uses 100,000,000 so a host can respond
after observing its ready marker. Both are finite diagnostic budgets, not
scheduling or timing guarantees.

## 4. QEMU-Virt Selection

Platform code consumes only a boot-information object that already passed the
complete protocol validator. It nevertheless requires the early-console
feature and exact contained-record shape before selecting:

- kind `PL011`;
- input and output capabilities and no unknown flags;
- physical base `0x09000000`;
- register stride 4 and width 32; and
- zero reserved fields.

The driver receives the preflighted device mapping at
`0xFFFFC00009000000`. Neither the device-class interface nor the PL011 driver
contains the QEMU physical or virtual address.

## 5. Kernel Entry And Observable Result

The extern-C platform entry first validates the 64-byte kernel context and the
complete aliased boot object through Core. It then selects the platform
console. Core writes exactly:

```text
BURROW_CONSOLE:driver=pl011:mode=polling:output=ready\r\n
```

Only after the complete line succeeds does Core store the retained witness and
return the normalized-entry success token. Selection or output failure returns
failure with the witness unchanged; the existing assembly boundary classifies
that as kernel-entry failure and retains its independent failure reporter.

## 6. Verification

Host tests cover exact and empty writes, invalid state, partial-output failure,
successful PL011 data-register output, bounded transmit-full timeout, exact
platform selection, descriptor rejection, and witness ordering. The independent
Burrow ELF verifier continues to require three permission-separated loads, no
runtime imports, and zero relocations.

Both inherited EL1 and EL2 QEMU routes require the exact `BURROW_CONSOLE` line
before accepting the existing `aarch64-normalized-entry` pass result. Focused
ordinary images contain the reusable production console but no semihosting or
test-result transport.

## 7. Bounded Diagnostic Monitor

After one handled physical-timer tick, Core emits
`BURROW_MONITOR:ready:commands=help,status,exit`, then reads at most eight CR-
or LF-terminated commands of at most 15 bytes each. `help` repeats the command
set, `status` emits the nonzero observed timer tick, and `exit` emits an
accepted record and returns success. Unknown and empty commands emit a fixed
error and consume a slot. Timeout, endpoint failure, overflow, exhausted slots,
invalid timer state, or output failure returns a typed error; the architecture
boundary maps it to failure 83 or the ordinary masked wait.

System tests wait for the ready record before writing `status\r` and `exit\r`
to QEMU's PL011 input and require ready, status, and exit in that order before
accepting normalized success. No shell grammar, editing, history, allocator,
scheduler, IRQ-driven receive, or unbounded session is part of this contract.
