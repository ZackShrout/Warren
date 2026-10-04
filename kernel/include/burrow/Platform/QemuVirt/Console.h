//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Drivers/Console.h>
#include <burrow/Drivers/Pl011.h>

#include <warren/boot/BootInformation.h>

#include <stdint.h>

namespace burrow::platform::qemu_virt {
    constexpr uint64_t k_pl011_physical_address{ UINT64_C(0x09000000) };
    constexpr uint64_t k_pl011_virtual_address{ UINT64_C(0xffffc00009000000) };

    enum class console_selection_error_t : uint32_t
    {
        success = 0,
        missing_console = 1,
        invalid_section = 2,
        unsupported_console = 3,
        invalid_mapping = 4,
    };

    [[nodiscard]] console_selection_error_t select_early_console(
        const warren_boot_information_t& boot_information,
        drivers::pl011_device_t& device,
        drivers::console_reader_t& reader,
        drivers::console_writer_t& writer) noexcept;
} // namespace burrow::platform::qemu_virt
