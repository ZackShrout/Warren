//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/GenericTimer.h>

namespace
{
    constexpr uint32_t k_report_capacity{ 128 };

    struct report_builder_t
    {
        char bytes[k_report_capacity];
        uint32_t byte_count;
        bool valid;
    };

    void append_character(report_builder_t& builder, char character) noexcept
    {
        if (!builder.valid || builder.byte_count >= k_report_capacity)
        {
            builder.valid = false;
            return;
        }
        builder.bytes[builder.byte_count++] = character;
    }

    void append_text(report_builder_t& builder, const char* text) noexcept
    {
        while (*text != '\0') append_character(builder, *text++);
    }

    void append_decimal(report_builder_t& builder, uint64_t value) noexcept
    {
        char digits[20];
        uint32_t count{};
        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count != 0) append_character(builder, digits[--count]);
    }
}

namespace burrow::arch::aarch64 {
    generic_timer_error_t calculate_timer_interval(
        uint64_t counter_frequency,
        uint32_t requested_rate_hz,
        uint32_t& interval) noexcept
    {
        interval = 0;
        if (counter_frequency == 0)
            return generic_timer_error_t::invalid_frequency;
        if (requested_rate_hz == 0)
            return generic_timer_error_t::invalid_rate;

        const uint64_t calculated{ counter_frequency / requested_rate_hz };
        if (calculated == 0 || calculated > k_physical_timer_maximum_interval)
            return generic_timer_error_t::interval_out_of_range;

        interval = static_cast<uint32_t>(calculated);
        return generic_timer_error_t::success;
    }

    generic_timer_error_t report_timer_tick(
        const drivers::console_writer_t& console,
        uint64_t counter_frequency,
        uint64_t tick_count) noexcept
    {
        if (counter_frequency == 0)
            return generic_timer_error_t::invalid_frequency;
        if (tick_count == 0)
            return generic_timer_error_t::invalid_tick_count;

        report_builder_t report;
        report.byte_count = 0;
        report.valid = true;
        append_text(report, "BURROW_TIMER:source=cntp:interrupt=");
        append_decimal(report, k_physical_timer_interrupt_id);
        append_text(report, ":ticks=");
        append_decimal(report, tick_count);
        append_text(report, ":frequency=");
        append_decimal(report, counter_frequency);
        append_text(report, "\r\n");

        if (!report.valid ||
            drivers::write_console(console, report.bytes, report.byte_count) !=
                drivers::console_write_error_t::success)
            return generic_timer_error_t::output_failure;
        return generic_timer_error_t::success;
    }
} // namespace burrow::arch::aarch64
