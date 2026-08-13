//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformationValidation.h>

#include <warren/boot/BootInformation.h>

namespace warren::boot
{
    namespace
    {
        struct range_t
        {
            uint64_t begin;
            uint64_t end;
            bool present;
        };

        struct section_range_t
        {
            uint64_t begin;
            uint64_t end;
            bool present;
        };

        [[nodiscard]] bool add_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left)
                return false;

            result = left * right;
            return true;
        }

        [[nodiscard]] bool make_range(
            uint64_t begin,
            uint64_t size,
            range_t& range) noexcept
        {
            if (begin == 0 || size == 0)
                return false;

            uint64_t end{ 0 };
            if (!add_without_overflow(begin, size, end))
                return false;

            range = { begin, end, true };
            return true;
        }

        [[nodiscard]] bool ranges_overlap(
            const range_t& left,
            const range_t& right) noexcept
        {
            return left.present && right.present &&
                   left.begin < right.end && right.begin < left.end;
        }

        [[nodiscard]] bool section_ranges_overlap(
            const section_range_t& left,
            const section_range_t& right) noexcept
        {
            return left.present && right.present &&
                   left.begin < right.end && right.begin < left.end;
        }

        [[nodiscard]] bool bytes_are_zero(
            const uint8_t* bytes,
            uint64_t count) noexcept
        {
            for (uint64_t index{ 0 }; index < count; ++index)
            {
                if (bytes[index] != 0)
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool section_is_zero(
            const warren_boot_section_t& section) noexcept
        {
            return section.offset == 0 && section.count == 0 &&
                   section.stride == 0 && section.reserved == 0;
        }

        [[nodiscard]] boot_information_error_t validate_section(
            const warren_boot_section_t& section,
            bool present,
            uint32_t expected_stride,
            uint32_t expected_count,
            uint32_t header_size,
            uint32_t total_size,
            section_range_t& range) noexcept
        {
            range = { 0, 0, false };

            if (!present)
            {
                if (!section_is_zero(section))
                    return boot_information_error_t::invalid_section_descriptor;

                return boot_information_error_t::success;
            }

            if (section.reserved != 0 || section.count == 0 ||
                section.stride != expected_stride ||
                (expected_count != 0 && section.count != expected_count) ||
                section.offset < header_size || (section.offset & 7U) != 0)
                return boot_information_error_t::invalid_section_descriptor;

            uint64_t byte_count{ 0 };
            if (!multiply_without_overflow(section.count, section.stride, byte_count))
                return boot_information_error_t::section_out_of_bounds;

            uint64_t end{ 0 };
            if (!add_without_overflow(section.offset, byte_count, end) || end > total_size)
                return boot_information_error_t::section_out_of_bounds;

            range = { section.offset, end, true };
            return boot_information_error_t::success;
        }

        [[nodiscard]] bool valid_utf8(
            const uint8_t* bytes,
            uint32_t count) noexcept
        {
            uint32_t index{ 0 };
            while (index < count)
            {
                const uint8_t first{ bytes[index] };
                if (first == 0)
                    return false;

                if (first <= 0x7f)
                {
                    ++index;
                    continue;
                }

                uint32_t length{ 0 };
                uint32_t code_point{ 0 };
                uint32_t minimum{ 0 };
                if ((first & 0xe0U) == 0xc0U)
                {
                    length = 2;
                    code_point = first & 0x1fU;
                    minimum = 0x80;
                }
                else if ((first & 0xf0U) == 0xe0U)
                {
                    length = 3;
                    code_point = first & 0x0fU;
                    minimum = 0x800;
                }
                else if ((first & 0xf8U) == 0xf0U)
                {
                    length = 4;
                    code_point = first & 0x07U;
                    minimum = 0x10000;
                }
                else
                    return false;

                if (length > count - index)
                    return false;

                for (uint32_t continuation{ 1 }; continuation < length; ++continuation)
                {
                    const uint8_t byte{ bytes[index + continuation] };
                    if ((byte & 0xc0U) != 0x80U)
                        return false;

                    code_point = (code_point << 6U) | (byte & 0x3fU);
                }

                if (code_point < minimum || code_point > 0x10ffffU ||
                    (code_point >= 0xd800U && code_point <= 0xdfffU))
                    return false;

                index += length;
            }

            return true;
        }

        [[nodiscard]] bool memory_map_covers(
            const warren_boot_memory_entry_t* entries,
            uint32_t entry_count,
            uint64_t physical_start,
            uint64_t size,
            uint32_t required_kind) noexcept
        {
            uint64_t required_end{ 0 };
            if (!add_without_overflow(physical_start, size, required_end))
                return false;

            uint64_t cursor{ physical_start };
            for (uint32_t index{ 0 }; index < entry_count; ++index)
            {
                const warren_boot_memory_entry_t& entry{ entries[index] };
                uint64_t entry_size{ 0 };
                uint64_t entry_end{ 0 };
                if (!multiply_without_overflow(
                        entry.page_count,
                        WARREN_BOOT_INFORMATION_PAGE_SIZE,
                        entry_size) ||
                    !add_without_overflow(entry.physical_start, entry_size, entry_end))
                    return false;

                if (entry_end <= cursor)
                    continue;
                if (entry.physical_start > cursor || entry.memory_kind != required_kind)
                    return false;

                cursor = entry_end < required_end ? entry_end : required_end;
                if (cursor == required_end)
                    return true;
            }

            return false;
        }
    }

    boot_information_error_t validate_boot_information(
        const void* object,
        uint32_t readable_size,
        uint64_t physical_address) noexcept
    {
        if (object == nullptr)
            return boot_information_error_t::null_object;

        if ((reinterpret_cast<uintptr_t>(object) & 7U) != 0 ||
            (physical_address & 7U) != 0)
            return boot_information_error_t::unaligned_object;

        if (readable_size < WARREN_BOOT_INFORMATION_HEADER_SIZE)
            return boot_information_error_t::truncated_fixed_header;

        const auto* bytes{ static_cast<const uint8_t*>(object) };
        const auto& header{ *static_cast<const warren_boot_information_t*>(object) };

        constexpr uint8_t magic[WARREN_BOOT_INFORMATION_MAGIC_SIZE]{
            'W', 'A', 'R', 'R', 'E', 'N', 'B', 'I'
        };
        for (uint32_t index{ 0 }; index < WARREN_BOOT_INFORMATION_MAGIC_SIZE; ++index)
        {
            if (header.magic[index] != magic[index])
                return boot_information_error_t::invalid_magic;
        }

        if (header.major != WARREN_BOOT_INFORMATION_MAJOR)
            return boot_information_error_t::unsupported_major;

        if (header.header_size < WARREN_BOOT_INFORMATION_HEADER_SIZE ||
            (header.header_size & 7U) != 0 ||
            header.header_size > header.total_size)
            return boot_information_error_t::invalid_header_size;

        if (header.total_size > readable_size || (header.total_size & 7U) != 0)
            return boot_information_error_t::invalid_total_size;

        if (header.page_size != WARREN_BOOT_INFORMATION_PAGE_SIZE)
            return boot_information_error_t::invalid_page_size;

        if (header.self_physical_address != physical_address)
            return boot_information_error_t::self_address_mismatch;

        if (header.minor == 0 && !bytes_are_zero(header.reserved, sizeof(header.reserved)))
            return boot_information_error_t::nonzero_reserved;

        if ((header.required_features & ~header.present_features) != 0)
            return boot_information_error_t::invalid_feature_masks;
        if ((header.required_features & ~WARREN_BOOT_FEATURE_KNOWN_MASK) != 0)
            return boot_information_error_t::unsupported_required_feature;
        if ((header.present_features & WARREN_BOOT_FEATURE_MEMORY_MAP) == 0 ||
            (header.required_features & WARREN_BOOT_FEATURE_MEMORY_MAP) == 0)
            return boot_information_error_t::missing_memory_map;

        range_t object_range{ 0, 0, false };
        range_t kernel_range{ 0, 0, false };
        range_t stack_range{ 0, 0, false };
        range_t initial_image_range{ 0, 0, false };

        if (!make_range(physical_address, header.total_size, object_range))
            return boot_information_error_t::invalid_total_size;

        if ((header.kernel_physical_start % header.page_size) != 0 ||
            (header.kernel_physical_size % header.page_size) != 0 ||
            header.kernel_load_bias > header.kernel_physical_start ||
            !make_range(
                header.kernel_physical_start,
                header.kernel_physical_size,
                kernel_range))
            return boot_information_error_t::invalid_kernel_range;

        if (header.kernel_entry_physical_address < kernel_range.begin ||
            header.kernel_entry_physical_address >= kernel_range.end)
            return boot_information_error_t::invalid_entry_address;

        if ((header.bootstrap_stack_physical_start % header.page_size) != 0 ||
            (header.bootstrap_stack_size % header.page_size) != 0 ||
            !make_range(
                header.bootstrap_stack_physical_start,
                header.bootstrap_stack_size,
                stack_range) ||
            (stack_range.end & 15U) != 0)
            return boot_information_error_t::invalid_stack_range;

        section_range_t section_ranges[4]{};
        boot_information_error_t section_result{ validate_section(
            header.memory_map,
            true,
            sizeof(warren_boot_memory_entry_t),
            0,
            header.header_size,
            header.total_size,
            section_ranges[0]) };
        if (section_result != boot_information_error_t::success)
            return section_result;

        section_result = validate_section(
            header.command_line,
            (header.present_features & WARREN_BOOT_FEATURE_COMMAND_LINE) != 0,
            1,
            0,
            header.header_size,
            header.total_size,
            section_ranges[1]);
        if (section_result != boot_information_error_t::success)
            return section_result;

        section_result = validate_section(
            header.early_console,
            (header.present_features & WARREN_BOOT_FEATURE_EARLY_CONSOLE) != 0,
            sizeof(warren_boot_early_console_t),
            1,
            header.header_size,
            header.total_size,
            section_ranges[2]);
        if (section_result != boot_information_error_t::success)
            return section_result;

        section_result = validate_section(
            header.framebuffer,
            (header.present_features & WARREN_BOOT_FEATURE_FRAMEBUFFER) != 0,
            sizeof(warren_boot_framebuffer_t),
            1,
            header.header_size,
            header.total_size,
            section_ranges[3]);
        if (section_result != boot_information_error_t::success)
            return section_result;

        for (uint32_t left{ 0 }; left < 4; ++left)
        {
            for (uint32_t right{ left + 1 }; right < 4; ++right)
            {
                if (section_ranges_overlap(section_ranges[left], section_ranges[right]))
                    return boot_information_error_t::overlapping_sections;
            }
        }

        if (header.minor == 0)
        {
            for (uint32_t index{ header.header_size }; index < header.total_size; ++index)
            {
                bool described{ false };
                for (const section_range_t& section : section_ranges)
                {
                    if (section.present && index >= section.begin && index < section.end)
                    {
                        described = true;
                        break;
                    }
                }

                if (!described && bytes[index] != 0)
                    return boot_information_error_t::nonzero_padding;
            }
        }

        const bool initial_image_present{
            (header.present_features & WARREN_BOOT_FEATURE_INITIAL_IMAGE) != 0
        };
        if (initial_image_present)
        {
            if (!make_range(
                    header.initial_image_physical_start,
                    header.initial_image_size,
                    initial_image_range))
                return boot_information_error_t::invalid_optional_resource;
        }
        else if (header.initial_image_physical_start != 0 ||
                 header.initial_image_size != 0)
            return boot_information_error_t::invalid_optional_resource;

        const bool acpi_present{
            (header.present_features & WARREN_BOOT_FEATURE_ACPI_RSDP) != 0
        };
        if ((acpi_present &&
             (header.acpi_rsdp_physical_address == 0 ||
              (header.acpi_rsdp_physical_address & 15U) != 0)) ||
            (!acpi_present && header.acpi_rsdp_physical_address != 0))
            return boot_information_error_t::invalid_optional_resource;

        const bool device_tree_present{
            (header.present_features & WARREN_BOOT_FEATURE_DEVICE_TREE) != 0
        };
        range_t device_tree_range{ 0, 0, false };
        if (device_tree_present)
        {
            if (!make_range(
                    header.device_tree_physical_address,
                    header.device_tree_size,
                    device_tree_range))
                return boot_information_error_t::invalid_optional_resource;
        }
        else if (header.device_tree_physical_address != 0 ||
                 header.device_tree_size != 0)
            return boot_information_error_t::invalid_optional_resource;

        const range_t ranges[]{
            object_range,
            kernel_range,
            stack_range,
            initial_image_range,
        };
        for (uint32_t left{ 0 }; left < 4; ++left)
        {
            for (uint32_t right{ left + 1 }; right < 4; ++right)
            {
                if (ranges_overlap(ranges[left], ranges[right]))
                    return boot_information_error_t::overlapping_physical_resources;
            }
        }

        const auto* memory_entries{ reinterpret_cast<const warren_boot_memory_entry_t*>(
            bytes + header.memory_map.offset) };
        uint64_t previous_end{ 0 };
        for (uint32_t index{ 0 }; index < header.memory_map.count; ++index)
        {
            const warren_boot_memory_entry_t& entry{ memory_entries[index] };
            if (entry.page_count == 0 ||
                (entry.physical_start % header.page_size) != 0 ||
                entry.memory_kind > WARREN_BOOT_MEMORY_KIND_MAXIMUM ||
                entry.reserved != 0)
                return boot_information_error_t::invalid_memory_map;

            if (entry.source_kind == WARREN_BOOT_MEMORY_SOURCE_NONE)
            {
                if (entry.source_type != 0 || entry.source_attributes != 0)
                    return boot_information_error_t::invalid_memory_map;
            }
            else if (entry.source_kind != WARREN_BOOT_MEMORY_SOURCE_UEFI)
                return boot_information_error_t::unsupported_memory_source;

            uint64_t entry_size{ 0 };
            uint64_t entry_end{ 0 };
            if (!multiply_without_overflow(entry.page_count, header.page_size, entry_size) ||
                !add_without_overflow(entry.physical_start, entry_size, entry_end) ||
                (index != 0 && entry.physical_start < previous_end))
                return boot_information_error_t::invalid_memory_map;

            previous_end = entry_end;
        }

        if (!memory_map_covers(
                memory_entries,
                header.memory_map.count,
                object_range.begin,
                header.total_size,
                WARREN_BOOT_MEMORY_BOOT_INFORMATION) ||
            !memory_map_covers(
                memory_entries,
                header.memory_map.count,
                kernel_range.begin,
                header.kernel_physical_size,
                WARREN_BOOT_MEMORY_KERNEL_IMAGE) ||
            !memory_map_covers(
                memory_entries,
                header.memory_map.count,
                stack_range.begin,
                header.bootstrap_stack_size,
                WARREN_BOOT_MEMORY_BOOTSTRAP_STACK) ||
            (initial_image_present &&
             !memory_map_covers(
                 memory_entries,
                 header.memory_map.count,
                 initial_image_range.begin,
                 header.initial_image_size,
                 WARREN_BOOT_MEMORY_INITIAL_IMAGE)))
            return boot_information_error_t::missing_resource_coverage;

        if ((header.present_features & WARREN_BOOT_FEATURE_COMMAND_LINE) != 0 &&
            !valid_utf8(bytes + header.command_line.offset, header.command_line.count))
            return boot_information_error_t::invalid_utf8;

        if ((header.present_features & WARREN_BOOT_FEATURE_EARLY_CONSOLE) != 0)
        {
            const auto& console{ *reinterpret_cast<const warren_boot_early_console_t*>(
                bytes + header.early_console.offset) };
            if (console.kind != WARREN_BOOT_CONSOLE_PL011 ||
                (console.flags & WARREN_BOOT_CONSOLE_OUTPUT) == 0 ||
                (console.flags & ~WARREN_BOOT_CONSOLE_KNOWN_FLAGS) != 0 ||
                console.physical_address == 0 ||
                console.register_stride != 4 || console.register_width != 32 ||
                console.reserved_0 != 0 ||
                !bytes_are_zero(
                    reinterpret_cast<const uint8_t*>(console.reserved_1),
                    sizeof(console.reserved_1)))
                return boot_information_error_t::invalid_console;
        }

        if ((header.present_features & WARREN_BOOT_FEATURE_FRAMEBUFFER) != 0)
        {
            const auto& framebuffer{ *reinterpret_cast<const warren_boot_framebuffer_t*>(
                bytes + header.framebuffer.offset) };
            range_t framebuffer_range{ 0, 0, false };
            uint64_t pixel_count{ 0 };
            uint64_t minimum_size{ 0 };
            if (!make_range(
                    framebuffer.physical_address,
                    framebuffer.size,
                    framebuffer_range) ||
                framebuffer.width == 0 || framebuffer.height == 0 ||
                framebuffer.pixels_per_scan_line < framebuffer.width ||
                framebuffer.pixel_format < WARREN_BOOT_PIXEL_RGB_RESERVED_8 ||
                framebuffer.pixel_format > WARREN_BOOT_PIXEL_BIT_MASK ||
                !multiply_without_overflow(
                    framebuffer.pixels_per_scan_line,
                    framebuffer.height,
                    pixel_count) ||
                !multiply_without_overflow(pixel_count, 4, minimum_size) ||
                minimum_size > framebuffer.size ||
                !bytes_are_zero(
                    reinterpret_cast<const uint8_t*>(framebuffer.reserved),
                    sizeof(framebuffer.reserved)))
                return boot_information_error_t::invalid_framebuffer;
        }

        return boot_information_error_t::success;
    }
}
