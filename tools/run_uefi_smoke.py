#!/usr/bin/env python3

"""Boot Warren's AArch64 ESP in QEMU and require its versioned marker."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile

from warren_test_protocol import HostClassification, classify_process_result


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-code", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-vars", required=True, type=pathlib.Path)
    parser.add_argument("--esp", required=True, type=pathlib.Path)
    parser.add_argument("--expected-test", required=True)
    parser.add_argument(
        "--expected-result",
        choices=("pass", "fail", "panic"),
        default="pass",
    )
    parser.add_argument(
        "--expected-code",
        type=int,
        help="exact guest result code for an expected failure or panic",
    )
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--require-output", action="append", default=[])
    parser.add_argument("--forbid-output", action="append", default=[])
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    with tempfile.TemporaryDirectory(prefix="warren-qemu-") as temporary_directory:
        variable_store = pathlib.Path(temporary_directory) / "edk2-vars.fd"
        shutil.copyfile(arguments.firmware_vars, variable_store)

        command = [
            str(arguments.qemu),
            "-machine",
            "virt-11.0,gic-version=3",
            "-accel",
            "tcg",
            "-cpu",
            "cortex-a57",
            "-smp",
            "1",
            "-m",
            "512M",
            "-display",
            "none",
            "-monitor",
            "none",
            "-semihosting-config",
            "enable=on,target=native",
            "-serial",
            "stdio",
            "-no-reboot",
            "-drive",
            f"if=pflash,format=raw,unit=0,readonly=on,file={arguments.firmware_code}",
            "-drive",
            f"if=pflash,format=raw,unit=1,file={variable_store}",
            "-drive",
            f"if=none,format=raw,id=warrenesp,file={arguments.esp}",
            "-device",
            "virtio-blk-device,drive=warrenesp",
            "-boot",
            "order=c",
        ]

        try:
            result = subprocess.run(
                command,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=arguments.timeout,
            )
        except subprocess.TimeoutExpired as error:
            output = error.stdout or ""
            if isinstance(output, str):
                output = output.encode()
            sys.stdout.buffer.write(output)
            print(f"QEMU smoke test timed out after {arguments.timeout:g}s", file=sys.stderr)
            return 124

    sys.stdout.buffer.write(result.stdout)
    for required_text in arguments.require_output:
        required = required_text.encode("ascii")
        if required not in result.stdout:
            print(
                f"UEFI smoke output is missing required text: {required_text}",
                file=sys.stderr,
            )
            return 3
    for forbidden_text in arguments.forbid_output:
        forbidden = forbidden_text.encode("ascii")
        if forbidden in result.stdout:
            print(
                f"UEFI smoke output contains forbidden text: {forbidden_text}",
                file=sys.stderr,
            )
            return 3
    protocol_result = classify_process_result(result.stdout, result.returncode)
    expected_classification, default_code = {
        "pass": (HostClassification.PASS, 0),
        "fail": (HostClassification.EXPLICIT_FAILURE, 64),
        "panic": (HostClassification.PANIC_OR_ASSERTION, 2),
    }[arguments.expected_result]
    expected_code = (
        arguments.expected_code
        if arguments.expected_code is not None
        else default_code
    )
    if arguments.expected_result == "pass" and expected_code != 0:
        print("a pass result requires expected code 0", file=sys.stderr)
        return 125
    if (
        protocol_result.classification is not expected_classification
        or protocol_result.test_identifier != arguments.expected_test
        or protocol_result.guest_result_code != expected_code
    ):
        print(
            f"UEFI smoke classification: {protocol_result.classification.value}: "
            f"{protocol_result.detail}",
            file=sys.stderr,
        )
        return protocol_result.harness_status or 3

    print(f"Warren UEFI smoke test observed expected {arguments.expected_result} result")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
