#!/usr/bin/env python3

#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

"""Verify Warren's bounded AArch64 Burrow ELF image contract."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import struct
import subprocess
import sys


ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")
SECTION_HEADER = struct.Struct("<IIQQQQIIQQ")
SYMBOL = struct.Struct("<IBBHQQ")
DYNAMIC_ENTRY = struct.Struct("<qQ")
RELA_ENTRY = struct.Struct("<QQq")

ELF_MAGIC = b"\x7fELF"
ELF_CLASS_64 = 2
ELF_DATA_LITTLE_ENDIAN = 1
ELF_VERSION_CURRENT = 1
ELF_OSABI_SYSTEM_V = 0
ELF_TYPE_DYNAMIC = 3
ELF_MACHINE_AARCH64 = 183

PROGRAM_TYPE_LOAD = 1
PROGRAM_TYPE_DYNAMIC = 2
PROGRAM_TYPE_INTERPRETER = 3
PROGRAM_TYPE_NOTE = 4
PROGRAM_TYPE_SHLIB = 5
PROGRAM_TYPE_PHDR = 6
PROGRAM_TYPE_TLS = 7
PROGRAM_TYPE_GNU_STACK = 0x6474E551

PROGRAM_FLAG_EXECUTE = 1
PROGRAM_FLAG_WRITE = 2
PROGRAM_FLAG_READ = 4
PROGRAM_FLAGS_ALLOWED = (
    PROGRAM_FLAG_READ,
    PROGRAM_FLAG_READ | PROGRAM_FLAG_EXECUTE,
    PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE,
)

SECTION_TYPE_NULL = 0
SECTION_TYPE_PROGBITS = 1
SECTION_TYPE_SYMBOL_TABLE = 2
SECTION_TYPE_STRING_TABLE = 3
SECTION_TYPE_RELA = 4
SECTION_TYPE_DYNAMIC = 6
SECTION_TYPE_NOBITS = 8
SECTION_TYPE_REL = 9
SECTION_TYPE_DYNAMIC_SYMBOLS = 11
SECTION_TYPE_INIT_ARRAY = 14
SECTION_TYPE_FINI_ARRAY = 15
SECTION_TYPE_PREINIT_ARRAY = 16
SECTION_TYPE_RELR = 19

SECTION_FLAG_WRITE = 1
SECTION_FLAG_ALLOCATE = 2
SECTION_FLAG_EXECUTE = 4
SECTION_FLAG_TLS = 0x400

SYMBOL_BINDING_GLOBAL = 1
SYMBOL_BINDING_LOCAL = 0
SYMBOL_TYPE_FUNCTION = 2
SYMBOL_VISIBILITY_HIDDEN = 2
SYMBOL_SECTION_UNDEFINED = 0
SYMBOL_SECTION_ABSOLUTE = 0xFFF1
SYMBOL_SECTION_COMMON = 0xFFF2
SYMBOL_SECTION_EXTENDED = 0xFFFF

DYNAMIC_TAG_NULL = 0
DYNAMIC_TAG_NEEDED = 1
DYNAMIC_TAG_PLTRELSZ = 2
DYNAMIC_TAG_PLTGOT = 3
DYNAMIC_TAG_RELA = 7
DYNAMIC_TAG_RELASZ = 8
DYNAMIC_TAG_RELAENT = 9
DYNAMIC_TAG_INIT = 12
DYNAMIC_TAG_FINI = 13
DYNAMIC_TAG_SONAME = 14
DYNAMIC_TAG_RPATH = 15
DYNAMIC_TAG_SYMBOLIC = 16
DYNAMIC_TAG_REL = 17
DYNAMIC_TAG_RELSZ = 18
DYNAMIC_TAG_RELENT = 19
DYNAMIC_TAG_PLTREL = 20
DYNAMIC_TAG_DEBUG = 21
DYNAMIC_TAG_TEXTREL = 22
DYNAMIC_TAG_JMPREL = 23
DYNAMIC_TAG_BIND_NOW = 24
DYNAMIC_TAG_INIT_ARRAY = 25
DYNAMIC_TAG_FINI_ARRAY = 26
DYNAMIC_TAG_INIT_ARRAY_SIZE = 27
DYNAMIC_TAG_FINI_ARRAY_SIZE = 28
DYNAMIC_TAG_RUNPATH = 29
DYNAMIC_TAG_FLAGS = 30
DYNAMIC_TAG_PREINIT_ARRAY = 32
DYNAMIC_TAG_PREINIT_ARRAY_SIZE = 33
DYNAMIC_TAG_RELRSZ = 35
DYNAMIC_TAG_RELR = 36
DYNAMIC_TAG_RELRENT = 37

DYNAMIC_FLAG_TEXTREL = 0x4
RELOCATION_AARCH64_RELATIVE = 1027

PAGE_SIZE = 4096
IMAGE_LIMIT = 0x80000000
MAXIMUM_U64 = 0xFFFFFFFFFFFFFFFF
ENTRY_SYMBOL = "burrow_aarch64_entry"
EMERGENCY_VECTOR_SYMBOL = "burrow_aarch64_emergency_vectors"
STABLE_VECTOR_SYMBOL = "burrow_aarch64_stable_vectors"
EMERGENCY_REPORTER_SYMBOL = "burrow_aarch64_emergency_exception"
STABLE_REPORTER_SYMBOL = "burrow_aarch64_stable_exception"
NORMALIZATION_SYMBOL = "burrow_aarch64_normalize"
COMMON_EL1_SYMBOL = "burrow_aarch64_common_el1"
ACTIVATION_SYMBOL = "burrow_aarch64_activate_translation"
KERNEL_ENTRY_SYMBOL = "burrow_kernel_entry"
QEMU_SEMIHOST_HLT = bytes.fromhex("00005ed4")
QEMU_PASS_MARKER = b"WARREN_TEST:1:PASS:aarch64-normalized-entry\r\n\0"
QEMU_FIRST_ENTRY_FAILURE_MARKER_TEMPLATE = (
    b"WARREN_TEST:1:FAIL:burrow-first-entry:00\r\n\0"
)
QEMU_NORMALIZED_FAILURE_MARKER_TEMPLATE = (
    b"WARREN_TEST:1:FAIL:aarch64-normalized-entry:00\r\n\0"
)
QEMU_EXCEPTION_MARKER = b"WARREN_TEST:1:PANIC:aarch64-normalized-entry:4\r\n\0"
QEMU_EXIT_ARGUMENTS = struct.pack("<QQ", 0x20026, 0)
QEMU_RESULT_MARKERS = {
    "pass": QEMU_PASS_MARKER,
    "fail": b"WARREN_TEST:1:FAIL:aarch64-normalized-entry:64\r\n\0",
    "panic": b"WARREN_TEST:1:PANIC:aarch64-normalized-entry:2\r\n\0",
}
QEMU_RESULT_ARGUMENTS = {
    "pass": QEMU_EXIT_ARGUMENTS,
    "fail": struct.pack("<QQ", 0x20026, 64),
    "panic": struct.pack("<QQ", 0x20026, 2),
}

AARCH64_FAULT_SENTINELS = {
    "emergency": "mov w15, #0xf100",
    "planning": "mov w15, #0xf101",
    "unsupported-feature": "mov w15, #0xf102",
    "common-el1": "mov w15, #0xf103",
    "tables": "mov w15, #0xf104",
    "activation": "mov w15, #0xf105",
    "identity-failure": "mov w15, #0xf106",
    "kernel-entry": "mov w15, #0xf107",
    "lower-guard": "mov w15, #0xf110",
    "upper-guard": "mov w15, #0xf111",
    "text-write": "mov w15, #0xf112",
    "data-execute": "mov w15, #0xf113",
    "stale-identity": "mov w15, #0xf114",
    "common-el1-vector": "mov w15, #0xf115",
    "reported-breakpoint": "mov w15, #0xf116",
    "timer-initialization": "mov w15, #0xf117",
    "monitor": "mov w15, #0xf118",
}

AARCH64_FAULT_OPERATIONS = {
    "emergency": "brk #0x777",
    "planning": "mov w0, #0x4d",
    "unsupported-feature": "mov x0, #0xf0000000",
    "common-el1": "mov w0, #0x4c",
    "tables": "str xzr, [x2, #0x308]",
    "activation": "mov w0, #0x4f",
    "identity-failure": "b ",
    "kernel-entry": "mov w0, wzr",
    "lower-guard": "ldr x0, [x0]",
    "upper-guard": "ldr x0, [x0]",
    "text-write": "str xzr, [x0]",
    "data-execute": "br x0",
    "stale-identity": "ldr x0, [x0]",
    "common-el1-vector": "brk #0x779",
    "reported-breakpoint": "brk #0x77a",
    "timer-initialization": "mov w0, #0x1",
    "monitor": "mov w0, #0x1",
}

_FORBIDDEN_PROGRAM_TYPES = {
    PROGRAM_TYPE_INTERPRETER: "PT_INTERP",
    PROGRAM_TYPE_NOTE: "PT_NOTE",
    PROGRAM_TYPE_SHLIB: "PT_SHLIB",
    PROGRAM_TYPE_TLS: "PT_TLS",
}

_FORBIDDEN_SECTION_TYPES = {
    SECTION_TYPE_REL: "SHT_REL",
    SECTION_TYPE_INIT_ARRAY: "SHT_INIT_ARRAY",
    SECTION_TYPE_FINI_ARRAY: "SHT_FINI_ARRAY",
    SECTION_TYPE_PREINIT_ARRAY: "SHT_PREINIT_ARRAY",
    SECTION_TYPE_RELR: "SHT_RELR",
}

_FORBIDDEN_SECTION_NAMES = (
    ".interp",
    ".plt",
    ".init",
    ".fini",
    ".init_array",
    ".fini_array",
    ".preinit_array",
    ".ctors",
    ".dtors",
    ".tdata",
    ".tbss",
    ".eh_frame",
    ".eh_frame_hdr",
    ".gcc_except_table",
)

_FORBIDDEN_DYNAMIC_TAGS = {
    DYNAMIC_TAG_NEEDED: "DT_NEEDED",
    DYNAMIC_TAG_PLTRELSZ: "DT_PLTRELSZ",
    DYNAMIC_TAG_PLTGOT: "DT_PLTGOT",
    DYNAMIC_TAG_INIT: "DT_INIT",
    DYNAMIC_TAG_FINI: "DT_FINI",
    DYNAMIC_TAG_SONAME: "DT_SONAME",
    DYNAMIC_TAG_RPATH: "DT_RPATH",
    DYNAMIC_TAG_SYMBOLIC: "DT_SYMBOLIC",
    DYNAMIC_TAG_REL: "DT_REL",
    DYNAMIC_TAG_RELSZ: "DT_RELSZ",
    DYNAMIC_TAG_RELENT: "DT_RELENT",
    DYNAMIC_TAG_PLTREL: "DT_PLTREL",
    DYNAMIC_TAG_DEBUG: "DT_DEBUG",
    DYNAMIC_TAG_TEXTREL: "DT_TEXTREL",
    DYNAMIC_TAG_JMPREL: "DT_JMPREL",
    DYNAMIC_TAG_BIND_NOW: "DT_BIND_NOW",
    DYNAMIC_TAG_INIT_ARRAY: "DT_INIT_ARRAY",
    DYNAMIC_TAG_FINI_ARRAY: "DT_FINI_ARRAY",
    DYNAMIC_TAG_INIT_ARRAY_SIZE: "DT_INIT_ARRAYSZ",
    DYNAMIC_TAG_FINI_ARRAY_SIZE: "DT_FINI_ARRAYSZ",
    DYNAMIC_TAG_RUNPATH: "DT_RUNPATH",
    DYNAMIC_TAG_PREINIT_ARRAY: "DT_PREINIT_ARRAY",
    DYNAMIC_TAG_PREINIT_ARRAY_SIZE: "DT_PREINIT_ARRAYSZ",
    DYNAMIC_TAG_RELRSZ: "DT_RELRSZ",
    DYNAMIC_TAG_RELR: "DT_RELR",
    DYNAMIC_TAG_RELRENT: "DT_RELRENT",
}

_FORBIDDEN_RUNTIME_SYMBOL_PREFIXES = (
    "__aeabi_",
    "__atomic_",
    "__cxa_",
    "__clang_",
    "__div",
    "__gxx_",
    "__mul",
    "__stack_chk_",
    "__udiv",
    "__umod",
    "_GLOBAL__sub_I_",
    "_Unwind_",
    "_Zd",
    "_Zn",
    "_ZSt",
    "dyld_stub_binder",
)

_FORBIDDEN_RUNTIME_SYMBOLS = {
    "__dso_handle",
    "abort",
    "atexit",
    "calloc",
    "free",
    "malloc",
    "memcmp",
    "memcpy",
    "memmove",
    "memset",
    "realloc",
}


class VerificationError(ValueError):
    """The input does not satisfy Warren's Burrow image contract."""


