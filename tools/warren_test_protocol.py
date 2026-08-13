#
# Created by Zack Shrout on 8/13/26.
# Copyright (c) 2026 BunnySoft. All rights reserved.
#

from __future__ import annotations

import dataclasses
import enum
import re


PROTOCOL_PREFIX = b"WARREN_TEST:"
MAXIMUM_RECORD_SIZE = 96

_SIMPLE_RECORD = re.compile(
    rb"WARREN_TEST:1:(BEGIN|PASS):([a-z0-9._-]{1,64})"
)
_CODED_RECORD = re.compile(
    rb"WARREN_TEST:1:(FAIL|PANIC):([a-z0-9._-]{1,64}):([1-9][0-9]{0,2})"
)


class ProtocolError(ValueError):
    """A Warren-prefixed serial record violated protocol version 1."""


class RecordKind(enum.Enum):
    BEGIN = "BEGIN"
    PASS = "PASS"
    FAIL = "FAIL"
    PANIC = "PANIC"


class HostClassification(enum.Enum):
    PASS = "pass"
    EXPLICIT_FAILURE = "explicit failure"
    PANIC_OR_ASSERTION = "panic/assertion"
    PROTOCOL_ERROR = "protocol error"
    QEMU_PROCESS_FAILURE = "QEMU process failure"
    TIMEOUT = "timeout/hang"
    LAUNCH_ERROR = "launch or host-harness error"


_HARNESS_STATUS = {
    HostClassification.PASS: 0,
    HostClassification.EXPLICIT_FAILURE: 1,
    HostClassification.PANIC_OR_ASSERTION: 2,
    HostClassification.PROTOCOL_ERROR: 3,
    HostClassification.QEMU_PROCESS_FAILURE: 4,
    HostClassification.TIMEOUT: 124,
    HostClassification.LAUNCH_ERROR: 125,
}


@dataclasses.dataclass(frozen=True)
class TestRecord:
    kind: RecordKind
    test_identifier: str
    result_code: int | None


@dataclasses.dataclass(frozen=True)
class Transcript:
    begin_identifier: str | None
    terminal_record: TestRecord | None


@dataclasses.dataclass(frozen=True)
class HostResult:
    classification: HostClassification
    harness_status: int
    test_identifier: str | None
    guest_result_code: int | None
    detail: str


def _make_result(
    classification: HostClassification,
    detail: str,
    terminal_record: TestRecord | None = None,
) -> HostResult:
    return HostResult(
        classification=classification,
        harness_status=_HARNESS_STATUS[classification],
        test_identifier=(
            terminal_record.test_identifier if terminal_record is not None else None
        ),
        guest_result_code=(
            terminal_record.result_code if terminal_record is not None else None
        ),
        detail=detail,
    )


