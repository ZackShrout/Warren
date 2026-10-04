//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Drivers/Pl011.h>

#include <stdint.h>

namespace burrow::drivers {
    pl011_error_t initialize_pl011(
        volatile uint32_t* registers,
        uint32_t transmit_poll_limit,
        pl011_device_t& device) noexcept
    {
        if (registers == nullptr ||
            (reinterpret_cast<uintptr_t>(registers) & (alignof(uint32_t) - 1)) != 0)
            return pl011_error_t::invalid_registers;
        if (transmit_poll_limit == 0)
            return pl011_error_t::invalid_poll_limit;

        device.registers = registers;
        device.transmit_poll_limit = transmit_poll_limit;
        return pl011_error_t::success;
    }

    bool write_pl011_byte(void* context, uint8_t byte) noexcept
    {
        if (context == nullptr) return false;

        auto& device{ *static_cast<pl011_device_t*>(context) };
        if (device.registers == nullptr || device.transmit_poll_limit == 0)
            return false;

        for (uint32_t poll{ 0 }; poll < device.transmit_poll_limit; ++poll)
        {
            if ((device.registers[k_pl011_flag_register_index] &
                 k_pl011_transmit_fifo_full) != 0)
                continue;

            device.registers[k_pl011_data_register_index] = byte;
            return true;
        }

        return false;
    }

    console_writer_t make_pl011_writer(pl011_device_t& device) noexcept
    {
        return { &device, write_pl011_byte };
    }
} // namespace burrow::drivers