@dataclasses.dataclass(frozen=True)
class ElfHeader:
    entry: int
    program_offset: int
    section_offset: int
    flags: int
    program_count: int
    section_count: int
    section_name_index: int


@dataclasses.dataclass(frozen=True)
class ProgramHeader:
    index: int
    program_type: int
    flags: int
    offset: int
    virtual_address: int
    physical_address: int
    file_size: int
    memory_size: int
    alignment: int


@dataclasses.dataclass(frozen=True)
class SectionHeader:
    index: int
    name_offset: int
    section_type: int
    flags: int
    address: int
    offset: int
    size: int
    link: int
    information: int
    alignment: int
    entry_size: int
    name: str = ""


@dataclasses.dataclass(frozen=True)
class Symbol:
    name: str
    information: int
    visibility: int
    section_index: int
    value: int
    size: int


@dataclasses.dataclass(frozen=True)
class Relocation:
    offset: int
    relocation_type: int
    symbol_index: int
    addend: int


@dataclasses.dataclass(frozen=True)
class ImageSummary:
    entry: int
    load_segment_count: int
    section_count: int
    symbol_count: int
    relocation_count: int


class Reader:
    """Perform checked reads without allowing Python slicing to hide truncation."""

    def __init__(self, image: bytes) -> None:
        self.image = image

    def range(self, offset: int, size: int, description: str) -> bytes:
        _checked_end(offset, size, len(self.image), description)
        return self.image[offset : offset + size]

    def unpack(
        self, structure: struct.Struct, offset: int, description: str
    ) -> tuple[object, ...]:
        return structure.unpack(self.range(offset, structure.size, description))


def _checked_end(offset: int, size: int, limit: int, description: str) -> int:
    if offset < 0 or size < 0:
        raise VerificationError(f"{description} has a negative range")
    if offset > MAXIMUM_U64 or size > MAXIMUM_U64 - offset:
        raise VerificationError(f"{description} range overflows ELF64")

    end = offset + size
    if end > limit:
        raise VerificationError(f"{description} is truncated")
    return end


def _is_power_of_two(value: int) -> bool:
    return value > 0 and value & (value - 1) == 0


def _round_up_to_page(value: int, description: str) -> int:
    if value > MAXIMUM_U64 - (PAGE_SIZE - 1):
        raise VerificationError(f"{description} page range overflows ELF64")
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)


def _parse_header(reader: Reader) -> ElfHeader:
    values = reader.unpack(ELF_HEADER, 0, "ELF header")
    identification = values[0]
    assert isinstance(identification, bytes)

    if identification[:4] != ELF_MAGIC:
        raise VerificationError("invalid ELF magic")
    if identification[4] != ELF_CLASS_64:
        raise VerificationError("image is not ELF64")
    if identification[5] != ELF_DATA_LITTLE_ENDIAN:
        raise VerificationError("image is not little-endian")
    if identification[6] != ELF_VERSION_CURRENT:
        raise VerificationError("invalid ELF identification version")
    if identification[7] != ELF_OSABI_SYSTEM_V or identification[8] != 0:
        raise VerificationError("image does not use the System V ELF ABI")
    if any(identification[9:]):
        raise VerificationError("ELF identification padding is not zero")

    (
        _,
        elf_type,
        machine,
        version,
        entry,
        program_offset,
        section_offset,
        flags,
        header_size,
        program_entry_size,
        program_count,
        section_entry_size,
        section_count,
        section_name_index,
    ) = values

    if elf_type != ELF_TYPE_DYNAMIC:
        raise VerificationError("image is not ET_DYN")
    if machine != ELF_MACHINE_AARCH64:
        raise VerificationError("image is not AArch64")
    if version != ELF_VERSION_CURRENT:
        raise VerificationError("invalid ELF header version")
    if flags != 0:
        raise VerificationError("AArch64 ELF flags are not zero")
    if header_size != ELF_HEADER.size:
        raise VerificationError("invalid ELF header size")
    if program_count == 0:
        raise VerificationError("image has no program headers")
    if program_entry_size != PROGRAM_HEADER.size:
        raise VerificationError("invalid program-header entry size")
    if section_count == 0:
        raise VerificationError("image has no section headers")
    if section_entry_size != SECTION_HEADER.size:
        raise VerificationError("invalid section-header entry size")
    if section_name_index == 0 or section_name_index >= section_count:
        raise VerificationError("invalid section-name string-table index")

    _checked_end(
        program_offset,
        program_count * PROGRAM_HEADER.size,
        len(reader.image),
        "program-header table",
    )
    _checked_end(
        section_offset,
        section_count * SECTION_HEADER.size,
        len(reader.image),
        "section-header table",
    )

    return ElfHeader(
        entry=entry,
        program_offset=program_offset,
        section_offset=section_offset,
        flags=flags,
        program_count=program_count,
        section_count=section_count,
        section_name_index=section_name_index,
    )


def _parse_program_headers(
    reader: Reader, header: ElfHeader
) -> list[ProgramHeader]:
    program_headers: list[ProgramHeader] = []
    dynamic_count = 0

    for index in range(header.program_count):
        offset = header.program_offset + index * PROGRAM_HEADER.size
        values = reader.unpack(PROGRAM_HEADER, offset, f"program header {index}")
        program_header = ProgramHeader(index, *values)
        program_headers.append(program_header)

        forbidden_name = _FORBIDDEN_PROGRAM_TYPES.get(program_header.program_type)
        if forbidden_name is not None:
            raise VerificationError(f"image contains forbidden {forbidden_name}")
        if program_header.flags & ~0x7:
            raise VerificationError(f"program header {index} has unknown permission bits")
        if program_header.alignment not in (0, 1) and not _is_power_of_two(
            program_header.alignment
        ):
            raise VerificationError(f"program header {index} has invalid alignment")
        if program_header.file_size > 0:
            _checked_end(
                program_header.offset,
                program_header.file_size,
                len(reader.image),
                f"program header {index} file range",
            )

        if program_header.program_type == PROGRAM_TYPE_DYNAMIC:
            dynamic_count += 1
        if (
            program_header.program_type == PROGRAM_TYPE_GNU_STACK
            and program_header.flags & PROGRAM_FLAG_EXECUTE
        ):
            raise VerificationError("PT_GNU_STACK is executable")

    if dynamic_count > 1:
        raise VerificationError("image contains multiple PT_DYNAMIC segments")
    return program_headers


