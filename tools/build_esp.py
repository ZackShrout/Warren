#!/usr/bin/env python3

"""Build Warren's reproducible FAT32 EFI System Partition image."""

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import tempfile


ESP_SIZE_BYTES = 64 * 1024 * 1024
FAT_TIMESTAMP = 946684800  # 2000-01-01T00:00:00Z, representable by FAT.


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mformat", required=True, type=pathlib.Path)
    parser.add_argument("--mcopy", required=True, type=pathlib.Path)
    parser.add_argument("--bootloader", required=True, type=pathlib.Path)
    parser.add_argument("--burrow", action="append", type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    return parser.parse_args()


def run(command: list[str]) -> None:
    subprocess.run(command, check=True, env={**os.environ, "TZ": "UTC"})


def main() -> int:
    arguments = parse_arguments()
    burrow_inputs = arguments.burrow or []
    if len(burrow_inputs) > 1:
        raise SystemExit("Burrow input is ambiguous; provide --burrow exactly once")
    burrow_input = burrow_inputs[0] if burrow_inputs else None

    if not arguments.bootloader.is_file():
        raise SystemExit(f"Bootloader input is not a file: {arguments.bootloader}")
    if burrow_input is not None and not burrow_input.is_file():
        raise SystemExit(f"Burrow input is not a file: {burrow_input}")

    arguments.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(
        prefix="warren-esp-", dir=arguments.output.parent
    ) as temporary_directory:
        temporary_root = pathlib.Path(temporary_directory)
        image = temporary_root / "warren-esp.img"
        boot_directory = temporary_root / "staging" / "EFI" / "BOOT"
        boot_directory.mkdir(parents=True)
        bootloader = boot_directory / "BOOTAA64.EFI"
        shutil.copyfile(arguments.bootloader, bootloader)

        staged_paths = [bootloader]
        if burrow_input is not None:
            warren_directory = temporary_root / "staging" / "EFI" / "WARREN"
            warren_directory.mkdir()
            burrow = warren_directory / "BURROW.ELF"
            shutil.copyfile(burrow_input, burrow)
            staged_paths.extend((burrow, warren_directory))

        for path in (
            *staged_paths,
            boot_directory,
            boot_directory.parent,
            boot_directory.parent.parent,
        ):
            os.utime(path, (FAT_TIMESTAMP, FAT_TIMESTAMP))

        with image.open("wb") as image_file:
            image_file.truncate(ESP_SIZE_BYTES)

        run(
            [
                str(arguments.mformat),
                "-i",
                str(image),
                "-F",
                "-N",
                "0x5752524e",
                "::",
            ]
        )
        run(
            [
                str(arguments.mcopy),
                "-i",
                str(image),
                "-s",
                "-m",
                str(temporary_root / "staging" / "EFI"),
                "::/",
            ]
        )

        os.replace(image, arguments.output)

    print(f"Built EFI System Partition: {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
