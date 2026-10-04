//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/DiagnosticMonitor.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct input_t
    {
        const char* bytes;
        uint32_t byte_count;
        uint32_t index;
        burrow::drivers::console_read_error_t terminal_result;
    };

    struct output_t
    {
        char bytes[1024]{};
        uint32_t byte_count{};
        uint32_t fail_at{ UINT32_MAX };
    };

    [[nodiscard]] burrow::drivers::console_read_error_t read_byte(
        void* context,
        uint8_t& byte) noexcept
    {
        if (context == nullptr)
            return burrow::drivers::console_read_error_t::input_failure;
        auto& input{ *static_cast<input_t*>(context) };
        if (input.index == input.byte_count) return input.terminal_result;
        byte = static_cast<uint8_t>(input.bytes[input.index++]);
        return burrow::drivers::console_read_error_t::success;
    }

    [[nodiscard]] bool write_byte(void* context, uint8_t byte) noexcept
    {
        if (context == nullptr) return false;
        auto& output{ *static_cast<output_t*>(context) };
        if (output.byte_count == output.fail_at ||
            output.byte_count >= sizeof(output.bytes) - 1)
            return false;
        output.bytes[output.byte_count++] = static_cast<char>(byte);
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

    [[nodiscard]] burrow::core::diagnostic_monitor_error_t run(
        const char* input_bytes,
        burrow::drivers::console_read_error_t terminal_result,
        output_t& output,
        uint64_t timer_ticks = 1) noexcept
    {
        input_t input{
            input_bytes,
            static_cast<uint32_t>(std::strlen(input_bytes)),
            0,
            terminal_result,
        };
        return burrow::core::run_diagnostic_monitor(
            { &input, read_byte }, { &output, write_byte }, timer_ticks);
    }
}

int main()
{
    using burrow::core::diagnostic_monitor_error_t;
    using burrow::drivers::console_read_error_t;

    output_t output{};
    bool passed{ expect_u32(
        "all commands",
        static_cast<uint32_t>(run(
            "help\rstatus\rexit\r",
            console_read_error_t::timeout,
            output,
            UINT64_C(18446744073709551615))),
        static_cast<uint32_t>(diagnostic_monitor_error_t::success)) };
    passed &= expect_text(
        "all command output",
        output.bytes,
        "BURROW_MONITOR:ready:commands=help,status,exit\r\n"
        "BURROW_MONITOR:help=help,status,exit\r\n"
        "BURROW_MONITOR:status=ok:timer_ticks=18446744073709551615\r\n"
        "BURROW_MONITOR:exit=accepted\r\n");

    output = {};
    passed &= expect_u32(
        "line-feed terminator",
        static_cast<uint32_t>(run(
            "exit\n", console_read_error_t::timeout, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::success));

    output = {};
    passed &= expect_u32(
        "unknown and empty commands",
        static_cast<uint32_t>(run(
            "bogus\r\rexit\r", console_read_error_t::timeout, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::success));
    passed &= expect_text(
        "unknown command output",
        output.bytes,
        "BURROW_MONITOR:ready:commands=help,status,exit\r\n"
        "BURROW_MONITOR:error=unknown-command\r\n"
        "BURROW_MONITOR:error=unknown-command\r\n"
        "BURROW_MONITOR:exit=accepted\r\n");

    output = {};
    passed &= expect_u32(
        "command too long",
        static_cast<uint32_t>(run(
            "1234567890123456\r", console_read_error_t::timeout, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::command_too_long));

    output = {};
    passed &= expect_u32(
        "command limit",
        static_cast<uint32_t>(run(
            "x\rx\rx\rx\rx\rx\rx\rx\r",
            console_read_error_t::timeout,
            output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::command_limit_exceeded));

    output = {};
    passed &= expect_u32(
        "input timeout",
        static_cast<uint32_t>(run("", console_read_error_t::timeout, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::input_timeout));
    output = {};
    passed &= expect_u32(
        "input failure",
        static_cast<uint32_t>(run("", console_read_error_t::input_failure, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::input_failure));

    output = {};
    output.fail_at = 0;
    passed &= expect_u32(
        "output failure",
        static_cast<uint32_t>(run("exit\r", console_read_error_t::timeout, output)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::output_failure));

    output = {};
    passed &= expect_u32(
        "zero timer ticks",
        static_cast<uint32_t>(run(
            "exit\r", console_read_error_t::timeout, output, 0)),
        static_cast<uint32_t>(diagnostic_monitor_error_t::invalid_state));
    passed &= expect_u32("invalid state writes nothing", output.byte_count, 0);

    if (!passed) return 1;
    std::puts("Warren diagnostic monitor tests passed.");
    return 0;
}
