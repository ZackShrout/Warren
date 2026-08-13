#!/usr/bin/env python3

"""Build the same ESP twice and require byte-for-byte equality."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import subprocess
import tempfile


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python", required=True, type=pathlib.Path)
    parser.add_argument("--builder", required=True, type=pathlib.Path)
    parser.add_argument("--mformat", required=True, type=pathlib.Path)
    parser.add_argument("--mcopy", required=True, type=pathlib.Path)
    parser.add_argument("--bootloader", required=True, type=pathlib.Path)
    parser.add_argument("--burrow", type=pathlib.Path)
    return parser.parse_args()


def digest(path: pathlib.Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as file:
        while chunk := file.read(1024 * 1024):
            hasher.update(chunk)
    return hasher.hexdigest()


def main() -> int:
    arguments = parse_arguments()
    with tempfile.TemporaryDirectory(prefix="warren-esp-repro-") as temporary_directory:
        temporary_root = pathlib.Path(temporary_directory)
        images = [temporary_root / "first.img", temporary_root / "second.img"]

        for image in images:
            command = [
                str(arguments.python),
                str(arguments.builder),
                "--mformat",
                str(arguments.mformat),
                "--mcopy",
                str(arguments.mcopy),
                "--bootloader",
                str(arguments.bootloader),
                "--output",
                str(image),
            ]
            if arguments.burrow is not None:
                command.extend(("--burrow", str(arguments.burrow)))
            subprocess.run(command, check=True)

        first_digest = digest(images[0])
        second_digest = digest(images[1])
        if first_digest != second_digest:
            print(f"ESP mismatch: {first_digest} != {second_digest}")
            return 1

    print(f"Reproducible ESP SHA-256: {first_digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
