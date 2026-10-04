//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Drivers/Console.h>

namespace burrow::drivers {
    console_read_error_t read_console_byte(
        const console_reader_t& reader,
        uint8_t& byte) noexcept
    {
        byte = 0;
        if (reader.context == nullptr || reader.read_byte == nullptr)
            return console_read_error_t::invalid_reader;
        return reader.read_byte(reader.context, byte);
    }

    console_write_error_t write_console(
        const console_writer_t& writer,
        const char* text,
        uint32_t byte_count) noexcept
    {
        if (writer.context == nullptr || writer.write_byte == nullptr)
            return console_write_error_t::invalid_writer;
        if (text == nullptr)
            return console_write_error_t::invalid_text;

        for (uint32_t index{ 0 }; index < byte_count; ++index)
        {
            if (!writer.write_byte(writer.context, static_cast<uint8_t>(text[index])))
                return console_write_error_t::output_failure;
        }

        return console_write_error_t::success;
    }
} // namespace burrow::drivers