def _validate_load_segments(
    header: ElfHeader, program_headers: list[ProgramHeader]
) -> list[ProgramHeader]:
    loads = [
        program_header
        for program_header in program_headers
        if program_header.program_type == PROGRAM_TYPE_LOAD
    ]
    if len(loads) != 3:
        raise VerificationError("image must contain exactly three PT_LOAD segments")
    if [load.flags for load in loads] != list(PROGRAM_FLAGS_ALLOWED):
        raise VerificationError("PT_LOAD permission classes are not R, RX, and RW")
    first = loads[0]
    if first.offset != 0 or first.virtual_address != 0 or first.physical_address != 0:
        raise VerificationError("first PT_LOAD does not begin at image-relative address zero")
    if [load.virtual_address for load in loads] != sorted(
        load.virtual_address for load in loads
    ):
        raise VerificationError("PT_LOAD segments are not ordered by virtual address")

    previous_page_end = 0
    previous_file_page_end = 0
    for index, load in enumerate(loads):
        if load.memory_size == 0:
            raise VerificationError(f"PT_LOAD {index} is empty")
        if load.file_size > load.memory_size:
            raise VerificationError(f"PT_LOAD {index} has filesz greater than memsz")
        if load.alignment != PAGE_SIZE:
            raise VerificationError(f"PT_LOAD {index} is not 4 KiB aligned")
        if load.offset % PAGE_SIZE != 0 or load.virtual_address % PAGE_SIZE != 0:
            raise VerificationError(f"PT_LOAD {index} does not start on a 4 KiB page")
        if load.offset % load.alignment != load.virtual_address % load.alignment:
            raise VerificationError(f"PT_LOAD {index} violates offset/address congruence")
        if load.physical_address != load.virtual_address:
            raise VerificationError(f"PT_LOAD {index} physical and virtual addresses differ")

        memory_end = _checked_end(
            load.virtual_address,
            load.memory_size,
            IMAGE_LIMIT,
            f"PT_LOAD {index} memory range",
        )
        file_end = _checked_end(
            load.offset,
            load.file_size,
            MAXIMUM_U64,
            f"PT_LOAD {index} file range",
        )
        memory_page_end = _round_up_to_page(memory_end, f"PT_LOAD {index}")
        file_page_end = _round_up_to_page(file_end, f"PT_LOAD {index}")
        if index > 0 and load.virtual_address < previous_page_end:
            raise VerificationError("PT_LOAD memory pages overlap")
        if index > 0 and load.offset < previous_file_page_end:
            raise VerificationError("PT_LOAD file pages overlap")
        previous_page_end = memory_page_end
        previous_file_page_end = file_page_end

    program_table_end = header.program_offset + header.program_count * PROGRAM_HEADER.size
    if header.program_offset < first.offset or program_table_end > first.file_size:
        raise VerificationError("ELF and program headers are not in the first PT_LOAD")

    executable_load = loads[1]
    executable_end = executable_load.virtual_address + executable_load.file_size
    if not executable_load.virtual_address <= header.entry < executable_end:
        raise VerificationError("ELF entry is not in file-backed executable storage")

    return loads


def _parse_section_headers(
    reader: Reader, header: ElfHeader
) -> list[SectionHeader]:
    sections: list[SectionHeader] = []
    for index in range(header.section_count):
        offset = header.section_offset + index * SECTION_HEADER.size
        values = reader.unpack(SECTION_HEADER, offset, f"section header {index}")
        section = SectionHeader(index, *values)
        sections.append(section)

        if section.section_type != SECTION_TYPE_NOBITS and section.size > 0:
            _checked_end(
                section.offset,
                section.size,
                len(reader.image),
                f"section {index} file range",
            )
        if section.alignment not in (0, 1) and not _is_power_of_two(section.alignment):
            raise VerificationError(f"section {index} has invalid alignment")
        if section.entry_size != 0 and section.size % section.entry_size != 0:
            raise VerificationError(f"section {index} size is not entry-aligned")

    null_section = sections[0]
    if any(
        (
            null_section.name_offset,
            null_section.section_type,
            null_section.flags,
            null_section.address,
            null_section.offset,
            null_section.size,
            null_section.link,
            null_section.information,
            null_section.alignment,
            null_section.entry_size,
        )
    ):
        raise VerificationError("section header zero is not null")

    name_table = sections[header.section_name_index]
    if name_table.section_type != SECTION_TYPE_STRING_TABLE:
        raise VerificationError("section-name table is not SHT_STRTAB")
    name_bytes = reader.range(
        name_table.offset, name_table.size, "section-name string table"
    )
    if not name_bytes or name_bytes[0] != 0:
        raise VerificationError("section-name string table has no leading null")

    named_sections: list[SectionHeader] = []
    for section in sections:
        name = _read_string(name_bytes, section.name_offset, f"section {section.index} name")
        named_sections.append(dataclasses.replace(section, name=name))
    return named_sections


def _read_string(table: bytes, offset: int, description: str) -> str:
    if offset >= len(table):
        raise VerificationError(f"{description} is outside its string table")
    terminator = table.find(b"\0", offset)
    if terminator < 0:
        raise VerificationError(f"{description} is unterminated")
    try:
        return table[offset:terminator].decode("ascii")
    except UnicodeDecodeError as error:
        raise VerificationError(f"{description} is not ASCII") from error


def _containing_load(
    loads: list[ProgramHeader], address: int, size: int, *, file_backed: bool
) -> ProgramHeader | None:
    for load in loads:
        extent = load.file_size if file_backed else load.memory_size
        if address >= load.virtual_address and size <= extent:
            relative_address = address - load.virtual_address
            if relative_address <= extent - size:
                return load
    return None


def _validate_sections(
    sections: list[SectionHeader], loads: list[ProgramHeader], *, runtime: bool
) -> None:
    permission_content = {flags: False for flags in PROGRAM_FLAGS_ALLOWED}
    has_zero_filled_storage = False

    for section in sections[1:]:
        forbidden_type = _FORBIDDEN_SECTION_TYPES.get(section.section_type)
        if forbidden_type is not None:
            raise VerificationError(
                f"section {section.name or section.index} uses forbidden {forbidden_type}"
            )
        if section.flags & SECTION_FLAG_TLS:
            raise VerificationError(f"section {section.name or section.index} uses TLS")
        if any(
            section.name == name or section.name.startswith(f"{name}.")
            for name in _FORBIDDEN_SECTION_NAMES
        ):
            raise VerificationError(f"image contains forbidden section {section.name}")
        if runtime and section.name.startswith((".debug", ".zdebug", ".comment", ".note")):
            raise VerificationError(
                f"runtime image contains non-runtime metadata section {section.name}"
            )
        if not section.flags & SECTION_FLAG_ALLOCATE or section.size == 0:
            continue

        file_backed = section.section_type != SECTION_TYPE_NOBITS
        load = _containing_load(loads, section.address, section.size, file_backed=file_backed)
        if load is None:
            storage = "file-backed" if file_backed else "loaded"
            raise VerificationError(
                f"allocated section {section.name or section.index} is outside {storage} PT_LOAD storage"
            )

        expected_flags = PROGRAM_FLAG_READ
        if section.flags & SECTION_FLAG_WRITE:
            expected_flags |= PROGRAM_FLAG_WRITE
        if section.flags & SECTION_FLAG_EXECUTE:
            expected_flags |= PROGRAM_FLAG_EXECUTE
        if load.flags != expected_flags:
            raise VerificationError(
                f"section {section.name or section.index} permission class does not match its PT_LOAD"
            )

        if file_backed:
            expected_offset = load.offset + section.address - load.virtual_address
            if section.offset != expected_offset:
                raise VerificationError(
                    f"section {section.name or section.index} file/address mapping is inconsistent"
                )
            permission_content[load.flags] = True
        elif load.flags == PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE:
            file_backed_end = load.virtual_address + load.file_size
            if section.address < file_backed_end:
                raise VerificationError(
                    f"zero-filled section {section.name or section.index} overlaps file-backed storage"
                )
            has_zero_filled_storage = True

    for flags, has_content in permission_content.items():
        if not has_content:
            raise VerificationError(
                f"PT_LOAD permission class {flags:#x} has no allocated file-backed section"
            )
    if not has_zero_filled_storage:
        raise VerificationError("image has no writable zero-filled allocated section")


