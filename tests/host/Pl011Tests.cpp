//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Drivers/Pl011.h>

#include <cstdint>
#include <cstdio>

namespace
{
    [[nodiscard]] bool expect_u32(const char* name,
                                  uint32_t actual,
                                  uint32_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected 0x%x, received 0x%x\n",
                     name, expected, actual);
        return false;
    }
}

int main()
{
    using burrow::drivers::pl011_error_t;

    alignas(uint32_t) volatile uint32_t registers[7]{};
    burrow::drivers::pl011_device_t device{};
    bool passed{ expect_u32(
        "initialize",
        static_cast<uint32_t>(burrow::drivers::initialize_pl011(registers, 4, device)),
        static_cast<uint32_t>(pl011_error_t::success)) };
    passed &= expect_u32(
        "write available byte",
        burrow::drivers::write_pl011_byte(&device, UINT8_C(0xa5)) ? 1 : 0,
        1);
    passed &= expect_u32("data register", registers[0], UINT32_C(0xa5));

    registers[0] = UINT32_C(0x11223344);
    registers[burrow::drivers::k_pl011_flag_register_index] =
        burrow::drivers::k_pl011_transmit_fifo_full;
    passed &= expect_u32(
        "full FIFO timeout",
        burrow::drivers::write_pl011_byte(&device, UINT8_C(0x55)) ? 1 : 0,
        0);
    passed &= expect_u32("timeout preserves data", registers[0], UINT32_C(0x11223344));

    passed &= expect_u32(
        "null context",
        burrow::drivers::write_pl011_byte(nullptr, UINT8_C(0x55)) ? 1 : 0,
        0);
    passed &= expect_u32(
        "null registers",
        static_cast<uint32_t>(burrow::drivers::initialize_pl011(nullptr, 4, device)),
        static_cast<uint32_t>(pl011_error_t::invalid_registers));
    passed &= expect_u32(
        "zero poll limit",
        static_cast<uint32_t>(burrow::drivers::initialize_pl011(registers, 0, device)),
        static_cast<uint32_t>(pl011_error_t::invalid_poll_limit));

    alignas(uint32_t) uint8_t misaligned_storage[32]{};
    auto* misaligned{ reinterpret_cast<volatile uint32_t*>(misaligned_storage + 1) };
    passed &= expect_u32(
        "misaligned registers",
        static_cast<uint32_t>(burrow::drivers::initialize_pl011(misaligned, 4, device)),
        static_cast<uint32_t>(pl011_error_t::invalid_registers));

    if (!passed) return 1;
    std::puts("Warren PL011 tests passed.");
    return 0;
}
