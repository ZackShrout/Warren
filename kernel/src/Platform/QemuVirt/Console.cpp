//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Platform/QemuVirt/Console.h>

#include <burrow/Core/TransitionPlan.h>

static_assert(burrow::platform::qemu_virt::k_pl011_physical_address ==
              burrow::core::k_reference_pl011_physical_address);
static_assert(burrow::platform::qemu_virt::k_pl011_virtual_address ==
              burrow::core::k_reference_pl011_virtual_address);

namespace burrow::platform::qemu_virt {
    console_selection_error_t select_early_console(
        const warren_boot_information_t& boot_information,
        drivers::pl011_device_t& device,
        drivers::console_reader_t& reader,
        drivers::console_writer_t& writer) noexcept
    {
        if ((boot_information.present_features & WARREN_BOOT_FEATURE_EARLY_CONSOLE) == 0)
            return console_selection_error_t::missing_console;

        const warren_boot_section_t& section{ boot_information.early_console };
        if (section.count != 1 || section.stride != sizeof(warren_boot_early_console_t) ||
            section.reserved != 0 ||
            section.offset < boot_information.header_size ||
            (section.offset & (alignof(warren_boot_early_console_t) - 1)) != 0 ||
            section.offset > boot_information.total_size ||
            boot_information.total_size - section.offset < sizeof(warren_boot_early_console_t))
            return console_selection_error_t::invalid_section;

        const auto* bytes{ reinterpret_cast<const uint8_t*>(&boot_information) };
        const auto* record{ reinterpret_cast<const warren_boot_early_console_t*>(
            bytes + section.offset) };
        if (record->kind != WARREN_BOOT_CONSOLE_PL011 ||
            (record->flags & (WARREN_BOOT_CONSOLE_INPUT | WARREN_BOOT_CONSOLE_OUTPUT)) !=
                (WARREN_BOOT_CONSOLE_INPUT | WARREN_BOOT_CONSOLE_OUTPUT) ||
            (record->flags & ~WARREN_BOOT_CONSOLE_KNOWN_FLAGS) != 0 ||
            record->physical_address != k_pl011_physical_address ||
            record->register_stride != sizeof(uint32_t) ||
            record->register_width != 32 || record->reserved_0 != 0 ||
            record->reserved_1[0] != 0 || record->reserved_1[1] != 0 ||
            record->reserved_1[2] != 0)
            return console_selection_error_t::unsupported_console;

        const drivers::pl011_error_t result{ drivers::initialize_pl011(
            reinterpret_cast<volatile uint32_t*>(
                static_cast<uintptr_t>(k_pl011_virtual_address)),
            drivers::k_pl011_default_poll_limit,
            device)
        };
        if (result != drivers::pl011_error_t::success)
            return console_selection_error_t::invalid_mapping;

        reader = drivers::make_pl011_reader(device);
        writer = drivers::make_pl011_writer(device);
        return console_selection_error_t::success;
    }
} // namespace burrow::platform::qemu_virt
