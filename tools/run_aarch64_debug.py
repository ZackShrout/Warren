#!/usr/bin/env python3

"""Run Warren's pinned AArch64 QEMU profile paused for a local debugger."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile

from burrow_debug import DebugArtifactError, validate_port
from run_uefi_smoke import machine_configuration


def debug_qemu_command(
    qemu: pathlib.Path,
    firmware_code: pathlib.Path,
    variable_store: pathlib.Path,
    esp: pathlib.Path,
    initial_el: str,
    port: int,
) -> list[str]:
    validate_port(port)
    return [
        str(qemu),
        "-machine",
        machine_configuration(initial_el),
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
        "-S",
        "-gdb",
        f"tcp:127.0.0.1:{port}",
        "-drive",
        f"if=pflash,format=raw,unit=0,readonly=on,file={firmware_code}",
        "-drive",
        f"if=pflash,format=raw,unit=1,file={variable_store}",
        "-drive",
        f"if=none,format=raw,id=warrenesp,file={esp}",
        "-device",
        "virtio-blk-device,drive=warrenesp",
        "-boot",
        "order=c",
    ]


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-code", required=True, type=pathlib.Path)
    parser.add_argument("--firmware-vars", required=True, type=pathlib.Path)
    parser.add_argument("--esp", required=True, type=pathlib.Path)
    parser.add_argument("--initial-el", choices=("el1", "el2"), required=True)
    parser.add_argument("--port", type=int, default=1234)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        validate_port(arguments.port)
        for path, description in (
            (arguments.qemu, "QEMU"),
            (arguments.firmware_code, "firmware code"),
            (arguments.firmware_vars, "firmware variables"),
            (arguments.esp, "ESP"),
        ):
            if not path.exists():
                raise DebugArtifactError(f"{description} does not exist: {path}")

        with tempfile.TemporaryDirectory(prefix="warren-debug-") as temporary:
            variable_store = pathlib.Path(temporary) / "edk2-vars.fd"
            shutil.copyfile(arguments.firmware_vars, variable_store)
            command = debug_qemu_command(
                arguments.qemu,
                arguments.firmware_code,
                variable_store,
                arguments.esp,
                arguments.initial_el,
                arguments.port,
            )
            print(
                f"QEMU is paused; attach LLDB to 127.0.0.1:{arguments.port}.",
                flush=True,
            )
            return subprocess.run(command, check=False).returncode
    except KeyboardInterrupt:
        return 130
    except (DebugArtifactError, OSError) as error:
        print(f"Warren debug launch error: {error}", file=sys.stderr)
        return 125


if __name__ == "__main__":
    raise SystemExit(main())
