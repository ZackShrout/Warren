//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/ExceptionReporting.h>

namespace
{
    constexpr uint32_t k_report_capacity{ 1024 };

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

    void append_hexadecimal(report_builder_t& builder, uint64_t value) noexcept
    {
        constexpr char digits[]{ "0123456789ABCDEF" };
        append_text(builder, "0x");
        for (uint32_t index{ 0 }; index < 16; ++index)
        {
            const uint32_t shift{ 60 - index * 4 };
            append_character(builder, digits[(value >> shift) & 0xf]);
        }
    }

    void append_field(report_builder_t& builder,
                      const char* name,
                      uint64_t value) noexcept
    {
        append_character(builder, ':');
        append_text(builder, name);
        append_character(builder, '=');
        append_hexadecimal(builder, value);
    }
}

namespace burrow::arch::aarch64 {
    exception_report_error_t report_exception(
        const exception_frame_t& frame,
        const drivers::console_writer_t& console) noexcept
    {
        if (frame.abi_major != k_exception_frame_abi_major ||
            frame.structure_size != k_exception_frame_size || frame.vector >= 16 ||
            frame.reserved != 0 || frame.current_exception_level != 1)
            return exception_report_error_t::invalid_frame;

        report_builder_t report;
        report.byte_count = 0;
        report.valid = true;
        append_text(report, "BURROW_EXCEPTION_V1:stage=");
        append_decimal(report, frame.transition_stage);
        append_text(report, ":vector=");
        append_decimal(report, frame.vector);
        append_text(report, ":el=");
        append_decimal(report, frame.current_exception_level);
        append_field(report, "esr", frame.esr);
        append_field(report, "elr", frame.elr);
        append_field(report, "far", frame.far);
        append_field(report, "spsr", frame.spsr);
        append_field(report, "sp", frame.stack_pointer);
        for (uint32_t index{ 0 }; index < k_exception_register_count; ++index)
        {
            append_text(report, ":x");
            append_decimal(report, index);
            append_character(report, '=');
            append_hexadecimal(report, frame.registers[index]);
        }
        append_text(report, "\r\n");

        if (!report.valid ||
            drivers::write_console(console, report.bytes, report.byte_count) !=
                drivers::console_write_error_t::success)
            return exception_report_error_t::output_failure;
        return exception_report_error_t::success;
    }
} // namespace burrow::arch::aarch64
