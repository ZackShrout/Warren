//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::arch::aarch64 {
    constexpr uint32_t k_reference_timer_rate_hz{ 100 };
    constexpr uint32_t k_physical_timer_interrupt_id{ 30 };
    constexpr uint64_t k_physical_timer_maximum_interval{ UINT64_C(0x7fffffff) };

    enum class generic_timer_error_t : uint32_t
    {
        success = 0,
        invalid_frequency = 1,
        invalid_rate = 2,
        interval_out_of_range = 3,
        invalid_tick_count = 4,
        output_failure = 5,
    };

    [[nodiscard]] generic_timer_error_t calculate_timer_interval(
        uint64_t counter_frequency,
        uint32_t requested_rate_hz,
        uint32_t& interval) noexcept;

    [[nodiscard]] generic_timer_error_t report_timer_tick(
        const drivers::console_writer_t& console,
        uint64_t counter_frequency,
        uint64_t tick_count) noexcept;
} // namespace burrow::arch::aarch64

extern "C" uint64_t burrow_aarch64_read_counter_frequency() noexcept;
extern "C" void burrow_aarch64_arm_physical_timer(uint32_t interval) noexcept;
extern "C" void burrow_aarch64_disable_physical_timer() noexcept;
