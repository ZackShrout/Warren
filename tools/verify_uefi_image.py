#!/usr/bin/env python3

"""Reject a UEFI image with the wrong architecture, subsystem, or imports."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--readobj", required=True, type=pathlib.Path)
    parser.add_argument("--image", required=True, type=pathlib.Path)
    return parser.parse_args()


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

    print(f"Verified AArch64 EFI application with no imports: {arguments.image}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
