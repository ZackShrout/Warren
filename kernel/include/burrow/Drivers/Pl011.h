//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::drivers {
    constexpr uint32_t k_pl011_data_register_index{ 0 };
    constexpr uint32_t k_pl011_flag_register_index{ 0x18 / sizeof(uint32_t) };
    constexpr uint32_t k_pl011_receive_fifo_empty{ 0x10 };
    constexpr uint32_t k_pl011_transmit_fifo_full{ 0x20 };
    constexpr uint32_t k_pl011_default_poll_limit{ 1000000 };
    constexpr uint32_t k_pl011_monitor_poll_limit{ 100000000 };

    struct pl011_device_t
    {
        volatile uint32_t* registers;
        uint32_t transmit_poll_limit;
        uint32_t receive_poll_limit;
    };

    enum class pl011_error_t : uint32_t
    {
        success = 0,
        invalid_registers = 1,
        invalid_poll_limit = 2,
    };

    [[nodiscard]] pl011_error_t initialize_pl011(
        volatile uint32_t* registers,
        uint32_t transmit_poll_limit,
        pl011_device_t& device) noexcept;

    [[nodiscard]] bool write_pl011_byte(void* context, uint8_t byte) noexcept;

    [[nodiscard]] console_read_error_t read_pl011_byte(
        void* context,
        uint8_t& byte) noexcept;

    [[nodiscard]] console_reader_t make_pl011_reader(pl011_device_t& device) noexcept;
    [[nodiscard]] console_writer_t make_pl011_writer(pl011_device_t& device) noexcept;
} // namespace burrow::drivers
