//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Drivers/Console.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct capture_t
    {
        char bytes[32]{};
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

    [[nodiscard]] bool expect_u32(const char* name,
                                  uint32_t actual,
                                  uint32_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected %u, received %u\n",
                     name, expected, actual);
        return false;
    }
}

int main()
{
    using burrow::drivers::console_write_error_t;

    capture_t capture{};
    const burrow::drivers::console_writer_t writer{ &capture, capture_byte };
    bool passed{ expect_u32(
        "exact write",
        static_cast<uint32_t>(burrow::drivers::write_console(writer, "Burrow", 6)),
        static_cast<uint32_t>(console_write_error_t::success)) };
    passed &= expect_u32("exact byte count", capture.byte_count, 6);
    passed &= expect_u32("exact content", std::strcmp(capture.bytes, "Burrow"), 0);

    passed &= expect_u32(
        "empty write",
        static_cast<uint32_t>(burrow::drivers::write_console(writer, "", 0)),
        static_cast<uint32_t>(console_write_error_t::success));

    const burrow::drivers::console_writer_t null_context{ nullptr, capture_byte };
    passed &= expect_u32(
        "null context",
        static_cast<uint32_t>(burrow::drivers::write_console(null_context, "x", 1)),
        static_cast<uint32_t>(console_write_error_t::invalid_writer));
    const burrow::drivers::console_writer_t null_function{ &capture, nullptr };
    passed &= expect_u32(
        "null function",
        static_cast<uint32_t>(burrow::drivers::write_console(null_function, "x", 1)),
        static_cast<uint32_t>(console_write_error_t::invalid_writer));
    passed &= expect_u32(
        "null text",
        static_cast<uint32_t>(burrow::drivers::write_console(writer, nullptr, 0)),
        static_cast<uint32_t>(console_write_error_t::invalid_text));

    capture = {};
    capture.fail_at = 3;
    passed &= expect_u32(
        "mid-write failure",
        static_cast<uint32_t>(burrow::drivers::write_console(writer, "abcdef", 6)),
        static_cast<uint32_t>(console_write_error_t::output_failure));
    passed &= expect_u32("mid-write byte count", capture.byte_count, 3);
    passed &= expect_u32("mid-write prefix", std::strcmp(capture.bytes, "abc"), 0);

    if (!passed) return 1;
    std::puts("Warren console tests passed.");
    return 0;
}
