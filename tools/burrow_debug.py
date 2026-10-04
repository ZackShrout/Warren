#!/usr/bin/env python3

"""Verify Burrow debug artifacts and symbolize bounded runtime addresses."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import pathlib
import re
import struct
import subprocess
import sys
from collections.abc import Callable, Sequence


ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
ELF_MAGIC = b"\x7fELF"
ELF_CLASS_64 = 2
ELF_DATA_LITTLE_ENDIAN = 1
ELF_TYPE_DYNAMIC = 3
ELF_MACHINE_AARCH64 = 183
MAXIMUM_U64 = 0xFFFFFFFFFFFFFFFF
STABLE_IMAGE_BASE = 0xFFFFFFFF80000000
DEBUG_PHYSICAL_LOAD_BIAS = 0x40000000
MAP_HEADER = ("VMA", "LMA", "Size", "Align", "Out", "In", "Symbol")


class DebugArtifactError(Exception):
    """One debug artifact or requested address violates the workflow contract."""


@dataclasses.dataclass(frozen=True)
class LinkerMapSummary:
    entry: int
    image_end: int
    symbols: dict[str, int]


@dataclasses.dataclass(frozen=True)
class SymbolizedAddress:
    input_address: int
    relative_address: int
    function: str
    source: str
    line: int
    column: int


def parse_u64(text: str, description: str) -> int:
    try:
        value = int(text, 0)
    except ValueError as error:
        raise DebugArtifactError(f"{description} is not an integer: {text}") from error
    if value < 0 or value > MAXIMUM_U64:
        raise DebugArtifactError(f"{description} is outside uint64_t: {text}")
    return value


def argument_u64(description: str) -> Callable[[str], int]:
    def parse(text: str) -> int:
        try:
            return parse_u64(text, description)
        except DebugArtifactError as error:
            raise argparse.ArgumentTypeError(str(error)) from error

    return parse


def read_elf_entry(image: pathlib.Path) -> int:
    with image.open("rb") as stream:
        header_bytes = stream.read(ELF_HEADER.size)
    if len(header_bytes) != ELF_HEADER.size:
        raise DebugArtifactError("symbol image is smaller than one ELF64 header")
    fields = ELF_HEADER.unpack(header_bytes)
    identification = fields[0]
    if (
        identification[:4] != ELF_MAGIC
        or identification[4] != ELF_CLASS_64
        or identification[5] != ELF_DATA_LITTLE_ENDIAN
        or fields[1] != ELF_TYPE_DYNAMIC
        or fields[2] != ELF_MACHINE_AARCH64
    ):
        raise DebugArtifactError("symbol image is not little-endian AArch64 ET_DYN")
    return fields[4]


def parse_linker_map(text: str) -> LinkerMapSummary:
    lines = text.splitlines()
    if not lines or tuple(lines[0].split()) != MAP_HEADER:
        raise DebugArtifactError("linker map has an unexpected or missing header")

    entry_values: list[int] = []
    image_end_values: list[int] = []
    symbols: dict[str, int] = {}
    symbol_pattern = re.compile(
        r"^\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+"
        r"([0-9A-Fa-f]+)\s+([0-9]+)\s+(.+?)\s*$"
    )
    for line in lines[1:]:
        match = symbol_pattern.match(line)
        if match is None:
            continue
        vma = int(match.group(1), 16)
        description = match.group(5)
        if description == "burrow_aarch64_entry":
            entry_values.append(vma)
        if description == "HIDDEN(burrow_image_end = .)":
            image_end_values.append(vma)
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", description):
            previous = symbols.get(description)
            if previous is not None and previous != vma:
                raise DebugArtifactError(
                    f"linker map assigns {description} more than one address"
                )
            symbols[description] = vma

    if len(entry_values) != 1:
        raise DebugArtifactError("linker map must define burrow_aarch64_entry once")
    if len(image_end_values) != 1:
        raise DebugArtifactError("linker map must define burrow_image_end once")
    entry = entry_values[0]
    image_end = image_end_values[0]
    if entry == 0 or image_end <= entry:
        raise DebugArtifactError("linker map has an invalid image extent")
    if image_end > MAXIMUM_U64 - STABLE_IMAGE_BASE + 1:
        raise DebugArtifactError("linker map image cannot fit at the stable alias")
    return LinkerMapSummary(entry, image_end, symbols)


def verify_debug_artifacts(
    image: pathlib.Path,
    linker_map: pathlib.Path,
) -> LinkerMapSummary:
    entry = read_elf_entry(image)
    summary = parse_linker_map(linker_map.read_text(encoding="utf-8"))
    if summary.entry != entry:
        raise DebugArtifactError(
            "linker-map entry does not match the symbol image: "
            f"0x{summary.entry:X} != 0x{entry:X}"
        )
    return summary


def normalize_runtime_address(
    address: int,
    address_space: str,
    image_end: int,
    load_bias: int | None = None,
) -> int:
    if address_space == "elf":
        relative = address
    elif address_space == "stable":
        if address < STABLE_IMAGE_BASE:
            raise DebugArtifactError("stable address is below the stable image base")
        relative = address - STABLE_IMAGE_BASE
    elif address_space == "physical":
        if load_bias is None:
            raise DebugArtifactError("physical address space requires --load-bias")
        if address < load_bias:
            raise DebugArtifactError("physical address is below the load bias")
        relative = address - load_bias
    else:
        raise DebugArtifactError(f"unsupported address space: {address_space}")

    if relative >= image_end:
        raise DebugArtifactError(
            f"address normalizes outside the image extent 0x{image_end:X}"
        )
    return relative


def normalize_source_path(text: str, source_root: pathlib.Path) -> tuple[str, int, int]:
    try:
        path_text, line_text, column_text = text.rsplit(":", 2)
        line = int(line_text)
        column = int(column_text)
    except (ValueError, TypeError) as error:
        raise DebugArtifactError(
            f"symbolizer returned an invalid source location: {text}"
        ) from error
    if line < 0 or column < 0:
        raise DebugArtifactError("symbolizer returned a negative source position")

    source_path = pathlib.Path(os.path.normpath(path_text))
    root = source_root.resolve()
    if source_path.is_absolute():
        try:
            source_path = source_path.resolve().relative_to(root)
        except ValueError as error:
            raise DebugArtifactError(
                f"symbolizer source escapes the repository: {path_text}"
            ) from error
    if source_path == pathlib.Path("..") or ".." in source_path.parts:
        raise DebugArtifactError(
            f"symbolizer source escapes the repository: {path_text}"
        )
    return source_path.as_posix(), line, column


def symbolize_addresses(
    symbolizer: pathlib.Path,
    image: pathlib.Path,
    source_root: pathlib.Path,
    inputs: Sequence[tuple[int, int]],
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> list[SymbolizedAddress]:
    if not inputs:
        raise DebugArtifactError("at least one address is required")
    try:
        result = runner(
            [
                str(symbolizer),
                f"--obj={image}",
                "--demangle",
                "--functions=linkage",
                "--inlining=false",
                *(f"0x{relative:X}" for _, relative in inputs),
            ],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except OSError as error:
        raise DebugArtifactError(f"could not run llvm-symbolizer: {error}") from error
    if result.returncode != 0:
        raise DebugArtifactError(
            f"llvm-symbolizer failed with status {result.returncode}: "
            f"{result.stdout.strip()}"
        )
    blocks = re.split(r"\n\s*\n", result.stdout.strip())
    if len(blocks) != len(inputs):
        raise DebugArtifactError("llvm-symbolizer returned an unexpected record count")

    records: list[SymbolizedAddress] = []
    for (input_address, relative), block in zip(inputs, blocks, strict=True):
        lines = block.splitlines()
        if len(lines) != 2 or lines[0] == "??" or lines[1].startswith("??:"):
            raise DebugArtifactError(
                f"address 0x{relative:X} did not resolve to one source location"
            )
        source, line, column = normalize_source_path(lines[1], source_root)
        records.append(
            SymbolizedAddress(
                input_address,
                relative,
                lines[0],
                source,
                line,
                column,
            )
        )
    return records


def debug_symbol_probes(summary: LinkerMapSummary) -> list[tuple[int, int]]:
    cpp_symbol = summary.symbols.get("burrow_qemu_virt_panic")
    if cpp_symbol is None:
        raise DebugArtifactError(
            "linker map does not define the C++ debug probe burrow_qemu_virt_panic"
        )
    return [
        (summary.entry, summary.entry),
        (STABLE_IMAGE_BASE + summary.entry, summary.entry),
        (cpp_symbol, cpp_symbol),
        (DEBUG_PHYSICAL_LOAD_BIAS + cpp_symbol, cpp_symbol),
    ]


def _lldb_quote(value: pathlib.Path | str) -> str:
    return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'


def validate_port(port: int) -> int:
    if port < 1 or port > 65535:
        raise DebugArtifactError("debug port must be between 1 and 65535")
    return port


def lldb_slide(address_space: str, load_bias: int | None) -> int:
    if address_space == "stable":
        return STABLE_IMAGE_BASE
    if address_space == "physical":
        if load_bias is None:
            raise DebugArtifactError("physical LLDB mode requires --load-bias")
        return load_bias
    raise DebugArtifactError("LLDB address space must be stable or physical")


def validate_image_slide(slide: int, image_end: int) -> None:
    if image_end > MAXIMUM_U64 - slide + 1:
        raise DebugArtifactError("LLDB image slide overflows the address space")


def render_lldb_script(
    image: pathlib.Path,
    source_root: pathlib.Path,
    slide: int,
    port: int,
) -> str:
    validate_port(port)
    return (
        "# Generated by tools/burrow_debug.py. Do not edit.\n"
        "settings set target.default-arch aarch64\n"
        f"settings set target.source-map . {_lldb_quote(source_root.resolve())}\n"
        f"target create --arch aarch64 {_lldb_quote(image.resolve())}\n"
        f"gdb-remote 127.0.0.1:{port}\n"
        f"target modules load --file {_lldb_quote(image.resolve())} "
        f"--slide 0x{slide:X}\n"
    )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify and use Burrow linker-map and symbol artifacts."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    def add_artifacts(command: argparse.ArgumentParser) -> None:
        command.add_argument("--image", required=True, type=pathlib.Path)
        command.add_argument("--map", required=True, type=pathlib.Path)

    verify_parser = subparsers.add_parser("verify")
    add_artifacts(verify_parser)

    check_parser = subparsers.add_parser("check")
    add_artifacts(check_parser)
    check_parser.add_argument("--symbolizer", required=True, type=pathlib.Path)
    check_parser.add_argument("--source-root", required=True, type=pathlib.Path)

    symbolize_parser = subparsers.add_parser("symbolize")
    add_artifacts(symbolize_parser)
    symbolize_parser.add_argument("--symbolizer", required=True, type=pathlib.Path)
    symbolize_parser.add_argument("--source-root", required=True, type=pathlib.Path)
    symbolize_parser.add_argument(
        "--address-space", choices=("elf", "stable", "physical"), required=True
    )
    symbolize_parser.add_argument("--load-bias", type=argument_u64("load bias"))
    symbolize_parser.add_argument(
        "--address",
        action="append",
        required=True,
        type=argument_u64("address"),
    )

    lldb_parser = subparsers.add_parser("lldb")
    add_artifacts(lldb_parser)
    lldb_parser.add_argument("--source-root", required=True, type=pathlib.Path)
    lldb_parser.add_argument("--output", required=True, type=pathlib.Path)
    lldb_parser.add_argument(
        "--address-space", choices=("stable", "physical"), required=True
    )
    lldb_parser.add_argument("--load-bias", type=argument_u64("load bias"))
    lldb_parser.add_argument("--port", type=int, default=1234)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        summary = verify_debug_artifacts(arguments.image, arguments.map)
        if arguments.command == "verify":
            print(
                "Verified Burrow linker map: "
                f"entry=0x{summary.entry:X}, image_end=0x{summary.image_end:X}"
            )
            return 0
        if arguments.command == "check":
            records = symbolize_addresses(
                arguments.symbolizer,
                arguments.image,
                arguments.source_root,
                debug_symbol_probes(summary),
            )
            sources = [record.source for record in records]
            expected_sources = [
                "kernel/src/Arch/AArch64/Entry.S",
                "kernel/src/Arch/AArch64/Entry.S",
                "kernel/src/Platform/QemuVirt/Panic.cpp",
                "kernel/src/Platform/QemuVirt/Panic.cpp",
            ]
            if sources != expected_sources:
                raise DebugArtifactError(
                    "Debug symbols resolved to unexpected sources: "
                    + ", ".join(sources)
                )
            print(
                "Verified Burrow Debug symbols: "
                "ELF, stable, and physical aliases resolve to assembly and C++"
            )
            return 0
        if arguments.command == "symbolize":
            normalized = [
                (
                    address,
                    normalize_runtime_address(
                        address,
                        arguments.address_space,
                        summary.image_end,
                        arguments.load_bias,
                    ),
                )
                for address in arguments.address
            ]
            records = symbolize_addresses(
                arguments.symbolizer,
                arguments.image,
                arguments.source_root,
                normalized,
            )
            for record in records:
                print(
                    json.dumps(
                        {
                            "format": "BURROW_SYMBOL_V1",
                            "input": f"0x{record.input_address:016X}",
                            "relative": f"0x{record.relative_address:016X}",
                            "function": record.function,
                            "source": record.source,
                            "line": record.line,
                            "column": record.column,
                        },
                        separators=(",", ":"),
                    )
                )
            return 0
        slide = lldb_slide(arguments.address_space, arguments.load_bias)
        validate_image_slide(slide, summary.image_end)
        script = render_lldb_script(
            arguments.image,
            arguments.source_root,
            slide,
            arguments.port,
        )
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(script, encoding="utf-8")
        print(f"Generated Burrow LLDB commands: {arguments.output}")
        return 0
    except (DebugArtifactError, OSError) as error:
        print(f"Burrow debug artifact error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
