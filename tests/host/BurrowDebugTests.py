#
# Created by Zack Shrout on 10/4/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from burrow_debug import (  # noqa: E402
    DebugArtifactError,
    DEBUG_PHYSICAL_LOAD_BIAS,
    ELF_HEADER,
    STABLE_IMAGE_BASE,
    debug_symbol_probes,
    lldb_slide,
    normalize_runtime_address,
    parse_linker_map,
    render_lldb_script,
    symbolize_addresses,
    validate_image_slide,
    validate_port,
    verify_debug_artifacts,
)
from run_aarch64_debug import debug_qemu_command  # noqa: E402


ENTRY = 0x1000
IMAGE_END = 0xD580
VALID_MAP = """             VMA              LMA     Size Align Out     In      Symbol
               0                0        0     1 HIDDEN(burrow_image_start = 0)
            1000             1000      34c     1                 burrow_aarch64_entry
            AD00             AD00       E8     1                 burrow_qemu_virt_panic
            D580             D580        0     1 HIDDEN(burrow_image_end = .)
"""


def write_elf(path: pathlib.Path, entry: int = ENTRY) -> None:
    identification = bytearray(16)
    identification[:4] = b"\x7fELF"
    identification[4] = 2
    identification[5] = 1
    identification[6] = 1
    path.write_bytes(
        ELF_HEADER.pack(
            bytes(identification),
            3,
            183,
            1,
            entry,
            64,
            0,
            0,
            64,
            56,
            0,
            64,
            0,
            0,
        )
    )


class LinkerMapTests(unittest.TestCase):
    def test_valid_map_records_entry_extent_and_symbols(self) -> None:
        summary = parse_linker_map(VALID_MAP)
        self.assertEqual(summary.entry, ENTRY)
        self.assertEqual(summary.image_end, IMAGE_END)
        self.assertEqual(summary.symbols["burrow_qemu_virt_panic"], 0xAD00)

    def test_map_requires_exact_header(self) -> None:
        with self.assertRaisesRegex(DebugArtifactError, "header"):
            parse_linker_map("VMA LMA Size\n")

    def test_map_requires_one_entry_and_end(self) -> None:
        with self.assertRaisesRegex(DebugArtifactError, "entry"):
            parse_linker_map(VALID_MAP.replace("burrow_aarch64_entry", "missing"))
        with self.assertRaisesRegex(DebugArtifactError, "image_end"):
            parse_linker_map(VALID_MAP.replace("burrow_image_end", "missing"))

    def test_map_must_match_elf_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            image = root / "burrow.elf"
            linker_map = root / "burrow.map"
            write_elf(image, ENTRY + 4)
            linker_map.write_text(VALID_MAP, encoding="utf-8")
            with self.assertRaisesRegex(DebugArtifactError, "does not match"):
                verify_debug_artifacts(image, linker_map)


class AddressNormalizationTests(unittest.TestCase):
    def test_all_aliases_reach_the_same_relative_address(self) -> None:
        relative = 0xAD00
        bias = 0x5C73F000
        self.assertEqual(
            normalize_runtime_address(relative, "elf", IMAGE_END), relative
        )
        self.assertEqual(
            normalize_runtime_address(
                STABLE_IMAGE_BASE + relative, "stable", IMAGE_END
            ),
            relative,
        )
        self.assertEqual(
            normalize_runtime_address(
                bias + relative, "physical", IMAGE_END, bias
            ),
            relative,
        )

    def test_alias_underflow_and_extent_are_rejected(self) -> None:
        with self.assertRaisesRegex(DebugArtifactError, "below"):
            normalize_runtime_address(STABLE_IMAGE_BASE - 1, "stable", IMAGE_END)
        with self.assertRaisesRegex(DebugArtifactError, "load-bias"):
            normalize_runtime_address(0x1000, "physical", IMAGE_END)
        with self.assertRaisesRegex(DebugArtifactError, "outside"):
            normalize_runtime_address(IMAGE_END, "elf", IMAGE_END)


