//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/Panic.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct capture_t
    {
        char bytes[512]{};
        uint32_t byte_count{};
        uint32_t fail_at{ UINT32_MAX };
    };

    [[nodiscard]] bool capture_byte(void* context, uint8_t byte) noexcept
    {
        if (context == nullptr) return false;
        auto& capture{ *static_cast<capture_t*>(context) };
        if (capture.byte_count == capture.fail_at ||
            capture.byte_count >= sizeof(capture.bytes) - 1)
            return false;
        capture.bytes[capture.byte_count++] = static_cast<char>(byte);
        return true;
    }

    [[nodiscard]] bool expect_u32(
        const char* name,
        uint32_t actual,
        uint32_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected %u, received %u\n",
                     name, expected, actual);
        return false;
    }

    [[nodiscard]] bool expect_text(
        const char* name,
        const char* actual,
        const char* expected) noexcept
    {
        if (std::strcmp(actual, expected) == 0) return true;
        std::fprintf(stderr, "FAIL: %s\nexpected: %sreceived: %s",
                     name, expected, actual);
        return false;
    }

    [[nodiscard]] burrow::core::panic_record_t make_record(
        burrow::core::panic_kind_t kind) noexcept
    {
        static constexpr char identifier[]{ "phase1.contract" };
        static constexpr char file[]{ "kernel/src/Core/Panic.cpp" };
        static constexpr char message[]{ "required invariant failed" };
        return {
            kind,
            identifier,
            sizeof(identifier) - 1,
            file,
            sizeof(file) - 1,
            42,
            message,
            sizeof(message) - 1,
        };
    }
}

int main()
{
    using burrow::core::panic_kind_t;
    using burrow::core::panic_latch_result_t;
    using burrow::core::panic_report_error_t;

    capture_t capture{};
    burrow::drivers::console_writer_t writer{ &capture, capture_byte };
    burrow::core::panic_record_t record{ make_record(panic_kind_t::assertion) };
    bool passed{ expect_u32(
        "assertion record",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::success)) };
    passed &= expect_text(
        "assertion output",
        capture.bytes,
        "BURROW_PANIC_V1:kind=assertion:id=phase1.contract:"
        "file=kernel/src/Core/Panic.cpp:line=42:"
        "message=required invariant failed\r\n");

    capture = {};
    record = make_record(panic_kind_t::kernel);
    passed &= expect_u32(
        "kernel panic record",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::success));
    passed &= expect_text(
        "kernel panic output",
        capture.bytes,
        "BURROW_PANIC_V1:kind=panic:id=phase1.contract:"
        "file=kernel/src/Core/Panic.cpp:line=42:"
        "message=required invariant failed\r\n");

    char maximum_identifier[burrow::core::k_panic_identifier_capacity];
    char maximum_file[burrow::core::k_panic_file_capacity];
    char maximum_message[burrow::core::k_panic_message_capacity];
    std::memset(maximum_identifier, 'i', sizeof(maximum_identifier));
    std::memset(maximum_file, 'f', sizeof(maximum_file));
    std::memset(maximum_message, 'm', sizeof(maximum_message));
    capture = {};
    record = {
        panic_kind_t::kernel,
        maximum_identifier,
        sizeof(maximum_identifier),
        maximum_file,
        sizeof(maximum_file),
        UINT32_MAX,
        maximum_message,
        sizeof(maximum_message),
    };
    passed &= expect_u32(
        "maximum-size fields",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::success));
    passed &= expect_u32("maximum-size output", capture.byte_count, 319);

    record = make_record(panic_kind_t::kernel);
    record.kind = static_cast<panic_kind_t>(0);
    passed &= expect_u32(
        "invalid kind",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_kind));

    record = make_record(panic_kind_t::kernel);
    record.identifier = nullptr;
    passed &= expect_u32(
        "null identifier",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_identifier));
    record = make_record(panic_kind_t::kernel);
    record.identifier = "Bad";
    record.identifier_byte_count = 3;
    passed &= expect_u32(
        "invalid identifier character",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_identifier));
    record = make_record(panic_kind_t::kernel);
    record.identifier_byte_count = burrow::core::k_panic_identifier_capacity + 1;
    passed &= expect_u32(
        "identifier too long",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_identifier));

    record = make_record(panic_kind_t::kernel);
    record.file = "bad file";
    record.file_byte_count = 8;
    passed &= expect_u32(
        "invalid file",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_file));
    record = make_record(panic_kind_t::kernel);
    record.line = 0;
    passed &= expect_u32(
        "invalid line",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_line));
    record = make_record(panic_kind_t::kernel);
    record.message = "bad\nmessage";
    record.message_byte_count = 11;
    passed &= expect_u32(
        "invalid message",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::invalid_message));

    capture = {};
    capture.fail_at = 7;
    record = make_record(panic_kind_t::kernel);
    passed &= expect_u32(
        "output failure",
        static_cast<uint32_t>(burrow::core::report_panic(record, writer)),
        static_cast<uint32_t>(panic_report_error_t::output_failure));
    passed &= expect_u32("output failure prefix", capture.byte_count, 7);

    uint32_t latch{};
    passed &= expect_u32(
        "first panic entry",
        static_cast<uint32_t>(burrow::core::claim_panic_path(latch)),
        static_cast<uint32_t>(panic_latch_result_t::first_entry));
    passed &= expect_u32("latch claimed", latch, 1);
    passed &= expect_u32(
        "recursive panic entry",
        static_cast<uint32_t>(burrow::core::claim_panic_path(latch)),
        static_cast<uint32_t>(panic_latch_result_t::recursive_entry));

    if (!passed) return 1;
    std::puts("Warren panic tests passed.");
    return 0;
}
