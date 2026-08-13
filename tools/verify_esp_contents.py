#!/usr/bin/env python3

#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

"""Extract the combined ESP payloads and require byte-identical inputs."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcopy", required=True, type=pathlib.Path)
    parser.add_argument("--esp", required=True, type=pathlib.Path)
    parser.add_argument("--bootloader", required=True, type=pathlib.Path)
    parser.add_argument("--burrow", required=True, type=pathlib.Path)
    return parser.parse_args()


def extract(
    mcopy: pathlib.Path,
    esp: pathlib.Path,
    source: str,
    destination: pathlib.Path,
) -> None:
    subprocess.run(
        [str(mcopy), "-i", str(esp), source, str(destination)],
        check=True,
    )


def require_equal(expected: pathlib.Path, actual: pathlib.Path, name: str) -> None:
    if expected.read_bytes() != actual.read_bytes():
        raise SystemExit(f"Packaged {name} bytes do not match input: {expected}")


def main() -> int:
    arguments = parse_arguments()
    with tempfile.TemporaryDirectory(prefix="warren-esp-contents-") as directory:
        temporary_root = pathlib.Path(directory)
        packaged_bootloader = temporary_root / "BOOTAA64.EFI"
        packaged_burrow = temporary_root / "BURROW.ELF"

        extract(
            arguments.mcopy,
            arguments.esp,
            "::/EFI/BOOT/BOOTAA64.EFI",
            packaged_bootloader,
        )
        extract(
            arguments.mcopy,
            arguments.esp,
            "::/EFI/WARREN/BURROW.ELF",
            packaged_burrow,
        )
        require_equal(arguments.bootloader, packaged_bootloader, "bootloader")
        require_equal(arguments.burrow, packaged_burrow, "Burrow image")

    print("Verified byte-identical combined ESP bootloader and Burrow payloads")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
