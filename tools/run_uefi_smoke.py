#!/usr/bin/env python3

"""Boot Warren's AArch64 ESP in QEMU and require its versioned marker."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


EXPECTED_MARKER = "WARREN_TEST:1:PASS:uefi-first-light"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-code", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-vars", required=True, type=pathlib.Path)
    parser.add_argument("--esp", required=True, type=pathlib.Path)
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
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=arguments.timeout,
            )
        except subprocess.TimeoutExpired as error:
            output = error.stdout or ""
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
            sys.stdout.write(output)
            print(f"QEMU smoke test timed out after {arguments.timeout:g}s", file=sys.stderr)
            return 124

    sys.stdout.write(result.stdout)
    if EXPECTED_MARKER not in result.stdout:
        print(f"QEMU output did not contain {EXPECTED_MARKER!r}", file=sys.stderr)
        return 1
    if result.returncode != 0:
        print(f"QEMU exited with status {result.returncode}", file=sys.stderr)
        return result.returncode

    print("Warren UEFI smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
