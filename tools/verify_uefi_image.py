#!/usr/bin/env python3

"""Verify UEFI image identity, imports, and the exact AArch64 handoff boundary."""

from __future__ import annotations

import argparse
import pathlib
import re
import struct
import subprocess
import sys


QEMU_SEMIHOST_HLT = bytes.fromhex("00005ed4")
QEMU_POST_EXIT_FAILURE_MARKER = (
    b"WARREN_TEST:1:FAIL:burrow-first-entry:73\r\n\0"
)
QEMU_POST_EXIT_FAILURE_ARGUMENTS = struct.pack("<QQ", 0x20026, 73)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--readobj", required=True, type=pathlib.Path)
    parser.add_argument("--objdump", required=True, type=pathlib.Path)
    parser.add_argument("--handoff-object", required=True, type=pathlib.Path)
    parser.add_argument("--image", required=True, type=pathlib.Path)
    parser.add_argument("--qemu-post-exit-failure-result", action="store_true")
    return parser.parse_args()


def disassemble_symbol(objdump: pathlib.Path, image: pathlib.Path, symbol: str) -> list[str]:
    result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={symbol}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        raise RuntimeError(f"could not disassemble {symbol}")

    instructions: list[str] = []
    for line in result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        instructions.append(re.sub(r"\s+", " ", instruction))

    if not instructions:
        raise RuntimeError(f"missing disassembly for {symbol}")
    return instructions


def require_order(name: str, instructions: list[str], expected: tuple[str, ...]) -> None:
    cursor = 0
    for fragment in expected:
        while cursor < len(instructions) and fragment not in instructions[cursor]:
            cursor += 1
        if cursor == len(instructions):
            raise RuntimeError(f"{name} is missing ordered instruction: {fragment}")
        cursor += 1


def require_contiguous(name: str, instructions: list[str], expected: tuple[str, ...]) -> None:
    width = len(expected)
    if not any(tuple(instructions[index : index + width]) == expected
               for index in range(len(instructions) - width + 1)):
        raise RuntimeError(f"{name} does not contain the exact required sequence")


def verify_handoff_assembly(arguments: argparse.Namespace) -> None:
    cache = disassemble_symbol(
        arguments.objdump,
        arguments.handoff_object,
        "warren_aarch64_synchronize_instruction_range",
    )
    require_order(
        "instruction-cache synchronization",
        cache,
        (
            "mrs x2, CTR_EL0",
            "ubfx x3, x2, #16, #4",
            "dc cvau, x5",
            "adds x5, x5, x3",
            "b.hs",
            "dsb ish",
            "ubfx x3, x2, #0, #4",
            "ic ivau, x5",
            "adds x5, x5, x3",
            "b.hs",
            "dsb ish",
            "isb",
            "ret",
        ),
    )

    handoff = disassemble_symbol(
        arguments.objdump,
        arguments.handoff_object,
        "warren_aarch64_handoff",
    )
    require_order(
        "AArch64 handoff",
        handoff,
        (
            "mrs x8, CurrentEL",
            "ldr w7, [x12, #0x18]",
            "tbnz w7, #0x5",
            "str w6, [x12]",
        ),
    )
    require_contiguous(
        "final AArch64 transfer",
        handoff,
        (
            "msr DAIFSet, #0xf",
            "mov sp, x10",
            "mov x0, x11",
            "mov x1, xzr",
            "mov x2, xzr",
            "mov x3, xzr",
            "br x9",
        ),
    )
    if any(instruction.startswith(("bl ", "blr ", "ret")) for instruction in handoff):
        raise RuntimeError("AArch64 handoff contains a call or return path")
    if any(instruction.startswith(("brk", "hlt", "hvc", "smc", "svc"))
           for instruction in cache + handoff):
        raise RuntimeError("AArch64 handoff boundary contains a trap or firmware call")
    if any(instruction.startswith("msr ") and instruction != "msr DAIFSet, #0xf"
           for instruction in handoff):
        raise RuntimeError("AArch64 handoff modifies an unexpected system register")

    failure = disassemble_symbol(
        arguments.objdump,
        arguments.handoff_object,
        "warren_aarch64_post_exit_failure",
    )
    require_order(
        "post-exit failure containment",
        failure,
        (
            "ldr w7, [x12, #0x18]",
            "tbnz w7, #0x5",
            "str w6, [x12]",
            "msr DAIFSet, #0xf",
            "cbz x13",
            "br x13",
            "wfe",
        ),
    )
    if any(instruction.startswith(("bl ", "blr ", "ret", "brk", "hlt", "hvc", "smc", "svc"))
           for instruction in failure):
        raise RuntimeError("post-exit failure containment can call, return, or trap")
    if [instruction for instruction in failure if instruction.startswith("br ")] != ["br x13"]:
        raise RuntimeError("post-exit failure containment has an unexpected branch target")

    image_bytes = arguments.image.read_bytes()
    for message in (
        b"WARREN_POST_EXIT:ExitBootServices:EL1\r\n\0",
        b"WARREN_POST_EXIT:ExitBootServices:EL2\r\n\0",
        b"WARREN_POST_EXIT:ExitBootServices:EL?\r\n\0",
        b"WARREN_POST_EXIT:FAIL\r\n\0",
    ):
        if image_bytes.count(message) != 1:
            raise RuntimeError(f"UEFI image does not contain exactly one {message!r}")

    expected_test_count = 1 if arguments.qemu_post_exit_failure_result else 0
    requirement = "exactly once" if expected_test_count else "absent"
    for payload, description in (
        (QEMU_SEMIHOST_HLT, "QEMU semihost HLT"),
        (QEMU_POST_EXIT_FAILURE_MARKER, "QEMU post-exit failure marker"),
        (QEMU_POST_EXIT_FAILURE_ARGUMENTS, "QEMU post-exit failure argument block"),
    ):
        count = image_bytes.count(payload)
        if count != expected_test_count:
            raise RuntimeError(
                f"{description} must be {requirement}, found {count}"
            )


def main() -> int:
    arguments = parse_arguments()
    result = subprocess.run(
        [
            str(arguments.readobj),
            "--file-headers",
            "--coff-imports",
            str(arguments.image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        return result.returncode

    required_fragments = ("Format: COFF-ARM64", "Subsystem: IMAGE_SUBSYSTEM_EFI_APPLICATION")
    missing = [fragment for fragment in required_fragments if fragment not in result.stdout]
    if missing:
        sys.stdout.write(result.stdout)
        print(f"UEFI verification failed; missing: {', '.join(missing)}", file=sys.stderr)
        return 1

    if "Import {" in result.stdout:
        sys.stdout.write(result.stdout)
        print("UEFI verification failed; freestanding image has imports", file=sys.stderr)
        return 1

    try:
        verify_handoff_assembly(arguments)
    except RuntimeError as error:
        print(f"UEFI verification failed: {error}", file=sys.stderr)
        return 1

    print(
        "Verified AArch64 EFI application with no imports and reviewed handoff assembly: "
        f"{arguments.image}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
