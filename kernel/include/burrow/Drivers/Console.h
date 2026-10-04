//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stdint.h>

namespace burrow::drivers {
    using console_write_byte_t = bool (*)(void* context, uint8_t byte) noexcept;

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
} // namespace burrow::drivers
