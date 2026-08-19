#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import dataclasses
import pathlib
import struct
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from verify_burrow_image import (  # noqa: E402
    DYNAMIC_TAG_FLAGS,
    DYNAMIC_TAG_NEEDED,
    DYNAMIC_TAG_NULL,
    DYNAMIC_TAG_RELA,
    DYNAMIC_TAG_RELAENT,
    DYNAMIC_TAG_RELASZ,
    ELF_HEADER,
    PROGRAM_FLAG_EXECUTE,
    PROGRAM_FLAG_READ,
    PROGRAM_FLAG_WRITE,
    PROGRAM_HEADER,
    PROGRAM_TYPE_DYNAMIC,
    PROGRAM_TYPE_INTERPRETER,
    QEMU_EXIT_ARGUMENTS,
    QEMU_PASS_MARKER,
    QEMU_SEMIHOST_HLT,
    RELOCATION_AARCH64_RELATIVE,
    SECTION_FLAG_ALLOCATE,
    SECTION_FLAG_EXECUTE,
    SECTION_FLAG_TLS,
    SECTION_FLAG_WRITE,
    SECTION_HEADER,
    SECTION_TYPE_DYNAMIC,
    SECTION_TYPE_NOBITS,
    SECTION_TYPE_PROGBITS,
    SECTION_TYPE_RELA,
    SECTION_TYPE_STRING_TABLE,
    SECTION_TYPE_SYMBOL_TABLE,
    SYMBOL,
    VerificationError,
    verify_image,
    verify_runtime_copy,
)


PROGRAM_OFFSET = ELF_HEADER.size
TEXT_OFFSET = 0x1000
DATA_OFFSET = 0x2000
RELA_OFFSET = 0x220
DYNAMIC_OFFSET = 0x2020
SYMBOL_OFFSET = 0x2800
STRING_OFFSET = 0x2900
SECTION_NAME_OFFSET = 0x2A00
SECTION_OFFSET = 0x3000
FILE_SIZE = 0x4000


@dataclasses.dataclass
class Fixture:
    image: bytearray
    programs: dict[str, int]
    sections: dict[str, int]
    strings: dict[str, int]


def string_table(names: tuple[str, ...]) -> tuple[bytes, dict[str, int]]:
    result = bytearray(b"\0")
    offsets: dict[str, int] = {"": 0}
    for name in names:
        offsets[name] = len(result)
        result.extend(name.encode("ascii"))
        result.append(0)
    return bytes(result), offsets


