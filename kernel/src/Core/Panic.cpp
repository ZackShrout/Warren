//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/Panic.h>

namespace
{
    constexpr uint32_t k_report_capacity{ 384 };

    struct report_builder_t
    {
        char bytes[k_report_capacity];
        uint32_t byte_count;
        bool valid;
    };

    void append_character(report_builder_t& builder, char character) noexcept
    {
        if (!builder.valid || builder.byte_count == k_report_capacity)
        {
            builder.valid = false;
            return;
        }
        builder.bytes[builder.byte_count++] = character;
    }

    void append_text(
        report_builder_t& builder,
        const char* text,
        uint32_t byte_count) noexcept
    {
        for (uint32_t index{ 0 }; index < byte_count; ++index)
            append_character(builder, text[index]);
    }

    void append_literal(report_builder_t& builder, const char* text) noexcept
    {
        while (*text != '\0') append_character(builder, *text++);
    }

    void append_decimal(report_builder_t& builder, uint32_t value) noexcept
    {
        char digits[10];
        uint32_t count{};
        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count != 0) append_character(builder, digits[--count]);
    }

    [[nodiscard]] bool valid_identifier_character(char character) noexcept
    {
        return (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9') || character == '.' ||
            character == '_' || character == '-';
    }

    [[nodiscard]] bool valid_file_character(char character) noexcept
    {
        return (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '/' ||
            character == '.' || character == '_' || character == '-';
    }

    [[nodiscard]] bool valid_message_character(char character) noexcept
    {
        return character >= 0x20 && character <= 0x7e &&
            character != '\r' && character != '\n';
    }

    template <typename Validator>
    [[nodiscard]] bool valid_field(
        const char* text,
        uint32_t byte_count,
        uint32_t capacity,
        Validator validator) noexcept
    {
        if (text == nullptr || byte_count == 0 || byte_count > capacity)
            return false;
        for (uint32_t index{ 0 }; index < byte_count; ++index)
            if (!validator(text[index])) return false;
        return true;
    }
}

namespace burrow::core {
    panic_latch_result_t claim_panic_path(uint32_t& latch) noexcept
    {
        if (latch != 0) return panic_latch_result_t::recursive_entry;
        latch = 1;
        return panic_latch_result_t::first_entry;
    }

    panic_report_error_t report_panic(
        const panic_record_t& record,
        const drivers::console_writer_t& writer) noexcept
    {
        if (record.kind != panic_kind_t::assertion &&
            record.kind != panic_kind_t::kernel)
            return panic_report_error_t::invalid_kind;
        if (!valid_field(
                record.identifier,
                record.identifier_byte_count,
                k_panic_identifier_capacity,
                valid_identifier_character))
            return panic_report_error_t::invalid_identifier;
        if (!valid_field(
                record.file,
                record.file_byte_count,
                k_panic_file_capacity,
                valid_file_character))
            return panic_report_error_t::invalid_file;
        if (record.line == 0) return panic_report_error_t::invalid_line;
        if (!valid_field(
                record.message,
                record.message_byte_count,
                k_panic_message_capacity,
                valid_message_character))
            return panic_report_error_t::invalid_message;

        report_builder_t builder;
        builder.byte_count = 0;
        builder.valid = true;
        append_literal(builder, "BURROW_PANIC_V1:kind=");
        append_literal(
            builder,
            record.kind == panic_kind_t::assertion ? "assertion" : "panic");
        append_literal(builder, ":id=");
        append_text(builder, record.identifier, record.identifier_byte_count);
        append_literal(builder, ":file=");
        append_text(builder, record.file, record.file_byte_count);
        append_literal(builder, ":line=");
        append_decimal(builder, record.line);
        append_literal(builder, ":message=");
        append_text(builder, record.message, record.message_byte_count);
        append_literal(builder, "\r\n");

        if (!builder.valid ||
            drivers::write_console(writer, builder.bytes, builder.byte_count) !=
                drivers::console_write_error_t::success)
            return panic_report_error_t::output_failure;
        return panic_report_error_t::success;
    }
} // namespace burrow::core
