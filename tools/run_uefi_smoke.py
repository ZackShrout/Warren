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
    parser.add_argument("--timeout", type=float, default=30.0)
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
    protocol_result = classify_process_result(result.stdout, result.returncode)
    if (
        protocol_result.classification is not HostClassification.PASS
        or protocol_result.test_identifier != arguments.expected_test
    ):
        print(
            f"UEFI smoke classification: {protocol_result.classification.value}: "
            f"{protocol_result.detail}",
            file=sys.stderr,
        )
        return protocol_result.harness_status or 3

    print("Warren UEFI smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