def build_fixture(*, dynamic: bool = True) -> Fixture:
    section_names, section_name_offsets = string_table(
        (
            ".text",
            ".rodata",
            ".data",
            ".dynamic",
            ".rela.dyn",
            ".bss",
            ".symtab",
            ".strtab",
            ".shstrtab",
            ".debug_info",
            ".init_array",
            ".tdata",
        )
    )
    strings, string_offsets = string_table(
        (
            "burrow_aarch64_entry",
            "__cxa_atexit",
            "replacement_entry",
        )
    )

    if dynamic:
        section_order = (
            "",
            ".text",
            ".rodata",
            ".data",
            ".dynamic",
            ".rela.dyn",
            ".bss",
            ".symtab",
            ".strtab",
            ".shstrtab",
        )
        program_names = ("headers", "text", "data", "dynamic")
        read_only_file_size = RELA_OFFSET + 24
        dynamic_entries = (
            (DYNAMIC_TAG_RELA, RELA_OFFSET),
            (DYNAMIC_TAG_RELASZ, 24),
            (DYNAMIC_TAG_RELAENT, 24),
            (DYNAMIC_TAG_NULL, 0),
        )
        dynamic_size = len(dynamic_entries) * 16
        bss_address = DYNAMIC_OFFSET + dynamic_size
        data_file_size = bss_address - DATA_OFFSET
    else:
        section_order = (
            "",
            ".text",
            ".rodata",
            ".data",
            ".bss",
            ".symtab",
            ".strtab",
            ".shstrtab",
        )
        program_names = ("headers", "text", "data")
        read_only_file_size = 0x210
        dynamic_entries = ()
        dynamic_size = 0
        bss_address = DATA_OFFSET + 0x10
        data_file_size = 0x10

    section_indices = {
        name: index for index, name in enumerate(section_order)
    }
    program_offsets = {
        name: PROGRAM_OFFSET + index * PROGRAM_HEADER.size
        for index, name in enumerate(program_names)
    }
    section_offsets = {
        name: SECTION_OFFSET + index * SECTION_HEADER.size
        for index, name in enumerate(section_order)
    }

    image = bytearray(FILE_SIZE)
    identification = b"\x7fELF\x02\x01\x01\x00" + bytes(8)
    ELF_HEADER.pack_into(
        image,
        0,
        identification,
        3,
        183,
        1,
        TEXT_OFFSET,
        PROGRAM_OFFSET,
        SECTION_OFFSET,
        0,
        ELF_HEADER.size,
        PROGRAM_HEADER.size,
        len(program_names),
        SECTION_HEADER.size,
        len(section_order),
        section_indices[".shstrtab"],
    )

    PROGRAM_HEADER.pack_into(
        image,
        program_offsets["headers"],
        1,
        PROGRAM_FLAG_READ,
        0,
        0,
        0,
        read_only_file_size,
        read_only_file_size,
        0x1000,
    )
    PROGRAM_HEADER.pack_into(
        image,
        program_offsets["text"],
        1,
        PROGRAM_FLAG_READ | PROGRAM_FLAG_EXECUTE,
        TEXT_OFFSET,
        TEXT_OFFSET,
        TEXT_OFFSET,
        0x10,
        0x10,
        0x1000,
    )
    PROGRAM_HEADER.pack_into(
        image,
        program_offsets["data"],
        1,
        PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE,
        DATA_OFFSET,
        DATA_OFFSET,
        DATA_OFFSET,
        data_file_size,
        bss_address + 0x10 - DATA_OFFSET,
        0x1000,
    )
    if dynamic:
        PROGRAM_HEADER.pack_into(
            image,
            program_offsets["dynamic"],
            PROGRAM_TYPE_DYNAMIC,
            PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE,
            DYNAMIC_OFFSET,
            DYNAMIC_OFFSET,
            DYNAMIC_OFFSET,
            dynamic_size,
            dynamic_size,
            8,
        )

    image[0x200:0x210] = b"BURROW-READ-ONLY"
    image[TEXT_OFFSET : TEXT_OFFSET + 0x10] = bytes.fromhex(
        "00000014000000140000001400000014"
    )
    image[DATA_OFFSET : DATA_OFFSET + 0x10] = b"BURROW-WRITABLE"

    if dynamic:
        for index, (tag, value) in enumerate(dynamic_entries):
            struct.pack_into("<qQ", image, DYNAMIC_OFFSET + index * 16, tag, value)
        struct.pack_into(
            "<QQq",
            image,
            RELA_OFFSET,
            DATA_OFFSET + 8,
            RELOCATION_AARCH64_RELATIVE,
            TEXT_OFFSET,
        )

    image[SYMBOL_OFFSET : SYMBOL_OFFSET + SYMBOL.size] = bytes(SYMBOL.size)
    SYMBOL.pack_into(
        image,
        SYMBOL_OFFSET + SYMBOL.size,
        string_offsets["burrow_aarch64_entry"],
        0x12,
        0,
        section_indices[".text"],
        TEXT_OFFSET,
        4,
    )
    image[STRING_OFFSET : STRING_OFFSET + len(strings)] = strings
    image[SECTION_NAME_OFFSET : SECTION_NAME_OFFSET + len(section_names)] = section_names

    sections: dict[str, tuple[int, int, int, int, int, int, int, int, int, int]] = {
        "": (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        ".text": (
            section_name_offsets[".text"],
            SECTION_TYPE_PROGBITS,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_EXECUTE,
            TEXT_OFFSET,
            TEXT_OFFSET,
            0x10,
            0,
            0,
            4,
            0,
        ),
        ".rodata": (
            section_name_offsets[".rodata"],
            SECTION_TYPE_PROGBITS,
            SECTION_FLAG_ALLOCATE,
            0x200,
            0x200,
            0x10,
            0,
            0,
            8,
            0,
        ),
        ".data": (
            section_name_offsets[".data"],
            SECTION_TYPE_PROGBITS,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_WRITE,
            DATA_OFFSET,
            DATA_OFFSET,
            0x10,
            0,
            0,
            8,
            0,
        ),
        ".bss": (
            section_name_offsets[".bss"],
            SECTION_TYPE_NOBITS,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_WRITE,
            bss_address,
            bss_address,
            0x10,
            0,
            0,
            8,
            0,
        ),
        ".symtab": (
            section_name_offsets[".symtab"],
            SECTION_TYPE_SYMBOL_TABLE,
            0,
            0,
            SYMBOL_OFFSET,
            SYMBOL.size * 2,
            section_indices[".strtab"],
            1,
            8,
            SYMBOL.size,
        ),
        ".strtab": (
            section_name_offsets[".strtab"],
            SECTION_TYPE_STRING_TABLE,
            0,
            0,
            STRING_OFFSET,
            len(strings),
            0,
            0,
            1,
            0,
        ),
        ".shstrtab": (
            section_name_offsets[".shstrtab"],
            SECTION_TYPE_STRING_TABLE,
            0,
            0,
            SECTION_NAME_OFFSET,
            len(section_names),
            0,
            0,
            1,
            0,
        ),
    }
    if dynamic:
        sections[".dynamic"] = (
            section_name_offsets[".dynamic"],
            SECTION_TYPE_DYNAMIC,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_WRITE,
            DYNAMIC_OFFSET,
            DYNAMIC_OFFSET,
            dynamic_size,
            0,
            0,
            8,
            16,
        )
        sections[".rela.dyn"] = (
            section_name_offsets[".rela.dyn"],
            SECTION_TYPE_RELA,
            SECTION_FLAG_ALLOCATE,
            RELA_OFFSET,
            RELA_OFFSET,
            24,
            section_indices[".symtab"],
            section_indices[".data"],
            8,
            24,
        )

    for section_name in section_order:
        SECTION_HEADER.pack_into(
            image, section_offsets[section_name], *sections[section_name]
        )

    return Fixture(
        image=image,
        programs=program_offsets,
        sections=section_offsets,
        strings={**section_name_offsets, **string_offsets},
    )


class BurrowImageFixtureTests(unittest.TestCase):
    def assert_rejected(self, fixture: Fixture, message: str) -> None:
        with self.assertRaisesRegex(VerificationError, message):
            verify_image(bytes(fixture.image))

    def test_accepts_audited_relative_relocation_image(self) -> None:
        summary = verify_image(bytes(build_fixture().image))
        self.assertEqual(summary.entry, TEXT_OFFSET)
        self.assertEqual(summary.load_segment_count, 3)
        self.assertEqual(summary.relocation_count, 1)

    def test_accepts_image_without_dynamic_metadata_or_relocations(self) -> None:
        summary = verify_image(bytes(build_fixture(dynamic=False).image))
        self.assertEqual(summary.relocation_count, 0)

    def test_accepts_complete_isolated_qemu_result_transport(self) -> None:
        fixture = build_fixture()
        fixture.image[TEXT_OFFSET : TEXT_OFFSET + len(QEMU_SEMIHOST_HLT)] = (
            QEMU_SEMIHOST_HLT
        )
        fixture.image[0x140 : 0x140 + len(QEMU_PASS_MARKER)] = QEMU_PASS_MARKER
        fixture.image[0x180 : 0x180 + len(QEMU_EXIT_ARGUMENTS)] = (
            QEMU_EXIT_ARGUMENTS
        )
        verify_image(bytes(fixture.image), qemu_test_result=True)

    def test_non_test_image_rejects_each_qemu_result_component(self) -> None:
        for payload, message in (
            (QEMU_SEMIHOST_HLT, "QEMU semihost HLT must be absent"),
            (QEMU_PASS_MARKER, "QEMU pass marker must be absent"),
            (QEMU_EXIT_ARGUMENTS, "QEMU exit argument block must be absent"),
        ):
            with self.subTest(message=message):
                fixture = build_fixture()
                fixture.image[0x140 : 0x140 + len(payload)] = payload
                with self.assertRaisesRegex(VerificationError, message):
                    verify_image(bytes(fixture.image))

    def test_test_image_rejects_incomplete_qemu_result_transport(self) -> None:
        fixture = build_fixture()
        fixture.image[TEXT_OFFSET : TEXT_OFFSET + len(QEMU_SEMIHOST_HLT)] = (
            QEMU_SEMIHOST_HLT
        )
        with self.assertRaisesRegex(VerificationError, "QEMU pass marker must be exactly once"):
            verify_image(bytes(fixture.image), qemu_test_result=True)

    def test_accepts_hidden_entry_localized_by_the_linker(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<B", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 4, 0x02)
        struct.pack_into("<B", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 5, 2)
        verify_image(bytes(fixture.image))

    def test_accepts_runtime_copy_with_non_loaded_differences(self) -> None:
        symbol_fixture = build_fixture()
        runtime_fixture = build_fixture()
        symbol_fixture.image[-1] = 0xA5
        verify_runtime_copy(
            bytes(symbol_fixture.image), bytes(runtime_fixture.image)
        )

    def test_rejects_runtime_copy_that_changes_loaded_bytes(self) -> None:
        symbol_fixture = build_fixture()
        runtime_fixture = build_fixture()
        runtime_fixture.image[0x200] ^= 0xFF
        with self.assertRaisesRegex(VerificationError, "changes loaded bytes"):
            verify_runtime_copy(
                bytes(symbol_fixture.image), bytes(runtime_fixture.image)
            )

    def test_rejects_truncated_header(self) -> None:
        fixture = build_fixture()
        fixture.image = fixture.image[: ELF_HEADER.size - 1]
        self.assert_rejected(fixture, "ELF header is truncated")

    def test_rejects_invalid_elf_identity(self) -> None:
        mutations = (
            (0, b"BAD!", "invalid ELF magic"),
            (4, b"\x01", "not ELF64"),
            (5, b"\x02", "not little-endian"),
            (6, b"\x02", "identification version"),
            (7, b"\x03", "System V ELF ABI"),
            (9, b"\x01", "padding is not zero"),
        )
        for offset, replacement, message in mutations:
            with self.subTest(message=message):
                fixture = build_fixture()
                fixture.image[offset : offset + len(replacement)] = replacement
                self.assert_rejected(fixture, message)

    def test_rejects_wrong_object_type_and_machine(self) -> None:
        mutations = (
            (16, "<H", 2, "not ET_DYN"),
            (18, "<H", 62, "not AArch64"),
            (20, "<I", 2, "header version"),
        )
        for offset, encoding, value, message in mutations:
            with self.subTest(message=message):
                fixture = build_fixture()
                struct.pack_into(encoding, fixture.image, offset, value)
                self.assert_rejected(fixture, message)

    def test_rejects_overflowing_program_header_table(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, 32, 0xFFFFFFFFFFFFFFF0)
        self.assert_rejected(fixture, "program-header table range overflows")

    def test_rejects_truncated_section_header_table(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, 40, FILE_SIZE - 16)
        self.assert_rejected(fixture, "section-header table is truncated")

    def test_rejects_entry_outside_executable_storage(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, 24, DATA_OFFSET)
        self.assert_rejected(fixture, "entry is not in file-backed executable")

    def test_rejects_nonzero_image_base(self) -> None:
        fixture = build_fixture()
        program = fixture.programs["headers"]
        struct.pack_into("<Q", fixture.image, program + 16, 0x3000)
        struct.pack_into("<Q", fixture.image, program + 24, 0x3000)
        self.assert_rejected(fixture, "first PT_LOAD does not begin")

    def test_rejects_invalid_load_size_and_alignment(self) -> None:
        fixture = build_fixture()
        program = fixture.programs["data"]
        struct.pack_into("<Q", fixture.image, program + 40, 1)
        self.assert_rejected(fixture, "filesz greater than memsz")

        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, fixture.programs["text"] + 48, 16)
        self.assert_rejected(fixture, "not 4 KiB aligned")

    def test_rejects_truncated_load_file_range(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, fixture.programs["data"] + 8, FILE_SIZE)
        self.assert_rejected(fixture, "program header 2 file range is truncated")

    def test_rejects_overlapping_load_pages(self) -> None:
        fixture = build_fixture()
        program = fixture.programs["data"]
        struct.pack_into("<Q", fixture.image, program + 16, TEXT_OFFSET)
        struct.pack_into("<Q", fixture.image, program + 24, TEXT_OFFSET)
        self.assert_rejected(fixture, "PT_LOAD memory pages overlap")

    def test_rejects_writable_executable_load(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            fixture.programs["data"] + 4,
            PROGRAM_FLAG_READ | PROGRAM_FLAG_WRITE | PROGRAM_FLAG_EXECUTE,
        )
        self.assert_rejected(fixture, "permission classes are not R, RX, and RW")

    def test_rejects_load_outside_two_gibibyte_window(self) -> None:
        fixture = build_fixture()
        program = fixture.programs["data"]
        struct.pack_into("<Q", fixture.image, program + 16, 0x7FFFF000)
        struct.pack_into("<Q", fixture.image, program + 24, 0x7FFFF000)
        struct.pack_into("<Q", fixture.image, program + 40, 0x2000)
        self.assert_rejected(fixture, "PT_LOAD 2 memory range")

    def test_rejects_section_name_outside_string_table(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<I", fixture.image, fixture.sections[".text"], 0xFFFFFFFF)
        self.assert_rejected(fixture, "section 1 name is outside")

    def test_rejects_allocated_section_outside_load(self) -> None:
        fixture = build_fixture()
        section = fixture.sections[".rodata"]
        struct.pack_into("<Q", fixture.image, section + 16, 0x300)
        struct.pack_into("<Q", fixture.image, section + 24, 0x300)
        self.assert_rejected(fixture, "allocated section .rodata is outside")

    def test_rejects_section_permission_mismatch(self) -> None:
        fixture = build_fixture()
        section = fixture.sections[".rodata"]
        struct.pack_into(
            "<Q",
            fixture.image,
            section + 8,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_EXECUTE,
        )
        self.assert_rejected(fixture, "permission class does not match")

    def test_rejects_missing_zero_filled_storage(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, fixture.sections[".bss"] + 32, 0)
        self.assert_rejected(fixture, "no writable zero-filled")

    def test_rejects_zero_filled_section_overlapping_file_data(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<Q", fixture.image, fixture.sections[".bss"] + 16, DATA_OFFSET + 8
        )
        self.assert_rejected(fixture, "zero-filled section .bss overlaps file-backed")

    def test_runtime_mode_rejects_non_runtime_metadata(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            fixture.sections[".rodata"],
            fixture.strings[".debug_info"],
        )
        verify_image(bytes(fixture.image))
        with self.assertRaisesRegex(VerificationError, "non-runtime metadata"):
            verify_image(bytes(fixture.image), runtime=True)

    def test_rejects_forbidden_section_and_tls(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            fixture.sections[".rodata"],
            fixture.strings[".init_array"],
        )
        self.assert_rejected(fixture, "forbidden section .init_array")

        fixture = build_fixture()
        section = fixture.sections[".data"]
        struct.pack_into(
            "<Q",
            fixture.image,
            section + 8,
            SECTION_FLAG_ALLOCATE | SECTION_FLAG_WRITE | SECTION_FLAG_TLS,
        )
        self.assert_rejected(fixture, "uses TLS")

    def test_rejects_undefined_entry_symbol(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<H", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 6, 0)
        self.assert_rejected(fixture, "undefined symbol burrow_aarch64_entry")

    def test_rejects_missing_or_forbidden_entry_symbol(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            SYMBOL_OFFSET + SYMBOL.size,
            fixture.strings["replacement_entry"],
        )
        self.assert_rejected(fixture, "must define burrow_aarch64_entry")

        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            SYMBOL_OFFSET + SYMBOL.size,
            fixture.strings["__cxa_atexit"],
        )
        self.assert_rejected(fixture, "forbidden runtime symbol __cxa_atexit")

    def test_rejects_invalid_entry_symbol_contract(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<B", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 4, 0x11)
        self.assert_rejected(fixture, "is not a retained C-compatible function")

        fixture = build_fixture()
        struct.pack_into(
            "<Q", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 8, TEXT_OFFSET + 4
        )
        self.assert_rejected(fixture, "does not match the ELF entry")

        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, SYMBOL_OFFSET + SYMBOL.size + 16, 0)
        self.assert_rejected(fixture, "burrow_aarch64_entry has zero size")

    def test_rejects_interpreter_program_header(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<I",
            fixture.image,
            fixture.programs["dynamic"],
            PROGRAM_TYPE_INTERPRETER,
        )
        self.assert_rejected(fixture, "forbidden PT_INTERP")

    def test_rejects_forbidden_dynamic_metadata(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<q", fixture.image, DYNAMIC_OFFSET, DYNAMIC_TAG_NEEDED)
        self.assert_rejected(fixture, "forbidden DT_NEEDED")

    def test_rejects_dynamic_table_without_null_terminator(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<q", fixture.image, DYNAMIC_OFFSET + 48, DYNAMIC_TAG_FLAGS)
        self.assert_rejected(fixture, "PT_DYNAMIC has no DT_NULL")

    def test_rejects_incomplete_dynamic_relocation_metadata(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<q", fixture.image, DYNAMIC_OFFSET + 32, DYNAMIC_TAG_FLAGS)
        self.assert_rejected(fixture, "dynamic RELA metadata is incomplete")

    def test_rejects_disagreeing_dynamic_section_metadata(self) -> None:
        fixture = build_fixture()
        struct.pack_into(
            "<Q", fixture.image, fixture.sections[".dynamic"] + 32, 16
        )
        self.assert_rejected(fixture, "SHT_DYNAMIC does not match PT_DYNAMIC")

    def test_rejects_allocated_dynamic_section_without_segment(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<I", fixture.image, fixture.programs["dynamic"], 0)
        self.assert_rejected(fixture, "allocated SHT_DYNAMIC has no PT_DYNAMIC")

    def test_rejects_allocated_relocations_without_dynamic_metadata(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<I", fixture.image, fixture.programs["dynamic"], 0)
        struct.pack_into(
            "<I",
            fixture.image,
            fixture.sections[".dynamic"] + 4,
            SECTION_TYPE_PROGBITS,
        )
        self.assert_rejected(fixture, "allocated RELA section has no PT_DYNAMIC")

    def test_rejects_non_relative_relocation(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, RELA_OFFSET + 8, 1026)
        self.assert_rejected(fixture, "is not R_AARCH64_RELATIVE")

    def test_rejects_symbol_bearing_relative_relocation(self) -> None:
        fixture = build_fixture()
        information = (1 << 32) | RELOCATION_AARCH64_RELATIVE
        struct.pack_into("<Q", fixture.image, RELA_OFFSET + 8, information)
        self.assert_rejected(fixture, "references a symbol")

    def test_rejects_relocation_target_outside_writable_storage(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<Q", fixture.image, RELA_OFFSET, TEXT_OFFSET)
        self.assert_rejected(fixture, "target is outside writable")

    def test_rejects_relocation_addend_outside_loaded_storage(self) -> None:
        fixture = build_fixture()
        struct.pack_into("<q", fixture.image, RELA_OFFSET + 16, 0x3000)
        self.assert_rejected(fixture, "addend does not name loaded")


if __name__ == "__main__":
    unittest.main()
