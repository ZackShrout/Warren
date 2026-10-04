//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stdint.h>

namespace burrow::drivers {
    enum class console_read_error_t : uint32_t
    {
        success = 0,
        invalid_reader = 1,
        timeout = 2,
        input_failure = 3,
    };

    using console_read_byte_t = console_read_error_t (*)(
        void* context,
        uint8_t& byte) noexcept;
    using console_write_byte_t = bool (*)(void* context, uint8_t byte) noexcept;

    struct console_reader_t
    {
        void* context;
        console_read_byte_t read_byte;
    };

    struct console_writer_t
    {
        void* context;
        console_write_byte_t write_byte;
    };

    enum class console_write_error_t : uint32_t
    {
        success = 0,
        invalid_writer = 1,
        invalid_text = 2,
        output_failure = 3,
    };

    [[nodiscard]] console_write_error_t write_console(
        const console_writer_t& writer,
        const char* text,
        uint32_t byte_count) noexcept;

    [[nodiscard]] console_read_error_t read_console_byte(
        const console_reader_t& reader,
        uint8_t& byte) noexcept;
} // namespace burrow::drivers
