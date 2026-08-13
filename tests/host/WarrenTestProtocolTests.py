#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from warren_test_protocol import (  # noqa: E402
    HostClassification,
    ProtocolError,
    RecordKind,
    SerialProtocolParser,
    classify_process_result,
    parse_serial_output,
)


def stream(begin: bytes, terminal: bytes) -> bytes:
    return begin + b"diagnostic text\r\n" + terminal


class SerialProtocolParserTests(unittest.TestCase):
    def test_pass_lf(self) -> None:
        transcript = parse_serial_output(
            stream(
                b"WARREN_TEST:1:BEGIN:first-light\n",
                b"WARREN_TEST:1:PASS:first-light\n",
            )
        )
        self.assertEqual(transcript.begin_identifier, "first-light")
        self.assertIsNotNone(transcript.terminal_record)
        self.assertEqual(transcript.terminal_record.kind, RecordKind.PASS)
        self.assertEqual(transcript.terminal_record.result_code, 0)

    def test_crlf_and_chunk_boundaries(self) -> None:
        parser = SerialProtocolParser()
        for chunk in (
            b"WARREN_",
            b"TEST:1:BEGIN:chunked\r",
            b"\nlog\nWARREN_TEST:1:",
            b"PASS:chunked\r\n",
        ):
            parser.feed(chunk)
        transcript = parser.finish()
        self.assertEqual(transcript.terminal_record.test_identifier, "chunked")

    def test_valid_fail_codes(self) -> None:
        for code in (1, 5, 64, 95):
            with self.subTest(code=code):
                transcript = parse_serial_output(
                    stream(
                        b"WARREN_TEST:1:BEGIN:failure\n",
                        f"WARREN_TEST:1:FAIL:failure:{code}\n".encode(),
                    )
                )
                self.assertEqual(transcript.terminal_record.result_code, code)

    def test_valid_panic_codes(self) -> None:
        for code in (2, 3, 4):
            with self.subTest(code=code):
                transcript = parse_serial_output(
                    stream(
                        b"WARREN_TEST:1:BEGIN:panic\n",
                        f"WARREN_TEST:1:PANIC:panic:{code}\n".encode(),
                    )
                )
                self.assertEqual(transcript.terminal_record.result_code, code)

    def test_panic_may_be_the_only_record(self) -> None:
        transcript = parse_serial_output(b"WARREN_TEST:1:PANIC:early-entry:4\n")
        self.assertIsNone(transcript.begin_identifier)
        self.assertEqual(transcript.terminal_record.test_identifier, "early-entry")

    def test_diagnostic_lines_are_ignored(self) -> None:
        transcript = parse_serial_output(
            b"Warren output\nnot WARREN_TEST:1:noise\n"
            b"WARREN_TEST:1:BEGIN:diagnostics\n"
            b"more output\n"
            b"WARREN_TEST:1:PASS:diagnostics\n"
            b"QEMU diagnostic after the terminal\n"
        )
        self.assertEqual(transcript.terminal_record.result_code, 0)

    def test_malformed_records(self) -> None:
        malformed_records = (
            b"WARREN_TEST:2:BEGIN:test\n",
            b"WARREN_TEST:1:UNKNOWN:test\n",
            b"WARREN_TEST:1:BEGIN:\n",
            b"WARREN_TEST:1:BEGIN:UPPER\n",
            b"WARREN_TEST:1:BEGIN:bad:id\n",
            b"WARREN_TEST:1:BEGIN:" + b"a" * 65 + b"\n",
            b"WARREN_TEST:1:FAIL:test:01\n",
            b"WARREN_TEST:1:FAIL:test:+1\n",
            b"WARREN_TEST:1:PASS:test:0\n",
            b"WARREN_TEST:1:BEGIN:t\xffst\n",
        )
        for record in malformed_records:
            with self.subTest(record=record):
                with self.assertRaises(ProtocolError):
                    parse_serial_output(record)

    def test_invalid_result_allocations(self) -> None:
        invalid_records = (
            b"WARREN_TEST:1:FAIL:test:2\n",
            b"WARREN_TEST:1:FAIL:test:6\n",
            b"WARREN_TEST:1:FAIL:test:96\n",
            b"WARREN_TEST:1:FAIL:test:120\n",
            b"WARREN_TEST:1:PANIC:test:1\n",
            b"WARREN_TEST:1:PANIC:test:5\n",
        )
        for terminal in invalid_records:
            with self.subTest(terminal=terminal):
                with self.assertRaises(ProtocolError):
                    parse_serial_output(
                        b"WARREN_TEST:1:BEGIN:test\n" + terminal
                    )

    def test_record_ordering(self) -> None:
        invalid_streams = (
            b"WARREN_TEST:1:PASS:test\n",
            b"WARREN_TEST:1:FAIL:test:1\n",
            b"WARREN_TEST:1:BEGIN:test\nWARREN_TEST:1:BEGIN:test\n",
            b"WARREN_TEST:1:BEGIN:a\nWARREN_TEST:1:PASS:b\n",
            b"WARREN_TEST:1:BEGIN:a\nWARREN_TEST:1:PANIC:b:3\n",
            b"WARREN_TEST:1:BEGIN:a\nWARREN_TEST:1:PASS:a\n"
            b"WARREN_TEST:1:PASS:a\n",
            b"WARREN_TEST:1:PANIC:a:3\nWARREN_TEST:1:BEGIN:a\n",
        )
        for serial_output in invalid_streams:
            with self.subTest(serial_output=serial_output):
                with self.assertRaises(ProtocolError):
                    parse_serial_output(serial_output)

    def test_unterminated_and_overlong_records(self) -> None:
        with self.assertRaises(ProtocolError):
            parse_serial_output(b"WARREN_TEST:1:BEGIN:unfinished")

        parser = SerialProtocolParser()
        with self.assertRaises(ProtocolError):
            parser.feed(b"WARREN_TEST:" + b"x" * 84)


