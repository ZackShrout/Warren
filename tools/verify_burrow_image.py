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
import struct
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


def verify_image(image: bytes, *, runtime: bool = False) -> ImageSummary:
    """Verify one complete Burrow image and return its audited shape."""

    reader = Reader(image)
    header = _parse_header(reader)
    program_headers = _parse_program_headers(reader, header)
    loads = _validate_load_segments(header, program_headers)
    sections = _parse_section_headers(reader, header)
    _validate_sections(sections, loads, runtime=runtime)
    symbols = _parse_symbols(reader, sections)
    _validate_entry_symbol(header, symbols, sections)
    relocations = _parse_dynamic_metadata(
        reader, program_headers, sections, loads
    )
    return ImageSummary(
        entry=header.entry,
        load_segment_count=len(loads),
        section_count=len(sections),
        symbol_count=len(symbols),
        relocation_count=len(relocations),
    )


def verify_runtime_copy(symbol_image: bytes, runtime_image: bytes) -> None:
    """Require debug stripping to preserve every loader-visible byte."""

    symbol_summary = verify_image(symbol_image)
    runtime_summary = verify_image(runtime_image, runtime=True)
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


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify Warren's AArch64 Burrow ELF image contract."
    )
    parser.add_argument("--image", required=True, type=pathlib.Path)
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
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        image = arguments.image.read_bytes()
        summary = verify_image(image, runtime=arguments.runtime)
        if arguments.reference_image is not None:
            if not arguments.runtime:
                raise VerificationError(
                    "--reference-image requires runtime-image verification"
                )
            reference_image = arguments.reference_image.read_bytes()
            verify_runtime_copy(reference_image, image)
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
