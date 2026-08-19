//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootHandoffStorage.h>
#include <warren/boot/BurrowLoader.h>

#include <warren/boot/BootInformation.h>

#include <stdint.h>

namespace warren::boot {
    struct aarch64_executable_range_t
    {
        uint64_t physical_begin;
        uint64_t physical_end;
    };

    struct aarch64_handoff_arguments_t
    {
        uint64_t boot_information_physical_address;
        uint64_t entry_physical_address;
        uint64_t bootstrap_stack_top;
        uint64_t pl011_physical_address;
    };

    enum class aarch64_handoff_error_t : uint32_t
    {
        success = 0,
        invalid_storage = 1,
        invalid_loaded_image = 2,
        invalid_executable_segment = 3,
        executable_range_overflow = 4,
        executable_range_outside_image = 5,
        invalid_entry = 6,
        invalid_stack = 7,
        invalid_boot_information = 8,
        invalid_console = 9,
    };

    [[nodiscard]] aarch64_handoff_error_t prepare_aarch64_handoff(
        const burrow_load_plan_t& load_plan,
        const burrow_loaded_image_t& loaded_image,
        const boot_handoff_storage_t& storage,
        const warren_boot_early_console_t& early_console,
        aarch64_executable_range_t& executable_range,
        aarch64_handoff_arguments_t& arguments) noexcept;

    [[nodiscard]] bool synchronize_aarch64_instruction_cache(
        const aarch64_executable_range_t& executable_range) noexcept;

    [[noreturn]] void transfer_to_burrow(
        const aarch64_handoff_arguments_t& arguments) noexcept;

    // This path is valid after the first ExitBootServices() attempt and never
    // calls firmware, returns, or attempts a second result transport.
    [[noreturn]] void wait_after_aarch64_handoff_failure(
        uint64_t pl011_physical_address) noexcept;
} // namespace warren::boot