class SymbolizerTests(unittest.TestCase):
    def test_debug_probes_cover_assembly_cpp_and_runtime_aliases(self) -> None:
        summary = parse_linker_map(VALID_MAP)
        self.assertEqual(
            debug_symbol_probes(summary),
            [
                (ENTRY, ENTRY),
                (STABLE_IMAGE_BASE + ENTRY, ENTRY),
                (0xAD00, 0xAD00),
                (DEBUG_PHYSICAL_LOAD_BIAS + 0xAD00, 0xAD00),
            ],
        )

        missing = parse_linker_map(
            VALID_MAP.replace("burrow_qemu_virt_panic", "another_symbol")
        )
        with self.assertRaisesRegex(DebugArtifactError, "C\\+\\+ debug probe"):
            debug_symbol_probes(missing)

    def test_batch_output_is_normalized_to_repository_paths(self) -> None:
        def runner(*_args: object, **_kwargs: object) -> subprocess.CompletedProcess[str]:
            return subprocess.CompletedProcess(
                [],
                0,
                "burrow_aarch64_entry\n./kernel/src/Arch/AArch64/Entry.S:65:0\n\n"
                "burrow_qemu_virt_panic\n"
                "/repo/kernel/src/Platform/QemuVirt/Panic.cpp:53:0\n",
            )

        records = symbolize_addresses(
            pathlib.Path("llvm-symbolizer"),
            pathlib.Path("burrow.elf"),
            pathlib.Path("/repo"),
            ((ENTRY, ENTRY), (STABLE_IMAGE_BASE + 0xAD00, 0xAD00)),
            runner=runner,
        )
        self.assertEqual(records[0].source, "kernel/src/Arch/AArch64/Entry.S")
        self.assertEqual(
            records[1].source, "kernel/src/Platform/QemuVirt/Panic.cpp"
        )

    def test_unresolved_or_wrong_count_output_is_rejected(self) -> None:
        def unresolved(*_args: object, **_kwargs: object) -> subprocess.CompletedProcess[str]:
            return subprocess.CompletedProcess([], 0, "??\n??:0:0\n")

        with self.assertRaisesRegex(DebugArtifactError, "did not resolve"):
            symbolize_addresses(
                pathlib.Path("llvm-symbolizer"),
                pathlib.Path("burrow.elf"),
                pathlib.Path("/repo"),
                ((ENTRY, ENTRY),),
                runner=unresolved,
            )

    def test_missing_symbolizer_is_rejected(self) -> None:
        def missing(*_args: object, **_kwargs: object) -> subprocess.CompletedProcess[str]:
            raise FileNotFoundError("missing")

        with self.assertRaisesRegex(DebugArtifactError, "could not run"):
            symbolize_addresses(
                pathlib.Path("llvm-symbolizer"),
                pathlib.Path("burrow.elf"),
                pathlib.Path("/repo"),
                ((ENTRY, ENTRY),),
                runner=missing,
            )


class LldbAndQemuTests(unittest.TestCase):
    def test_stable_lldb_script_uses_loopback_and_exact_slide(self) -> None:
        script = render_lldb_script(
            pathlib.Path("/build/burrow.elf"),
            pathlib.Path("/repo"),
            lldb_slide("stable", None),
            1234,
        )
        self.assertIn("target.default-arch aarch64", script)
        self.assertIn("gdb-remote 127.0.0.1:1234", script)
        self.assertIn("--slide 0xFFFFFFFF80000000", script)

    def test_physical_lldb_slide_requires_explicit_bias(self) -> None:
        with self.assertRaisesRegex(DebugArtifactError, "load-bias"):
            lldb_slide("physical", None)
        self.assertEqual(lldb_slide("physical", 0x40000000), 0x40000000)
        with self.assertRaisesRegex(DebugArtifactError, "overflows"):
            validate_image_slide(0xFFFFFFFFFFFFF000, 0x2000)

    def test_ports_are_bounded(self) -> None:
        self.assertEqual(validate_port(1234), 1234)
        for port in (0, 65536):
            with self.assertRaisesRegex(DebugArtifactError, "between"):
                validate_port(port)

    def test_qemu_debug_command_is_paused_and_loopback_only(self) -> None:
        command = debug_qemu_command(
            pathlib.Path("qemu"),
            pathlib.Path("code.fd"),
            pathlib.Path("vars.fd"),
            pathlib.Path("esp.img"),
            "el2",
            4321,
        )
        self.assertIn("virt-11.0,gic-version=3,virtualization=on", command)
        self.assertIn("-S", command)
        self.assertEqual(command[command.index("-gdb") + 1], "tcp:127.0.0.1:4321")
        self.assertTrue(any("file=vars.fd" in argument for argument in command))


if __name__ == "__main__":
    unittest.main()
