#
# Created by Zack Shrout on 8/20/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from run_uefi_smoke import machine_configuration, required_el_evidence  # noqa: E402


class InitialExceptionLevelProfileTests(unittest.TestCase):
    def test_el1_profile_disables_virtualization_explicitly(self) -> None:
        self.assertEqual(
            machine_configuration("el1"),
            "virt-11.0,gic-version=3,virtualization=off",
        )

    def test_el2_profile_enables_virtualization_explicitly(self) -> None:
        self.assertEqual(
            machine_configuration("el2"),
            "virt-11.0,gic-version=3,virtualization=on",
        )

    def test_burrow_evidence_includes_loader_and_entry(self) -> None:
        self.assertEqual(
            required_el_evidence("el2", "burrow"),
            (
                b"WARREN_POST_EXIT:ExitBootServices:EL2",
                b"BURROW_FIRST_ENTRY:EL2:",
            ),
        )

    def test_loader_evidence_stops_before_burrow(self) -> None:
        self.assertEqual(
            required_el_evidence("el1", "loader"),
            (b"WARREN_POST_EXIT:ExitBootServices:EL1",),
        )

    def test_no_evidence_is_available_before_post_exit_transfer(self) -> None:
        self.assertEqual(required_el_evidence("el1", "none"), ())


if __name__ == "__main__":
    unittest.main()
