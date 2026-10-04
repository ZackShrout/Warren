//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::core {
    constexpr uint32_t k_diagnostic_monitor_command_capacity{ 16 };
    constexpr uint32_t k_diagnostic_monitor_command_limit{ 8 };

    enum class diagnostic_monitor_error_t : uint32_t
    {
        success = 0,
        invalid_state = 1,
        input_timeout = 2,
        input_failure = 3,
        command_too_long = 4,
        command_limit_exceeded = 5,
        output_failure = 6,
    };

    [[nodiscard]] diagnostic_monitor_error_t run_diagnostic_monitor(
        const drivers::console_reader_t& reader,
        const drivers::console_writer_t& writer,
        uint64_t timer_ticks) noexcept;
} // namespace burrow::core