class SerialProtocolParser:
    """Incrementally parse Warren protocol records from a byte stream."""

    def __init__(self) -> None:
        self._pending = bytearray()
        self._begin_identifier: str | None = None
        self._terminal_record: TestRecord | None = None

    def feed(self, chunk: bytes) -> None:
        self._pending.extend(chunk)

        while True:
            newline = self._pending.find(b"\n")
            if newline < 0:
                break

            line = bytes(self._pending[: newline + 1])
            del self._pending[: newline + 1]
            self._parse_line(line)

        if (
            self._pending.startswith(PROTOCOL_PREFIX)
            and len(self._pending) >= MAXIMUM_RECORD_SIZE
        ):
            raise ProtocolError("Warren test record exceeds 96 bytes")

    def finish(self) -> Transcript:
        if self._pending.startswith(PROTOCOL_PREFIX):
            raise ProtocolError("unterminated Warren test record")

        return Transcript(
            begin_identifier=self._begin_identifier,
            terminal_record=self._terminal_record,
        )

    def _parse_line(self, line: bytes) -> None:
        if not line.startswith(PROTOCOL_PREFIX):
            return

        if len(line) > MAXIMUM_RECORD_SIZE:
            raise ProtocolError("Warren test record exceeds 96 bytes")
        if any(byte > 0x7f for byte in line):
            raise ProtocolError("Warren test record is not 7-bit ASCII")

        content = line[:-1]
        if content.endswith(b"\r"):
            content = content[:-1]

        simple_match = _SIMPLE_RECORD.fullmatch(content)
        coded_match = _CODED_RECORD.fullmatch(content)
        if simple_match is not None:
            kind = RecordKind(simple_match.group(1).decode("ascii"))
            identifier = simple_match.group(2).decode("ascii")
            result_code = 0 if kind is RecordKind.PASS else None
        elif coded_match is not None:
            kind = RecordKind(coded_match.group(1).decode("ascii"))
            identifier = coded_match.group(2).decode("ascii")
            result_code = int(coded_match.group(3))
        else:
            raise ProtocolError("malformed Warren test record")

        record = TestRecord(kind, identifier, result_code)
        self._accept_record(record)

    def _accept_record(self, record: TestRecord) -> None:
        if self._terminal_record is not None:
            raise ProtocolError("Warren test record appears after terminal result")

        if record.kind is RecordKind.BEGIN:
            if self._begin_identifier is not None:
                raise ProtocolError("duplicate Warren BEGIN record")
            self._begin_identifier = record.test_identifier
            return

        if record.kind in (RecordKind.PASS, RecordKind.FAIL):
            if self._begin_identifier is None:
                raise ProtocolError(f"{record.kind.value} appears before BEGIN")
            if record.test_identifier != self._begin_identifier:
                raise ProtocolError("terminal test identifier does not match BEGIN")
        elif (
            self._begin_identifier is not None
            and record.test_identifier != self._begin_identifier
        ):
            raise ProtocolError("PANIC test identifier does not match BEGIN")

        if record.kind is RecordKind.FAIL:
            if record.result_code not in (1, 5) and not (
                record.result_code is not None and 64 <= record.result_code <= 95
            ):
                raise ProtocolError("result code is not valid for FAIL")
        elif record.kind is RecordKind.PANIC:
            if record.result_code not in (2, 3, 4):
                raise ProtocolError("result code is not valid for PANIC")

        self._terminal_record = record


def parse_serial_output(serial_output: bytes) -> Transcript:
    parser = SerialProtocolParser()
    parser.feed(serial_output)
    return parser.finish()


def classify_process_result(
    serial_output: bytes,
    process_returncode: int | None,
    *,
    timed_out: bool = False,
    launch_error: bool = False,
) -> HostResult:
    if launch_error:
        return _make_result(
            HostClassification.LAUNCH_ERROR,
            "the QEMU process could not be launched",
        )
    if timed_out:
        return _make_result(
            HostClassification.TIMEOUT,
            "the QEMU process exceeded its deadline",
        )

    try:
        transcript = parse_serial_output(serial_output)
    except ProtocolError as error:
        return _make_result(HostClassification.PROTOCOL_ERROR, str(error))

    terminal = transcript.terminal_record
    if terminal is None:
        if process_returncode is not None and process_returncode != 0:
            return _make_result(
                HostClassification.QEMU_PROCESS_FAILURE,
                f"QEMU exited with status {process_returncode} without a terminal record",
            )
        return _make_result(
            HostClassification.PROTOCOL_ERROR,
            "QEMU exited cleanly without a terminal record",
        )

    expected_returncode = terminal.result_code
    if process_returncode != expected_returncode:
        return _make_result(
            HostClassification.PROTOCOL_ERROR,
            (
                f"serial result {expected_returncode} disagrees with "
                f"QEMU status {process_returncode}"
            ),
            terminal,
        )

    if terminal.kind is RecordKind.PASS:
        classification = HostClassification.PASS
    elif terminal.kind is RecordKind.FAIL:
        classification = HostClassification.EXPLICIT_FAILURE
    else:
        classification = HostClassification.PANIC_OR_ASSERTION

    return _make_result(
        classification,
        f"agreed {terminal.kind.value} result {terminal.result_code}",
        terminal,
    )
