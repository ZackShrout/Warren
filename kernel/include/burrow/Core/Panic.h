//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::core {
    constexpr uint32_t k_panic_identifier_capacity{ 32 };
    constexpr uint32_t k_panic_file_capacity{ 96 };
    constexpr uint32_t k_panic_message_capacity{ 128 };

    enum class panic_kind_t : uint32_t
    {
        assertion = 2,
        kernel = 3,
    };

    struct panic_record_t
    {
        panic_kind_t kind;
        const char* identifier;
        uint32_t identifier_byte_count;
        const char* file;
        uint32_t file_byte_count;
        uint32_t line;
        const char* message;
        uint32_t message_byte_count;
    };

    enum class panic_report_error_t : uint32_t
    {
        success = 0,
        invalid_kind = 1,
        invalid_identifier = 2,
        invalid_file = 3,
        invalid_line = 4,
        invalid_message = 5,
        output_failure = 6,
    };

    enum class panic_latch_result_t : uint32_t
    {
        first_entry = 0,
        recursive_entry = 1,
    };

    [[nodiscard]] panic_latch_result_t claim_panic_path(
        uint32_t& latch) noexcept;

    [[nodiscard]] panic_report_error_t report_panic(
        const panic_record_t& record,
        const drivers::console_writer_t& writer) noexcept;
} // namespace burrow::core