class HostClassifierTests(unittest.TestCase):
    def test_agreed_results(self) -> None:
        cases = (
            (
                b"WARREN_TEST:1:BEGIN:pass\nWARREN_TEST:1:PASS:pass\n",
                0,
                HostClassification.PASS,
                0,
            ),
            (
                b"WARREN_TEST:1:BEGIN:fail\nWARREN_TEST:1:FAIL:fail:64\n",
                64,
                HostClassification.EXPLICIT_FAILURE,
                1,
            ),
            (
                b"WARREN_TEST:1:BEGIN:panic\nWARREN_TEST:1:PANIC:panic:2\n",
                2,
                HostClassification.PANIC_OR_ASSERTION,
                2,
            ),
        )
        for serial_output, returncode, classification, harness_status in cases:
            with self.subTest(classification=classification):
                result = classify_process_result(serial_output, returncode)
                self.assertEqual(result.classification, classification)
                self.assertEqual(result.harness_status, harness_status)

    def test_channel_disagreement_is_protocol_error(self) -> None:
        cases = (
            (
                b"WARREN_TEST:1:BEGIN:pass\nWARREN_TEST:1:PASS:pass\n",
                1,
            ),
            (
                b"WARREN_TEST:1:BEGIN:fail\nWARREN_TEST:1:FAIL:fail:1\n",
                0,
            ),
            (b"WARREN_TEST:1:PANIC:panic:3\n", -6),
        )
        for serial_output, returncode in cases:
            with self.subTest(returncode=returncode):
                result = classify_process_result(serial_output, returncode)
                self.assertEqual(
                    result.classification,
                    HostClassification.PROTOCOL_ERROR,
                )
                self.assertEqual(result.harness_status, 3)

    def test_missing_terminal_classification(self) -> None:
        clean = classify_process_result(b"ordinary output\n", 0)
        self.assertEqual(clean.classification, HostClassification.PROTOCOL_ERROR)

        failed = classify_process_result(b"ordinary output\n", 17)
        self.assertEqual(
            failed.classification,
            HostClassification.QEMU_PROCESS_FAILURE,
        )
        self.assertEqual(failed.harness_status, 4)

    def test_malformed_output_is_protocol_error(self) -> None:
        result = classify_process_result(b"WARREN_TEST:garbage\n", 0)
        self.assertEqual(result.classification, HostClassification.PROTOCOL_ERROR)

    def test_timeout_wins_over_pass_marker(self) -> None:
        result = classify_process_result(
            b"WARREN_TEST:1:BEGIN:hang\nWARREN_TEST:1:PASS:hang\n",
            None,
            timed_out=True,
        )
        self.assertEqual(result.classification, HostClassification.TIMEOUT)
        self.assertEqual(result.harness_status, 124)

    def test_launch_error_has_highest_precedence(self) -> None:
        result = classify_process_result(
            b"WARREN_TEST:broken\n",
            None,
            timed_out=True,
            launch_error=True,
        )
        self.assertEqual(result.classification, HostClassification.LAUNCH_ERROR)
        self.assertEqual(result.harness_status, 125)


if __name__ == "__main__":
    unittest.main()