def _parse_symbols(reader: Reader, sections: list[SectionHeader]) -> list[Symbol]:
    symbols: list[Symbol] = []
    entry_symbols: list[Symbol] = []

    for section in sections:
        if section.section_type not in (
            SECTION_TYPE_SYMBOL_TABLE,
            SECTION_TYPE_DYNAMIC_SYMBOLS,
        ):
            continue
        if section.entry_size != SYMBOL.size:
            raise VerificationError(
                f"symbol table {section.name or section.index} has invalid entry size"
            )
        if section.link >= len(sections):
            raise VerificationError(
                f"symbol table {section.name or section.index} has invalid string-table link"
            )
        string_section = sections[section.link]
        if string_section.section_type != SECTION_TYPE_STRING_TABLE:
            raise VerificationError(
                f"symbol table {section.name or section.index} does not link to SHT_STRTAB"
            )
        strings = reader.range(
            string_section.offset,
            string_section.size,
            f"symbol strings for {section.name or section.index}",
        )
        if not strings or strings[0] != 0:
            raise VerificationError("symbol string table has no leading null")

        table_symbols: list[Symbol] = []
        for index in range(section.size // SYMBOL.size):
            offset = section.offset + index * SYMBOL.size
            name_offset, information, visibility, section_index, value, size = reader.unpack(
                SYMBOL, offset, f"symbol {index} in {section.name or section.index}"
            )
            name = _read_string(
                strings,
                name_offset,
                f"symbol {index} in {section.name or section.index}",
            )
            symbol = Symbol(name, information, visibility, section_index, value, size)
            table_symbols.append(symbol)
            symbols.append(symbol)

            if index == 0:
                if any((name_offset, information, visibility, section_index, value, size)):
                    raise VerificationError("symbol table entry zero is not null")
                continue
            if section_index == SYMBOL_SECTION_UNDEFINED:
                raise VerificationError(f"image contains undefined symbol {name or index}")
            if section_index in (SYMBOL_SECTION_COMMON, SYMBOL_SECTION_EXTENDED):
                raise VerificationError(f"symbol {name or index} uses an unsupported section index")
            if (
                section_index >= len(sections)
                and section_index != SYMBOL_SECTION_ABSOLUTE
            ):
                raise VerificationError(f"symbol {name or index} has an invalid section index")
            if name in _FORBIDDEN_RUNTIME_SYMBOLS or name.startswith(
                _FORBIDDEN_RUNTIME_SYMBOL_PREFIXES
            ):
                raise VerificationError(f"image contains forbidden runtime symbol {name}")
            if name == ENTRY_SYMBOL:
                entry_symbols.append(symbol)

        if section.information > len(table_symbols):
            raise VerificationError(
                f"symbol table {section.name or section.index} has invalid local-symbol boundary"
            )

    if not symbols:
        raise VerificationError("image contains no symbol table")
    if not entry_symbols:
        raise VerificationError(f"image must define {ENTRY_SYMBOL}")
    entry_contracts = {
        (
            symbol.information,
            symbol.visibility,
            symbol.section_index,
            symbol.value,
            symbol.size,
        )
        for symbol in entry_symbols
    }
    if len(entry_contracts) != 1:
        raise VerificationError(
            f"symbol tables disagree about the {ENTRY_SYMBOL} definition"
        )
    return symbols


def _validate_entry_symbol(
    header: ElfHeader, symbols: list[Symbol], sections: list[SectionHeader]
) -> None:
    entry_symbol = next(symbol for symbol in symbols if symbol.name == ENTRY_SYMBOL)
    binding = entry_symbol.information >> 4
    symbol_type = entry_symbol.information & 0xF
    is_exported = binding == SYMBOL_BINDING_GLOBAL
    is_linker_localized_export = (
        binding == SYMBOL_BINDING_LOCAL
        and entry_symbol.visibility == SYMBOL_VISIBILITY_HIDDEN
    )
    if (
        symbol_type != SYMBOL_TYPE_FUNCTION
        or not (is_exported or is_linker_localized_export)
    ):
        raise VerificationError(
            f"{ENTRY_SYMBOL} is not a retained C-compatible function"
        )
    if entry_symbol.value != header.entry:
        raise VerificationError(f"{ENTRY_SYMBOL} does not match the ELF entry")
    if entry_symbol.size == 0:
        raise VerificationError(f"{ENTRY_SYMBOL} has zero size")
    if entry_symbol.section_index >= len(sections):
        raise VerificationError(f"{ENTRY_SYMBOL} has an invalid section")
    entry_section = sections[entry_symbol.section_index]
    if not entry_section.flags & SECTION_FLAG_EXECUTE:
        raise VerificationError(f"{ENTRY_SYMBOL} is not in an executable section")
    entry_end = _checked_end(
        entry_symbol.value,
        entry_symbol.size,
        IMAGE_LIMIT,
        ENTRY_SYMBOL,
    )
    if (
        entry_symbol.value < entry_section.address
        or entry_end > entry_section.address + entry_section.size
    ):
        raise VerificationError(f"{ENTRY_SYMBOL} extends outside its section")


def _validate_emergency_vector_symbol(
    symbols: list[Symbol], sections: list[SectionHeader]
) -> None:
    vector_symbols = [
        symbol for symbol in symbols if symbol.name == EMERGENCY_VECTOR_SYMBOL
    ]
    if not vector_symbols:
        raise VerificationError(
            f"image must define {EMERGENCY_VECTOR_SYMBOL}"
        )
    contracts = {
        (symbol.information, symbol.section_index, symbol.value, symbol.size)
        for symbol in vector_symbols
    }
    if len(contracts) != 1:
        raise VerificationError("symbol tables disagree about emergency vectors")

    vector_symbol = vector_symbols[0]
    if (vector_symbol.information & 0xF) != SYMBOL_TYPE_FUNCTION:
        raise VerificationError("emergency vectors are not a function symbol")
    if vector_symbol.value % 2048 != 0:
        raise VerificationError("emergency vectors are not 2 KiB-aligned")
    if vector_symbol.size != 2048:
        raise VerificationError("emergency vector table is not exactly 2 KiB")
    if vector_symbol.section_index >= len(sections):
        raise VerificationError("emergency vectors have an invalid section")
    vector_section = sections[vector_symbol.section_index]
    if not vector_section.flags & SECTION_FLAG_EXECUTE:
        raise VerificationError("emergency vectors are not executable")
    if (
        vector_symbol.value < vector_section.address
        or vector_symbol.value + vector_symbol.size
        > vector_section.address + vector_section.size
    ):
        raise VerificationError("emergency vectors extend outside their section")


def _validate_emergency_vector_table(
    reader: Reader,
    loads: list[ProgramHeader],
    symbols: list[Symbol],
    sections: list[SectionHeader],
) -> None:
    vector = next(
        symbol for symbol in symbols if symbol.name == EMERGENCY_VECTOR_SYMBOL
    )
    reporters = [
        symbol for symbol in symbols if symbol.name == EMERGENCY_REPORTER_SYMBOL
    ]
    if not reporters:
        raise VerificationError(f"image must define {EMERGENCY_REPORTER_SYMBOL}")
    reporter_contracts = {
        (symbol.information, symbol.section_index, symbol.value, symbol.size)
        for symbol in reporters
    }
    if len(reporter_contracts) != 1:
        raise VerificationError("symbol tables disagree about the emergency reporter")
    reporter = reporters[0]
    if (reporter.information & 0xF) != SYMBOL_TYPE_FUNCTION:
        raise VerificationError("emergency reporter is not a function symbol")
    if reporter.size == 0:
        raise VerificationError("emergency reporter has zero size")
    if reporter.section_index >= len(sections):
        raise VerificationError("emergency reporter has an invalid section")
    reporter_section = sections[reporter.section_index]
    if not reporter_section.flags & SECTION_FLAG_EXECUTE:
        raise VerificationError("emergency reporter is not executable")
    reporter_end = _checked_end(
        reporter.value,
        reporter.size,
        IMAGE_LIMIT,
        EMERGENCY_REPORTER_SYMBOL,
    )
    if (
        reporter.value < reporter_section.address
        or reporter_end > reporter_section.address + reporter_section.size
    ):
        raise VerificationError("emergency reporter extends outside its section")
    reporter_address = reporter.value

    table_offset = _virtual_file_offset(
        loads,
        vector.value,
        vector.size,
        "emergency vector table",
    )
    table = reader.range(table_offset, vector.size, "emergency vector table")
    for vector_number in range(16):
        slot_offset = vector_number * 128
        move, branch = struct.unpack_from("<II", table, slot_offset)
        expected_move = 0xD2800011 | (vector_number << 5)
        if move != expected_move:
            raise VerificationError(
                f"emergency vector {vector_number} has an invalid classifier encoding"
            )
        if (branch & 0xFC000000) != 0x14000000:
            raise VerificationError(
                f"emergency vector {vector_number} has an invalid branch encoding"
            )
        immediate = branch & 0x03FFFFFF
        if immediate & 0x02000000:
            immediate -= 0x04000000
        branch_address = vector.value + slot_offset + 4
        target = branch_address + immediate * 4
        if target != reporter_address:
            raise VerificationError(
                f"emergency vector {vector_number} branches outside the reporter"
            )
        if any(table[slot_offset + 8 : slot_offset + 128]):
            raise VerificationError(
                f"emergency vector {vector_number} has nonzero slot padding"
            )


def _virtual_file_offset(
    loads: list[ProgramHeader], address: int, size: int, description: str
) -> int:
    load = _containing_load(loads, address, size, file_backed=True)
    if load is None:
        raise VerificationError(f"{description} is outside file-backed PT_LOAD storage")
    return load.offset + address - load.virtual_address


def _parse_relocations(
    reader: Reader,
    loads: list[ProgramHeader],
    address: int,
    size: int,
) -> list[Relocation]:
    if size % RELA_ENTRY.size != 0:
        raise VerificationError("dynamic RELA table size is not entry-aligned")
    file_offset = _virtual_file_offset(loads, address, size, "dynamic RELA table")
    relocations: list[Relocation] = []
    writable_load = next(
        load for load in loads if load.flags == PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE
    )

    for index in range(size // RELA_ENTRY.size):
        relocation_offset, information, addend = reader.unpack(
            RELA_ENTRY, file_offset + index * RELA_ENTRY.size, f"RELA entry {index}"
        )
        relocation_type = information & 0xFFFFFFFF
        symbol_index = information >> 32
        relocation = Relocation(
            relocation_offset, relocation_type, symbol_index, addend
        )
        relocations.append(relocation)

        if relocation_type != RELOCATION_AARCH64_RELATIVE:
            raise VerificationError(
                f"RELA entry {index} is not R_AARCH64_RELATIVE"
            )
        if symbol_index != 0:
            raise VerificationError(f"RELA entry {index} references a symbol")
        if _containing_load(
            [writable_load], relocation_offset, 8, file_backed=False
        ) is None:
            raise VerificationError(
                f"RELA entry {index} target is outside writable loaded storage"
            )
        if addend < 0 or addend >= IMAGE_LIMIT:
            raise VerificationError(f"RELA entry {index} addend is outside the image window")
        if _containing_load(loads, addend, 1, file_backed=False) is None:
            raise VerificationError(
                f"RELA entry {index} addend does not name loaded image storage"
            )

    return relocations


def _parse_dynamic_metadata(
    reader: Reader,
    program_headers: list[ProgramHeader],
    sections: list[SectionHeader],
    loads: list[ProgramHeader],
) -> list[Relocation]:
    dynamic_headers = [
        header
        for header in program_headers
        if header.program_type == PROGRAM_TYPE_DYNAMIC
    ]
    allocated_rela_sections = [
        section
        for section in sections
        if section.section_type == SECTION_TYPE_RELA
        and section.flags & SECTION_FLAG_ALLOCATE
        and section.size > 0
    ]
    allocated_dynamic_sections = [
        section
        for section in sections
        if section.section_type == SECTION_TYPE_DYNAMIC
        and section.flags & SECTION_FLAG_ALLOCATE
    ]

    if not dynamic_headers:
        if allocated_dynamic_sections:
            raise VerificationError("allocated SHT_DYNAMIC has no PT_DYNAMIC segment")
        if allocated_rela_sections:
            raise VerificationError("allocated RELA section has no PT_DYNAMIC metadata")
        return []

    dynamic = dynamic_headers[0]
    if dynamic.file_size == 0 or dynamic.file_size % DYNAMIC_ENTRY.size != 0:
        raise VerificationError("PT_DYNAMIC size is not entry-aligned")
    if dynamic.file_size != dynamic.memory_size:
        raise VerificationError("PT_DYNAMIC filesz and memsz differ")
    if len(allocated_dynamic_sections) != 1:
        raise VerificationError("PT_DYNAMIC requires exactly one allocated SHT_DYNAMIC")
    dynamic_section = allocated_dynamic_sections[0]
    if dynamic_section.entry_size != DYNAMIC_ENTRY.size:
        raise VerificationError("SHT_DYNAMIC has invalid entry size")
    if (
        dynamic_section.offset != dynamic.offset
        or dynamic_section.address != dynamic.virtual_address
        or dynamic_section.size != dynamic.file_size
    ):
        raise VerificationError("SHT_DYNAMIC does not match PT_DYNAMIC")
    containing_load = _containing_load(
        loads, dynamic.virtual_address, dynamic.memory_size, file_backed=True
    )
    if containing_load is None or not containing_load.flags & PROGRAM_FLAG_WRITE:
        raise VerificationError("PT_DYNAMIC is not in writable file-backed storage")
    expected_dynamic_offset = (
        containing_load.offset
        + dynamic.virtual_address
        - containing_load.virtual_address
    )
    if dynamic.offset != expected_dynamic_offset:
        raise VerificationError("PT_DYNAMIC file/address mapping is inconsistent")

    tags: dict[int, int] = {}
    saw_null = False
    for index in range(dynamic.file_size // DYNAMIC_ENTRY.size):
        tag, value = reader.unpack(
            DYNAMIC_ENTRY,
            dynamic.offset + index * DYNAMIC_ENTRY.size,
            f"dynamic entry {index}",
        )
        if tag == DYNAMIC_TAG_NULL:
            saw_null = True
            continue
        if saw_null:
            raise VerificationError("non-null dynamic entry follows DT_NULL")
        forbidden_name = _FORBIDDEN_DYNAMIC_TAGS.get(tag)
        if forbidden_name is not None:
            raise VerificationError(f"image contains forbidden {forbidden_name}")
        if tag in tags:
            raise VerificationError(f"dynamic tag {tag} appears more than once")
        tags[tag] = value

    if not saw_null:
        raise VerificationError("PT_DYNAMIC has no DT_NULL terminator")
    if tags.get(DYNAMIC_TAG_FLAGS, 0) & DYNAMIC_FLAG_TEXTREL:
        raise VerificationError("DT_FLAGS enables text relocations")

    rela_tags = (DYNAMIC_TAG_RELA, DYNAMIC_TAG_RELASZ, DYNAMIC_TAG_RELAENT)
    present_rela_tags = [tag for tag in rela_tags if tag in tags]
    if present_rela_tags and len(present_rela_tags) != len(rela_tags):
        raise VerificationError("dynamic RELA metadata is incomplete")
    if not present_rela_tags:
        if allocated_rela_sections:
            raise VerificationError("allocated RELA section is not described by PT_DYNAMIC")
        return []
    if tags[DYNAMIC_TAG_RELAENT] != RELA_ENTRY.size:
        raise VerificationError("DT_RELAENT is not the ELF64 RELA size")

    relocations = _parse_relocations(
        reader,
        loads,
        tags[DYNAMIC_TAG_RELA],
        tags[DYNAMIC_TAG_RELASZ],
    )

    if len(allocated_rela_sections) != 1:
        raise VerificationError("dynamic relocations require exactly one allocated SHT_RELA")
    rela_section = allocated_rela_sections[0]
    if rela_section.entry_size != RELA_ENTRY.size:
        raise VerificationError("allocated SHT_RELA has invalid entry size")
    if (
        rela_section.address != tags[DYNAMIC_TAG_RELA]
        or rela_section.size != tags[DYNAMIC_TAG_RELASZ]
    ):
        raise VerificationError("SHT_RELA does not match PT_DYNAMIC relocation metadata")
    return relocations


def _validate_qemu_test_result_transport(
    reader: Reader,
    loads: list[ProgramHeader],
    *,
    mode: str | None,
) -> None:
    loaded_bytes = b"".join(
        reader.range(load.offset, load.file_size, f"PT_LOAD {load.index}")
        for load in loads
    )
    hlt_count = loaded_bytes.count(QEMU_SEMIHOST_HLT)
    expected_hlt_count = 1 if mode is not None else 0
    if hlt_count != expected_hlt_count:
        requirement = "exactly once" if mode is not None else "absent"
        raise VerificationError(
            f"QEMU semihost HLT must be {requirement}, found {hlt_count}"
        )

    expected_failure_template_count = 1 if mode is not None else 0
    for marker, name in (
        (QEMU_FIRST_ENTRY_FAILURE_MARKER_TEMPLATE, "first-entry"),
        (QEMU_NORMALIZED_FAILURE_MARKER_TEMPLATE, "normalized-entry"),
    ):
        failure_template_count = loaded_bytes.count(marker)
        if failure_template_count != expected_failure_template_count:
            requirement = "exactly once" if mode is not None else "absent"
            raise VerificationError(
                f"QEMU {name} failure marker template must be "
                f"{requirement}, found {failure_template_count}"
            )

    exception_marker_count = loaded_bytes.count(QEMU_EXCEPTION_MARKER)
    expected_exception_marker_count = 1 if mode is not None else 0
    if exception_marker_count != expected_exception_marker_count:
        requirement = "exactly once" if mode is not None else "absent"
        raise VerificationError(
            "QEMU exception marker must be "
            f"{requirement}, found {exception_marker_count}"
        )

    for result_mode in QEMU_RESULT_MARKERS:
        expected_count = 1 if mode == result_mode else 0
        requirement = "exactly once" if expected_count == 1 else "absent"
        for payload, description in (
            (QEMU_RESULT_MARKERS[result_mode], f"QEMU {result_mode} marker"),
            (QEMU_RESULT_ARGUMENTS[result_mode], f"QEMU {result_mode} argument block"),
        ):
            count = loaded_bytes.count(payload)
            if count == expected_count:
                continue
            raise VerificationError(
                f"{description} must be {requirement}, found {count}"
            )


def verify_image(
    image: bytes,
    *,
    runtime: bool = False,
    qemu_test_result: str | None = None,
) -> ImageSummary:
    """Verify one complete Burrow image and return its audited shape."""

    reader = Reader(image)
    header = _parse_header(reader)
    program_headers = _parse_program_headers(reader, header)
    loads = _validate_load_segments(header, program_headers)
    sections = _parse_section_headers(reader, header)
    _validate_sections(sections, loads, runtime=runtime)
    symbols = _parse_symbols(reader, sections)
    _validate_entry_symbol(header, symbols, sections)
    _validate_emergency_vector_symbol(symbols, sections)
    _validate_emergency_vector_table(reader, loads, symbols, sections)
    relocations = _parse_dynamic_metadata(
        reader, program_headers, sections, loads
    )
    _validate_qemu_test_result_transport(
        reader, loads, mode=qemu_test_result
    )
    return ImageSummary(
        entry=header.entry,
        load_segment_count=len(loads),
        section_count=len(sections),
        symbol_count=len(symbols),
        relocation_count=len(relocations),
    )


def verify_runtime_copy(
    symbol_image: bytes,
    runtime_image: bytes,
    *,
    qemu_test_result: str | None = None,
) -> None:
    """Require debug stripping to preserve every loader-visible byte."""

    symbol_summary = verify_image(
        symbol_image, qemu_test_result=qemu_test_result
    )
    runtime_summary = verify_image(
        runtime_image, runtime=True, qemu_test_result=qemu_test_result
    )
    if symbol_summary.entry != runtime_summary.entry:
        raise VerificationError("runtime copy changes the ELF entry")
    if symbol_summary.relocation_count != runtime_summary.relocation_count:
        raise VerificationError("runtime copy changes the relocation count")

    symbol_reader = Reader(symbol_image)
    runtime_reader = Reader(runtime_image)
    symbol_header = _parse_header(symbol_reader)
    runtime_header = _parse_header(runtime_reader)
    symbol_programs = _parse_program_headers(symbol_reader, symbol_header)
    runtime_programs = _parse_program_headers(runtime_reader, runtime_header)
    if symbol_programs != runtime_programs:
        raise VerificationError("runtime copy changes the program-header table")

    for program_header in symbol_programs:
        if program_header.program_type != PROGRAM_TYPE_LOAD:
            continue
        symbol_bytes = bytearray(
            symbol_reader.range(
                program_header.offset,
                program_header.file_size,
                f"symbol image PT_LOAD {program_header.index}",
            )
        )
        runtime_bytes = bytearray(
            runtime_reader.range(
                program_header.offset,
                program_header.file_size,
                f"runtime image PT_LOAD {program_header.index}",
            )
        )

        # Debug stripping is allowed to relocate and resize the non-loaded
        # section-header table. Its ELF-header locator and counts therefore
        # differ even though the program headers and all loader-consumed bytes
        # retain their meaning. No other loaded byte may change.
        if program_header.offset == 0:
            for start, end in ((40, 48), (60, 64)):
                symbol_bytes[start:end] = bytes(end - start)
                runtime_bytes[start:end] = bytes(end - start)
        if symbol_bytes != runtime_bytes:
            raise VerificationError(
                f"runtime copy changes loaded bytes in PT_LOAD {program_header.index}"
            )


def verify_first_entry_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    qemu_test_result: str | None,
    aarch64_entry_fault: str | None,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={ENTRY_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble the Burrow first entry")

    instructions: list[str] = []
    for line in result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        instructions.append(re.sub(r"\s+", " ", instruction))
    if not instructions:
        raise VerificationError("Burrow first-entry disassembly is empty")

    cursor = 0
    for fragment in (
        "mov x20, x0",
        "mrs x21, CurrentEL",
        "mrs x23, DAIF",
        "msr VBAR_EL2",
        "msr VBAR_EL1",
        "isb",
        "stp x20, x21",
        "bl ",
        "msr DAIFSet, #0xf",
        "wfe",
    ):
        while cursor < len(instructions) and fragment not in instructions[cursor]:
            cursor += 1
        if cursor == len(instructions):
            raise VerificationError(
                f"Burrow first entry is missing ordered instruction {fragment}"
            )
        cursor += 1

    preparation_branches = [
        instruction for instruction in instructions
        if instruction.startswith("bl ") and "burrow_aarch64_prepare_transition" in instruction
    ]
    if len(preparation_branches) != 1:
        raise VerificationError(
            "Burrow first entry must call transition preparation exactly once"
        )

    forbidden_registers = (
        "SPSR_EL2",
        "HCR_EL2",
        "SCTLR_EL1",
        "TCR_EL1",
        "MAIR_EL1",
        "TTBR0_EL1",
        "TTBR1_EL1",
    )
    if any(instruction.startswith(("eret", "hvc", "smc", "svc"))
           for instruction in instructions):
        raise VerificationError("Burrow first entry contains a forbidden exception path")
    if any(register in instruction for instruction in instructions
           for register in forbidden_registers):
        raise VerificationError("Burrow first entry modifies normalization state")

    if any(instruction.startswith("hlt") for instruction in instructions):
        raise VerificationError(
            "Burrow first entry directly contains semihosting"
        )
    brk_instructions = [
        instruction for instruction in instructions if instruction.startswith("brk")
    ]
    expected_brk = ["brk #0x777"] if aarch64_entry_fault == "emergency" else []
    if brk_instructions != expected_brk:
        raise VerificationError(
            "Burrow first entry has an unexpected emergency fault injection"
        )
    for code in range(65, 73):
        expected = f"mov w0, #0x{code:x}"
        locations = [
            index for index, instruction in enumerate(instructions)
            if instruction == expected
        ]
        if len(locations) != 1:
            raise VerificationError(
                f"Burrow first entry must classify failure code {code} exactly once"
            )
        location = locations[0]
        if location + 1 == len(instructions) or not instructions[location + 1].startswith("b "):
            raise VerificationError(
                f"Burrow first-entry failure code {code} does not terminate"
            )
    branches_to_transport = [
        instruction for instruction in instructions
        if instruction.startswith("b ") and "burrow_qemu_test_result" in instruction
    ]
    if branches_to_transport:
        raise VerificationError("Burrow first entry has an unexpected QEMU transport branch")
    branches_to_failure_transport = [
        instruction for instruction in instructions
        if instruction.startswith("b ") and "burrow_qemu_test_failure" in instruction
    ]
    if len(branches_to_failure_transport) != (1 if qemu_test_result is not None else 0):
        raise VerificationError(
            "Burrow first entry has an unexpected QEMU failure-transport branch"
        )


def verify_emergency_vectors_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    qemu_test_result: str | None,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={EMERGENCY_VECTOR_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble the emergency vectors")

    symbol_match = re.search(
        rf"^([0-9a-fA-F]+) <{EMERGENCY_VECTOR_SYMBOL}>:$",
        result.stdout,
        re.MULTILINE,
    )
    if symbol_match is None:
        raise VerificationError("emergency vector symbol address is missing")
    base = int(symbol_match.group(1), 16)

    table_result = subprocess.run(
        [
            str(objdump),
            "--disassemble",
            f"--start-address={base:#x}",
            f"--stop-address={base + 2048:#x}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if table_result.returncode != 0:
        raise VerificationError("could not disassemble the complete emergency table")

    instructions: dict[int, str] = {}
    for line in table_result.stdout.splitlines():
        stripped = line.strip()
        match = re.match(r"^([0-9a-fA-F]+):", stripped)
        if match is None:
            continue
        instruction = stripped.split(":", 1)[1].split("//", 1)[0].strip()
        instructions[int(match.group(1), 16)] = re.sub(r"\s+", " ", instruction)
    if not instructions:
        raise VerificationError("emergency vector disassembly is empty")

    if base % 2048 != 0:
        raise VerificationError("emergency vector disassembly is not aligned")
    for vector in range(16):
        address = base + vector * 128
        expected_move = f"mov x17, #0x{vector:x}"
        if instructions.get(address) != expected_move:
            raise VerificationError(
                f"emergency vector {vector} does not begin with its classification"
            )
        branch = instructions.get(address + 4, "")
        if not branch.startswith("b ") or "burrow_aarch64_emergency_exception" not in branch:
            raise VerificationError(
                f"emergency vector {vector} does not branch to the terminal reporter"
            )

    stable_result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={STABLE_VECTOR_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if stable_result.returncode != 0:
        raise VerificationError("could not disassemble the stable vectors")
    stable_match = re.search(
        rf"^([0-9a-fA-F]+) <{STABLE_VECTOR_SYMBOL}>:$",
        stable_result.stdout,
        re.MULTILINE,
    )
    if stable_match is None:
        raise VerificationError("stable vector symbol address is missing")
    stable_base = int(stable_match.group(1), 16)
    if stable_base % 2048 != 0:
        raise VerificationError("stable vector disassembly is not aligned")
    if stable_base == base:
        raise VerificationError("stable vectors alias the inherited emergency table")
    stable_table_result = subprocess.run(
        [
            str(objdump),
            "--disassemble",
            f"--start-address={stable_base:#x}",
            f"--stop-address={stable_base + 2048:#x}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    stable_instructions: dict[int, str] = {}
    for line in stable_table_result.stdout.splitlines():
        stripped = line.strip()
        match = re.match(r"^([0-9a-fA-F]+):", stripped)
        if match is None:
            continue
        instruction = stripped.split(":", 1)[1].split("//", 1)[0].strip()
        stable_instructions[int(match.group(1), 16)] = re.sub(
            r"\s+", " ", instruction
        )
    for vector in range(16):
        address = stable_base + vector * 128
        if stable_instructions.get(address) != "sub sp, sp, #0x140":
            raise VerificationError(
                f"stable vector {vector} does not allocate the fixed frame"
            )
        save = stable_instructions.get(address + 4, "")
        if not re.fullmatch(r"stp x0, x1, \[sp, #(?:0x)?10\]", save):
            raise VerificationError(
                f"stable vector {vector} does not preserve x0 and x1"
            )
        if stable_instructions.get(address + 8) != f"mov x0, #0x{vector:x}":
            raise VerificationError(
                f"stable vector {vector} does not classify its source"
            )
        branch = stable_instructions.get(address + 12, "")
        if not branch.startswith("b ") or STABLE_REPORTER_SYMBOL not in branch:
            raise VerificationError(
                f"stable vector {vector} does not branch to the stable reporter"
            )

    stable_reporter_result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={STABLE_REPORTER_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if stable_reporter_result.returncode != 0:
        raise VerificationError("could not disassemble the stable reporter")
    stable_reporter = re.sub(r"\s+", " ", stable_reporter_result.stdout)
    for fragment in (
        "stp x2, x3, [sp, #0x20]",
        "stp x4, x5, [sp, #0x30]",
        "stp x6, x7, [sp, #0x40]",
        "stp x8, x9, [sp, #0x50]",
        "stp x10, x11, [sp, #0x60]",
        "stp x12, x13, [sp, #0x70]",
        "stp x14, x15, [sp, #0x80]",
        "stp x16, x17, [sp, #0x90]",
        "stp x18, x19, [sp, #0xa0]",
        "stp x20, x21, [sp, #0xb0]",
        "stp x22, x23, [sp, #0xc0]",
        "stp x24, x25, [sp, #0xd0]",
        "stp x26, x27, [sp, #0xe0]",
        "stp x28, x29, [sp, #0xf0]",
        "str x30, [sp, #0x100]",
        "mrs x1, ESR_EL1",
        "mrs x1, ELR_EL1",
        "mrs x1, FAR_EL1",
        "mrs x1, SPSR_EL1",
        "burrow_aarch64_dispatch_irq",
        "ldr x30, [sp, #0x100]",
        "ldp x28, x29, [sp, #0xf0]",
        "ldp x26, x27, [sp, #0xe0]",
        "ldp x24, x25, [sp, #0xd0]",
        "ldp x22, x23, [sp, #0xc0]",
        "ldp x20, x21, [sp, #0xb0]",
        "ldp x18, x19, [sp, #0xa0]",
        "ldp x16, x17, [sp, #0x90]",
        "ldp x14, x15, [sp, #0x80]",
        "ldp x12, x13, [sp, #0x70]",
        "ldp x10, x11, [sp, #0x60]",
        "ldp x8, x9, [sp, #0x50]",
        "ldp x6, x7, [sp, #0x40]",
        "ldp x4, x5, [sp, #0x30]",
        "ldp x2, x3, [sp, #0x20]",
        "ldp x0, x1, [sp, #0x10]",
        "add sp, sp, #0x140",
        "eret",
        "burrow_aarch64_report_exception",
    ):
        if fragment not in stable_reporter:
            raise VerificationError(
                f"stable reporter is missing instruction {fragment}"
            )

    reporter_result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_aarch64_emergency_exception",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if reporter_result.returncode != 0:
        raise VerificationError("could not disassemble the emergency reporter")
    reporter = re.sub(r"\s+", " ", reporter_result.stdout)
    for fragment in (
        "msr DAIFSet, #0xf",
        "mrs x15, CurrentEL",
        "mrs x12, ESR_EL2",
        "mrs x13, ELR_EL2",
        "mrs x14, FAR_EL2",
        "mrs x15, SPSR_EL2",
        "mrs x12, ESR_EL1",
        "mrs x13, ELR_EL1",
        "mrs x14, FAR_EL1",
        "mrs x15, SPSR_EL1",
        "wfe",
    ):
        if fragment not in reporter:
            raise VerificationError(
                f"emergency reporter is missing instruction {fragment}"
            )
    has_test_branch = "burrow_qemu_test_exception" in reporter
    if has_test_branch != (qemu_test_result is not None):
        raise VerificationError(
            "emergency reporter has an unexpected QEMU exception branch"
        )


def verify_transition_preparation_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_aarch64_prepare_transition",
            "--demangle",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble transition preparation")

    output = re.sub(r"\s+", " ", result.stdout)
    for fragment in (
        "burrow::core::consume_boot_information",
        "burrow::core::plan_aarch64_transition",
    ):
        if fragment not in output:
            raise VerificationError(
                f"transition preparation is missing {fragment}"
            )
    for value, name in (("4a", "validation"), ("4d", "planning")):
        if re.search(rf"\bmov\s+w[0-9]+,\s*#0x{value}\b", result.stdout) is None:
            raise VerificationError(
                f"transition preparation is missing the {name} failure code"
            )


def verify_normalization_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    qemu_test_result: str | None,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={NORMALIZATION_SYMBOL},{COMMON_EL1_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble AArch64 normalization")

    common_match = re.search(
        rf"^([0-9a-fA-F]+) <{COMMON_EL1_SYMBOL}>:$",
        result.stdout,
        re.MULTILINE,
    )
    if common_match is None:
        raise VerificationError("common EL1 symbol address is missing")
    common_address = int(common_match.group(1), 16)

    addressed_instructions: list[tuple[int, str]] = []
    for line in result.stdout.splitlines():
        match = re.match(r"^([0-9a-fA-F]+):", line.strip())
        if match is None:
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        addressed_instructions.append(
            (int(match.group(1), 16), re.sub(r"\s+", " ", instruction))
        )
    if not addressed_instructions:
        raise VerificationError("AArch64 normalization disassembly is empty")

    pre_common = [
        instruction for address, instruction in addressed_instructions
        if address < common_address
    ]
    common_and_helpers = [
        instruction for address, instruction in addressed_instructions
        if address >= common_address
    ]

    def require_ordered(instructions: list[str], fragments: tuple[str, ...], name: str) -> None:
        cursor = 0
        for fragment in fragments:
            while cursor < len(instructions) and fragment not in instructions[cursor]:
                cursor += 1
            if cursor == len(instructions):
                raise VerificationError(
                    f"{name} is missing ordered instruction {fragment}"
                )
            cursor += 1

    require_ordered(
        pre_common,
        (
            "cmp x21, #0x4",
            "mrs x0, SCTLR_EL1",
            "mov x1, #0x1005",
            "bic x0, x0, x1",
            "msr SCTLR_EL1, x0",
            "isb",
            "ic iallu",
            "dsb sy",
            "isb",
            "msr CPACR_EL1, xzr",
            "msr CNTKCTL_EL1, xzr",
            COMMON_EL1_SYMBOL,
        ),
        "initial EL1 normalization route",
    )

    el1_start = next(
        index for index, instruction in enumerate(pre_common)
        if instruction == "mrs x0, SCTLR_EL1"
    )
    el1_end = next(
        index for index in range(el1_start, len(pre_common))
        if COMMON_EL1_SYMBOL in pre_common[index]
    )
    if any("_EL2" in instruction for instruction in pre_common[el1_start:el1_end + 1]):
        raise VerificationError("initial EL1 normalization route accesses an EL2 register")

    require_ordered(
        pre_common,
        (
            "mrs x0, SCTLR_EL2",
            "msr SCTLR_EL2, x0",
            "isb",
            "ic iallu",
            "dsb sy",
            "isb",
            "mov x0, #0x80000000",
            "msr HCR_EL2, x0",
            "msr CPTR_EL2, xzr",
            "mov x0, #0x3",
            "msr CNTHCTL_EL2, x0",
            "msr CNTVOFF_EL2, xzr",
            "mov x0, #0x800",
            "movk x0, #0x30d0, lsl #16",
            "msr SCTLR_EL1, x0",
            "msr CPACR_EL1, xzr",
            "msr CNTKCTL_EL1, xzr",
            "msr VBAR_EL1, x0",
            "msr SP_EL1, x22",
            "msr ELR_EL2, x0",
            "mov x0, #0x3c5",
            "msr SPSR_EL2, x0",
            "dsb sy",
            "isb",
            "eret",
        ),
        "initial EL2 normalization route",
    )
    if sum(instruction == "eret" for instruction in pre_common) != 1:
        raise VerificationError("initial EL2 route must contain exactly one ERET")

    require_ordered(
        common_and_helpers,
        (
            "mrs x0, CurrentEL",
            "cmp x0, #0x4",
            "mrs x0, DAIF",
            "and x0, x0, #0x3c0",
            "mov x0, sp",
            "cmp x0, x22",
            "mrs x0, SCTLR_EL1",
            "tst x0, x1",
            "burrow_image_start",
            "ldr x1, [x20, #0x30]",
            "mrs x0, VBAR_EL1",
            "burrow_aarch64_emergency_vectors",
            "mov x1, #0x3",
            "str x1, [x0]",
            "mrs x0, ID_AA64MMFR0_EL1",
            "burrow_aarch64_build_transition_tables",
            ACTIVATION_SYMBOL,
            "mov w0, #0x4c",
            "msr DAIFSet, #0xf",
            "wfe",
            "mrs x0, CLIDR_EL1",
            "mrs x2, CCSIDR_EL1",
            "dc cisw, x9",
            "dsb sy",
            "isb",
        ),
        "common EL1 proof and cache walk",
    )

    result_branches = [
        instruction for _, instruction in addressed_instructions
        if instruction.startswith("b ") and "burrow_qemu_test_result" in instruction
    ]
    failure_branches = [
        instruction for _, instruction in addressed_instructions
        if instruction.startswith("b ") and "burrow_qemu_test_failure" in instruction
    ]
    if result_branches:
        raise VerificationError("normalization has an unexpected QEMU result branch")
    expected_transport_count = 1 if qemu_test_result is not None else 0
    if len(failure_branches) != expected_transport_count:
        raise VerificationError("normalization has an unexpected QEMU failure branch")


def verify_table_preparation_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_aarch64_build_transition_tables,burrow_aarch64_preflight_activation",
            "--demangle",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble table preparation")

    output = re.sub(r"\s+", " ", result.stdout)
    for fragment in (
        "burrow::arch::aarch64::build_page_tables",
        "burrow::arch::aarch64::audit_page_tables",
        "burrow::arch::aarch64::preflight_activation",
    ):
        if fragment not in output:
            raise VerificationError(f"table preparation is missing {fragment}")
    for value, name in (("4b", "architecture"), ("4e", "table"), ("4f", "activation")):
        if re.search(rf"\bmov\s+w[0-9]+,\s*#0x{value}\b", result.stdout) is None:
            raise VerificationError(
                f"table preparation is missing the {name} failure code"
            )


def verify_activation_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    qemu_test_result: str | None,
    aarch64_entry_fault: str | None,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            f"--disassemble-symbols={ACTIVATION_SYMBOL}",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble AArch64 activation")

    instructions: list[str] = []
    for line in result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        instructions.append(re.sub(r"\s+", " ", instruction))
    if not instructions:
        raise VerificationError("AArch64 activation disassembly is empty")

    ordered_fragments = [
        "burrow_aarch64_preflight_activation",
        "dsb sy",
        "msr MAIR_EL1, x9",
        "msr TCR_EL1, x10",
        "msr TTBR0_EL1, x11",
        "msr TTBR1_EL1, x12",
        "isb",
        "tlbi vmalle1",
        "dsb ish",
        "isb",
        "msr SCTLR_EL1, x13",
        "isb",
        "br x26",
        "msr VBAR_EL1, x27",
        "isb",
        "mov sp, x16",
        "dsb ishst",
        "msr TTBR0_EL1, x17",
        "isb",
        "tlbi vmalle1is",
        "dsb ish",
        "isb",
        "mrs x0, CurrentEL",
        "mrs x0, TTBR0_EL1",
        "mrs x0, TTBR1_EL1",
        "mrs x0, VBAR_EL1",
        "mrs x0, TCR_EL1",
        "mrs x0, MAIR_EL1",
        "mrs x0, SCTLR_EL1",
        "mov x1, #0x8",
        "str x1, [x0]",
        "sub sp, sp, #0x40",
        "mov x0, sp",
        "mov x1, xzr",
        "mov x7, xzr",
        KERNEL_ENTRY_SYMBOL,
        "mov w1, #0x5231",
        "movk w1, #0x5741, lsl #16",
        "cmp w0, w1",
        "mov sp, x22",
        "mov x1, #0x9",
        "str x1, [x0]",
        "burrow_qemu_virt_start_timer",
        "msr DAIFClr, #0x2",
        "sevl",
        "wfe",
        "burrow_qemu_virt_publish_timer_tick",
    ]
    if aarch64_entry_fault != "monitor":
        ordered_fragments.append("burrow_qemu_virt_run_monitor")

    cursor = 0
    for fragment in ordered_fragments:
        while cursor < len(instructions) and fragment not in instructions[cursor]:
            cursor += 1
        if cursor == len(instructions):
            raise VerificationError(
                f"AArch64 activation is missing ordered instruction {fragment}"
            )
        cursor += 1

    result_branches = [
        instruction for instruction in instructions
        if instruction.startswith("b ") and "burrow_qemu_test_result" in instruction
    ]
    low_failure_branches = [
        instruction for instruction in instructions
        if instruction.startswith("b ") and
        "burrow_qemu_test_failure>" in instruction
    ]
    stable_failure_branches = [
        instruction for instruction in instructions
        if instruction.startswith("b ") and
        "burrow_qemu_test_failure_stable" in instruction
    ]
    expected = 1 if qemu_test_result is not None else 0
    if len(result_branches) != expected or len(low_failure_branches) != expected or \
            len(stable_failure_branches) != 4 * expected:
        raise VerificationError("activation has an unexpected QEMU transport branch")

    boundary_begin = next(
        index for index, instruction in enumerate(instructions)
        if instruction == "dsb sy"
    )
    boundary_end = next(
        index for index in range(boundary_begin, len(instructions))
        if instructions[index] == "br x26"
    )
    forbidden_boundary_instructions = [
        instruction for instruction in instructions[boundary_begin:boundary_end + 1]
        if instruction.startswith(("bl ", "ret")) or "sp" in instruction
    ]
    if forbidden_boundary_instructions:
        raise VerificationError(
            "activation register boundary uses a call, return, or stack reference"
        )

    kernel_entry_calls = [
        instruction for instruction in instructions
        if instruction.startswith("bl ") and KERNEL_ENTRY_SYMBOL in instruction
    ]
    if len(kernel_entry_calls) != 1:
        raise VerificationError("activation must call burrow_kernel_entry exactly once")


def verify_qemu_result_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    mode: str | None,
) -> None:
    if mode is None:
        return

    result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_qemu_emit_result",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble the QEMU result transport")

    instructions = []
    for line in result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        instructions.append(re.sub(r"\s+", " ", instruction))
    if not instructions:
        raise VerificationError("QEMU result transport disassembly is empty")

    cursor = 0
    for fragment in (
        "bl ",
        "ldr w5, [x24, #0x18]",
        "tst w5, #0x8",
        "mov w0, #0x20",
        "adr x1,",
        "hlt #0xf000",
        "msr DAIFSet, #0xf",
        "wfe",
    ):
        while cursor < len(instructions) and fragment not in instructions[cursor]:
            cursor += 1
        if cursor == len(instructions):
            raise VerificationError(
                f"QEMU result transport is missing ordered instruction {fragment}"
            )
        cursor += 1

    if [instruction for instruction in instructions if instruction.startswith("hlt")] != ["hlt #0xf000"]:
        raise VerificationError("QEMU result transport must contain exactly one HLT #0xF000")
    if any(instruction.startswith(("brk", "eret", "hvc", "smc", "svc"))
           for instruction in instructions):
        raise VerificationError("QEMU result transport uses an unexpected trap")

    failure_result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_qemu_test_failure,burrow_qemu_test_failure_stable",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if failure_result.returncode != 0:
        raise VerificationError("could not disassemble the QEMU failure transport")

    failure_instructions = []
    for line in failure_result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        failure_instructions.append(re.sub(r"\s+", " ", instruction))
    if not failure_instructions:
        raise VerificationError("QEMU failure transport disassembly is empty")

    for fragment in (
        "cmp w0, #0x41",
        "cmp w0, #0x53",
        "mov x24, #0x9000000",
        "udiv w7, w19, w6",
        "msub w8, w7, w6, w19",
        "strb w7, [x18]",
        "strb w8, [x18, #0x1]",
        "str x5, [x6, #0x8]",
        "b ",
        "msr DAIFSet, #0xf",
        "wfe",
    ):
        if not any(fragment in instruction for instruction in failure_instructions):
            raise VerificationError(
                f"QEMU failure transport is missing instruction {fragment}"
            )
    if not any(
        instruction.startswith("b ") and "burrow_qemu_emit_result" in instruction
        for instruction in failure_instructions
    ):
        raise VerificationError("QEMU failure transport does not use the common emitter")

    exception_result = subprocess.run(
        [
            str(objdump),
            "--disassemble-symbols=burrow_qemu_test_exception",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if exception_result.returncode != 0:
        raise VerificationError("could not disassemble the QEMU exception transport")
    exception_instructions = re.sub(r"\s+", " ", exception_result.stdout)
    for fragment in (
        "mov x24, x0",
        "mov x5, #0x4",
        "str x5, [x6, #0x8]",
        "burrow_qemu_emit_result",
    ):
        if fragment not in exception_instructions:
            raise VerificationError(
                f"QEMU exception transport is missing instruction {fragment}"
            )


def verify_aarch64_fault_injection_disassembly(
    objdump: pathlib.Path,
    image: pathlib.Path,
    *,
    selected_fault: str | None,
) -> None:
    result = subprocess.run(
        [
            str(objdump),
            "--disassemble",
            "--no-show-raw-insn",
            str(image),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise VerificationError("could not disassemble AArch64 fault injection")

    instructions: list[str] = []
    for line in result.stdout.splitlines():
        if not re.match(r"^[0-9a-fA-F]+:", line.strip()):
            continue
        instruction = line.split(":", 1)[1].split("//", 1)[0].strip()
        instructions.append(re.sub(r"\s+", " ", instruction))

    observed = [
        (fault, index)
        for fault, sentinel in AARCH64_FAULT_SENTINELS.items()
        for index, instruction in enumerate(instructions)
        if instruction == sentinel
    ]
    expected = [] if selected_fault is None else [selected_fault]
    if [fault for fault, _ in observed] != expected:
        raise VerificationError(
            "AArch64 fault injection sentinels do not match the selected fixture"
        )
    if selected_fault is None:
        return

    sentinel_index = observed[0][1]
    operation = AARCH64_FAULT_OPERATIONS[selected_fault]
    window = instructions[sentinel_index + 1 : sentinel_index + 9]
    if operation == "b ":
        matched = any(instruction.startswith(operation) for instruction in window)
    else:
        matched = operation in window
    if not matched:
        raise VerificationError(
            f"AArch64 {selected_fault} fixture is missing its fault operation"
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify Warren's AArch64 Burrow ELF image contract."
    )
    parser.add_argument("--image", required=True, type=pathlib.Path)
    parser.add_argument("--objdump", required=True, type=pathlib.Path)
    parser.add_argument(
        "--reference-image",
        type=pathlib.Path,
        help="unstripped symbol image whose loader-visible bytes must match",
    )
    parser.add_argument(
        "--runtime",
        action="store_true",
        help="also reject debug, comment, and note metadata",
    )
    parser.add_argument(
        "--qemu-test-result",
        choices=("pass", "fail", "panic"),
        help="require the selected isolated QEMU result transport",
    )
    parser.add_argument(
        "--aarch64-entry-fault",
        choices=tuple(AARCH64_FAULT_SENTINELS),
        help="require one selected test-only AArch64 entry fault",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        image = arguments.image.read_bytes()
        summary = verify_image(
            image,
            runtime=arguments.runtime,
            qemu_test_result=arguments.qemu_test_result,
        )
        verify_first_entry_disassembly(
            arguments.objdump,
            arguments.image,
            qemu_test_result=arguments.qemu_test_result,
            aarch64_entry_fault=arguments.aarch64_entry_fault,
        )
        verify_emergency_vectors_disassembly(
            arguments.objdump,
            arguments.image,
            qemu_test_result=arguments.qemu_test_result,
        )
        verify_transition_preparation_disassembly(
            arguments.objdump,
            arguments.image,
        )
        verify_normalization_disassembly(
            arguments.objdump,
            arguments.image,
            qemu_test_result=arguments.qemu_test_result,
        )
        verify_table_preparation_disassembly(
            arguments.objdump,
            arguments.image,
        )
        verify_activation_disassembly(
            arguments.objdump,
            arguments.image,
            qemu_test_result=arguments.qemu_test_result,
            aarch64_entry_fault=arguments.aarch64_entry_fault,
        )
        verify_qemu_result_disassembly(
            arguments.objdump,
            arguments.image,
            mode=arguments.qemu_test_result,
        )
        verify_aarch64_fault_injection_disassembly(
            arguments.objdump,
            arguments.image,
            selected_fault=arguments.aarch64_entry_fault,
        )
        if arguments.reference_image is not None:
            if not arguments.runtime:
                raise VerificationError(
                    "--reference-image requires runtime-image verification"
                )
            reference_image = arguments.reference_image.read_bytes()
            verify_runtime_copy(
                reference_image,
                image,
                qemu_test_result=arguments.qemu_test_result,
            )
    except (OSError, VerificationError) as error:
        print(f"Burrow image verification failed: {error}", file=sys.stderr)
        return 1

    print(
        f"Verified AArch64 Burrow ET_DYN image: {arguments.image} "
        f"({summary.load_segment_count} loads, "
        f"{summary.relocation_count} relocations)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
