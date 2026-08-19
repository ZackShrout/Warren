//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/AArch64Handoff.h>

#include <stdint.h>

namespace warren::boot {
    namespace {
        [[nodiscard]] bool add_without_overflow(uint64_t left, uint64_t right,
                                                uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(uint64_t left, uint64_t right,
                                                     uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left) return false;

            result = left * right;
            return true;
        }
    } // anonymous namespace

    aarch64_handoff_error_t prepare_aarch64_handoff(
        const burrow_load_plan_t& load_plan,
        const burrow_loaded_image_t& loaded_image,
        const boot_handoff_storage_t& storage,
        const warren_boot_early_console_t& early_console,
        aarch64_executable_range_t& executable_range,
        aarch64_handoff_arguments_t& arguments) noexcept
    {
        executable_range = { };
        arguments = { };

        uint64_t image_end{ 0 };
        if (loaded_image.physical_start == 0 ||
            (loaded_image.physical_start % k_burrow_page_size) != 0 ||
            loaded_image.physical_size == 0 ||
            (loaded_image.physical_size % k_burrow_page_size) != 0 ||
            loaded_image.physical_size != load_plan.allocation_size ||
            loaded_image.load_bias > loaded_image.physical_start ||
            !add_without_overflow(
                loaded_image.physical_start, loaded_image.physical_size, image_end))
            return aarch64_handoff_error_t::invalid_loaded_image;

        const burrow_load_segment_t& executable{
            load_plan.segments[static_cast<uint32_t>(burrow_segment_class_t::executable)]
        };
        if (executable.segment_class != burrow_segment_class_t::executable ||
            executable.memory_size == 0)
            return aarch64_handoff_error_t::invalid_executable_segment;

        uint64_t executable_begin{ 0 };
        uint64_t executable_end{ 0 };
        if (!add_without_overflow(
                loaded_image.load_bias, executable.virtual_address, executable_begin) ||
            !add_without_overflow(
                executable_begin, executable.memory_size, executable_end))
            return aarch64_handoff_error_t::executable_range_overflow;

        if (executable_begin < loaded_image.physical_start || executable_end > image_end)
            return aarch64_handoff_error_t::executable_range_outside_image;

        if ((executable_begin & 3U) != 0 ||
            (loaded_image.entry_physical_address & 3U) != 0 ||
            loaded_image.entry_physical_address < executable_begin ||
            loaded_image.entry_physical_address >= executable_end)
            return aarch64_handoff_error_t::invalid_entry;

        uint64_t stack_size{ 0 };
        uint64_t stack_top{ 0 };
        if (!multiply_without_overflow(
                storage.bootstrap_stack.page_count,
                WARREN_BOOT_INFORMATION_PAGE_SIZE,
                stack_size) ||
            !add_without_overflow(
                storage.bootstrap_stack.physical_start, stack_size, stack_top) ||
            (stack_top & 15U) != 0)
            return aarch64_handoff_error_t::invalid_stack;

        if (storage.object.physical_start == 0 ||
            (storage.object.physical_start & 7U) != 0)
            return aarch64_handoff_error_t::invalid_boot_information;

        if (!boot_handoff_storage_is_valid(storage))
            return aarch64_handoff_error_t::invalid_storage;

        if (early_console.kind != WARREN_BOOT_CONSOLE_PL011 ||
            (early_console.flags & WARREN_BOOT_CONSOLE_OUTPUT) == 0 ||
            (early_console.flags & ~WARREN_BOOT_CONSOLE_KNOWN_FLAGS) != 0 ||
            early_console.physical_address == 0 ||
            (early_console.physical_address & 3U) != 0 ||
            early_console.physical_address > UINT64_MAX - 0x18 ||
            early_console.register_stride != 4 ||
            early_console.register_width != 32 ||
            early_console.reserved_0 != 0)
            return aarch64_handoff_error_t::invalid_console;

        for (uint32_t index{ 0 }; index < 3; ++index)
        {
            if (early_console.reserved_1[index] != 0)
                return aarch64_handoff_error_t::invalid_console;
        }

        executable_range = { executable_begin, executable_end };
        arguments = {
            storage.object.physical_start,
            loaded_image.entry_physical_address,
            stack_top,
            early_console.physical_address,
        };
        return aarch64_handoff_error_t::success;
    }
} // namespace warren::boot
