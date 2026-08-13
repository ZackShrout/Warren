#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile
import unittest


BUILDER = pathlib.Path(__file__).resolve().parents[2] / "tools" / "build_esp.py"


class EspBuilderInputTests(unittest.TestCase):
    def run_builder(self, arguments: list[str]) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(BUILDER), *arguments],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

    def base_arguments(
        self, bootloader: pathlib.Path, output: pathlib.Path
    ) -> list[str]:
        return [
            "--mformat",
            "/unused/mformat",
            "--mcopy",
            "/unused/mcopy",
            "--bootloader",
            str(bootloader),
            "--output",
            str(output),
        ]

    def test_rejects_missing_bootloader(self) -> None:
        with tempfile.TemporaryDirectory(prefix="warren-esp-input-") as directory:
            root = pathlib.Path(directory)
            result = self.run_builder(
                self.base_arguments(root / "missing.efi", root / "esp.img")
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn("Bootloader input is not a file", result.stdout)

    def test_rejects_missing_burrow_input(self) -> None:
        with tempfile.TemporaryDirectory(prefix="warren-esp-input-") as directory:
            root = pathlib.Path(directory)
            bootloader = root / "BOOTAA64.EFI"
            bootloader.write_bytes(b"bootloader")
            arguments = self.base_arguments(bootloader, root / "esp.img")
            arguments.extend(("--burrow", str(root / "missing.elf")))
            result = self.run_builder(arguments)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Burrow input is not a file", result.stdout)

    def test_rejects_ambiguous_burrow_input(self) -> None:
        with tempfile.TemporaryDirectory(prefix="warren-esp-input-") as directory:
            root = pathlib.Path(directory)
            bootloader = root / "BOOTAA64.EFI"
            burrow = root / "burrow-runtime.elf"
            bootloader.write_bytes(b"bootloader")
            burrow.write_bytes(b"burrow")
            arguments = self.base_arguments(bootloader, root / "esp.img")
            arguments.extend(
                ("--burrow", str(burrow), "--burrow", str(burrow))
            )
            result = self.run_builder(arguments)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Burrow input is ambiguous", result.stdout)


if __name__ == "__main__":
    unittest.main()
