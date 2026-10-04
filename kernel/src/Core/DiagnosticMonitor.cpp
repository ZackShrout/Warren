//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/DiagnosticMonitor.h>

namespace
{
    constexpr char k_ready[]{
        "BURROW_MONITOR:ready:commands=help,status,exit\r\n"
    };
    constexpr char k_help[]{
        "BURROW_MONITOR:help=help,status,exit\r\n"
    };
    constexpr char k_unknown[]{
        "BURROW_MONITOR:error=unknown-command\r\n"
    };
    constexpr char k_exit[]{
        "BURROW_MONITOR:exit=accepted\r\n"
    };

    [[nodiscard]] bool equal_command(
        const char* command,
        uint32_t byte_count,
        const char* expected,
        uint32_t expected_byte_count) noexcept
    {
        if (byte_count != expected_byte_count) return false;
        for (uint32_t index{ 0 }; index < byte_count; ++index)
        {
            if (command[index] != expected[index]) return false;
        }
        return true;
    }

    [[nodiscard]] bool write_text(
        const burrow::drivers::console_writer_t& writer,
        const char* text,
        uint32_t byte_count) noexcept
    {
        return burrow::drivers::write_console(writer, text, byte_count) ==
            burrow::drivers::console_write_error_t::success;
    }

    [[nodiscard]] bool write_status(
        const burrow::drivers::console_writer_t& writer,
        uint64_t timer_ticks) noexcept
    {
        char record[80];
        uint32_t count{};
        constexpr char prefix[]{ "BURROW_MONITOR:status=ok:timer_ticks=" };
        for (uint32_t index{ 0 }; index < sizeof(prefix) - 1; ++index)
            record[count++] = prefix[index];

        char digits[20];
        uint32_t digit_count{};
        do
        {
            digits[digit_count++] = static_cast<char>('0' + timer_ticks % 10);
            timer_ticks /= 10;
        } while (timer_ticks != 0);
        while (digit_count != 0) record[count++] = digits[--digit_count];
        record[count++] = '\r';
        record[count++] = '\n';
        return write_text(writer, record, count);
    }
}

namespace burrow::core {
    diagnostic_monitor_error_t run_diagnostic_monitor(
        const drivers::console_reader_t& reader,
        const drivers::console_writer_t& writer,
        uint64_t timer_ticks) noexcept
    {
        if (timer_ticks == 0)
            return diagnostic_monitor_error_t::invalid_state;
        if (!write_text(writer, k_ready, sizeof(k_ready) - 1))
            return diagnostic_monitor_error_t::output_failure;

        for (uint32_t command_index{ 0 };
             command_index < k_diagnostic_monitor_command_limit;
             ++command_index)
        {
            char command[k_diagnostic_monitor_command_capacity];
            uint32_t command_length{};
            while (true)
            {
                uint8_t byte{};
                const drivers::console_read_error_t read_result{
                    drivers::read_console_byte(reader, byte)
                };
                if (read_result == drivers::console_read_error_t::timeout)
                    return diagnostic_monitor_error_t::input_timeout;
                if (read_result != drivers::console_read_error_t::success)
                    return diagnostic_monitor_error_t::input_failure;
                if (byte == '\r' || byte == '\n') break;
                if (command_length == k_diagnostic_monitor_command_capacity - 1)
                    return diagnostic_monitor_error_t::command_too_long;
                command[command_length++] = static_cast<char>(byte);
            }

            if (equal_command(command, command_length, "help", 4))
            {
                if (!write_text(writer, k_help, sizeof(k_help) - 1))
                    return diagnostic_monitor_error_t::output_failure;
                continue;
            }
            if (equal_command(command, command_length, "status", 6))
            {
                if (!write_status(writer, timer_ticks))
                    return diagnostic_monitor_error_t::output_failure;
                continue;
            }
            if (equal_command(command, command_length, "exit", 4))
            {
                if (!write_text(writer, k_exit, sizeof(k_exit) - 1))
                    return diagnostic_monitor_error_t::output_failure;
                return diagnostic_monitor_error_t::success;
            }
            if (!write_text(writer, k_unknown, sizeof(k_unknown) - 1))
                return diagnostic_monitor_error_t::output_failure;
        }

        return diagnostic_monitor_error_t::command_limit_exceeded;
    }
} // namespace burrow::core
