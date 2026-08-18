//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformationProducer.h>

#include <stddef.h>
#include <stdint.h>

namespace warren::boot {
    namespace {
        constexpr uint64_t k_page_size{ WARREN_BOOT_INFORMATION_PAGE_SIZE };
        constexpr uint32_t k_resource_count{ 3 };

        struct range_t
        {
            uint64_t begin;
            uint64_t end;
            uint32_t memory_kind;
        };

        [[nodiscard]] bool add_without_overflow(uint64_t left, uint64_t right, uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(uint64_t left, uint64_t right, uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left) return false;

            result = left * right;
            return true;
        }

        [[nodiscard]] bool make_page_range(uint64_t physical_start, uint64_t byte_count, uint32_t memory_kind,
                                           range_t& range) noexcept
        {
            if (physical_start == 0 || byte_count == 0 || (physical_start % k_page_size) != 0 ||
                (byte_count % k_page_size) != 0)
                return false;

            uint64_t end{ 0 };

            if (!add_without_overflow(physical_start, byte_count, end))
                return false;

            range = { physical_start, end, memory_kind };
            return true;
        }

        [[nodiscard]] bool ranges_overlap(const range_t& left, const range_t& right) noexcept
        {
            return left.begin < right.end && right.begin < left.end;
        }

        [[nodiscard]] boot_information_producer_error_t map_uefi_memory_type(uint32_t source_type,
                                                                             uint32_t& memory_kind) noexcept
        {
            switch (static_cast<uefi_memory_type_t>(source_type))
            {
                case uefi_memory_type_t::reserved:
                case uefi_memory_type_t::pal_code:
                case uefi_memory_type_t::unaccepted:
                    memory_kind = WARREN_BOOT_MEMORY_RESERVED;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::loader_code:
                case uefi_memory_type_t::loader_data:
                    memory_kind = WARREN_BOOT_MEMORY_LOADER_RECLAIMABLE;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::boot_services_code:
                case uefi_memory_type_t::boot_services_data:
                    memory_kind = WARREN_BOOT_MEMORY_FIRMWARE_RECLAIMABLE;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::runtime_services_code:
                case uefi_memory_type_t::runtime_services_data:
                    memory_kind = WARREN_BOOT_MEMORY_FIRMWARE_RUNTIME;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::conventional:
                    memory_kind = WARREN_BOOT_MEMORY_USABLE;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::unusable:
                    memory_kind = WARREN_BOOT_MEMORY_UNUSABLE;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::acpi_reclaim:
                    memory_kind = WARREN_BOOT_MEMORY_ACPI_RECLAIMABLE;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::acpi_nvs:
                    memory_kind = WARREN_BOOT_MEMORY_ACPI_NVS;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::memory_mapped_io:
                case uefi_memory_type_t::memory_mapped_io_port_space:
                    memory_kind = WARREN_BOOT_MEMORY_MMIO;
                    return boot_information_producer_error_t::success;
                case uefi_memory_type_t::persistent:
                    memory_kind = WARREN_BOOT_MEMORY_PERSISTENT;
                    return boot_information_producer_error_t::success;
            }

            return boot_information_producer_error_t::unsupported_source_type;
        }

        [[nodiscard]] boot_information_producer_error_t validate_source_map(
            const boot_information_source_descriptor_t* descriptors,
            uint32_t descriptor_count) noexcept
        {
            uint64_t previous_end{ 0 };

            for (uint32_t index{ 0 }; index < descriptor_count; ++index)
            {
                const boot_information_source_descriptor_t& descriptor{ descriptors[index] };
                uint32_t ignored_kind{ 0 };

                const boot_information_producer_error_t type_result{
                    map_uefi_memory_type(descriptor.source_type, ignored_kind)
                };

                if (type_result != boot_information_producer_error_t::success)
                    return type_result;

                uint64_t byte_count{ 0 };
                uint64_t end{ 0 };

                if (descriptor.page_count == 0 || (descriptor.physical_start % k_page_size) != 0 ||
                    !multiply_without_overflow(descriptor.page_count, k_page_size, byte_count) ||
                    !add_without_overflow(descriptor.physical_start, byte_count, end) ||
                    (index != 0 && descriptor.physical_start < previous_end))
                    return boot_information_producer_error_t::invalid_source_descriptor;

                previous_end = end;
            }

            return boot_information_producer_error_t::success;
        }

        void sort_ranges(range_t* ranges, uint32_t count) noexcept
        {
            for (uint32_t index{ 1 }; index < count; ++index)
            {
                const range_t value{ ranges[index] };
                uint32_t insertion{ index };

                while (insertion > 0 && ranges[insertion - 1].begin > value.begin)
                {
                    ranges[insertion] = ranges[insertion - 1];
                    --insertion;
                }

                ranges[insertion] = value;
            }
        }

        [[nodiscard]] bool range_is_covered(const range_t& resource,
                                            const boot_information_source_descriptor_t* descriptors,
                                            uint32_t descriptor_count) noexcept
        {
            uint64_t cursor{ resource.begin };

            for (uint32_t index{ 0 }; index < descriptor_count; ++index)
            {
                const boot_information_source_descriptor_t& descriptor{ descriptors[index] };
                uint64_t descriptor_size{ 0 };
                uint64_t descriptor_end{ 0 };
                static_cast<void>(multiply_without_overflow(descriptor.page_count, k_page_size, descriptor_size));
                static_cast<void>(add_without_overflow(descriptor.physical_start, descriptor_size, descriptor_end));

                if (descriptor_end <= cursor) continue;
                if (descriptor.physical_start > cursor) return false;

                cursor = descriptor_end < resource.end ? descriptor_end : resource.end;

                if (cursor == resource.end) return true;
            }

            return false;
        }

        [[nodiscard]] bool console_is_valid(const warren_boot_early_console_t& console) noexcept
        {
            if (console.kind != WARREN_BOOT_CONSOLE_PL011 ||
                (console.flags & WARREN_BOOT_CONSOLE_OUTPUT) == 0 ||
                (console.flags & ~WARREN_BOOT_CONSOLE_KNOWN_FLAGS) != 0 || console.physical_address == 0 ||
                console.register_stride != 4 || console.register_width != 32 || console.reserved_0 != 0)
                return false;

            for (uint32_t index{ 0 }; index < 3; ++index)
            {
                if (console.reserved_1[index] != 0) return false;
            }

            return true;
        }

        [[nodiscard]] boot_information_producer_error_t append_entry(
            warren_boot_memory_entry_t* entries,
            uint32_t capacity,
            uint32_t& count,
            uint64_t physical_start,
            uint64_t page_count,
            uint32_t memory_kind,
            uint32_t source_type,
            uint64_t source_attributes) noexcept
        {
            if (count != 0)
            {
                warren_boot_memory_entry_t& previous{ entries[count - 1] };
                uint64_t previous_size{ 0 };
                uint64_t previous_end{ 0 };
                static_cast<void>(multiply_without_overflow(previous.page_count, k_page_size, previous_size));
                static_cast<void>(add_without_overflow(previous.physical_start, previous_size, previous_end));

                if (previous_end == physical_start && previous.memory_kind == memory_kind &&
                    previous.source_kind == WARREN_BOOT_MEMORY_SOURCE_UEFI && previous.source_type == source_type &&
                    previous.source_attributes == source_attributes && page_count <= UINT64_MAX - previous.page_count)
                {
                    previous.page_count += page_count;
                    return boot_information_producer_error_t::success;
                }
            }

            if (count >= capacity)
                return boot_information_producer_error_t::insufficient_work_capacity;

            warren_boot_memory_entry_t& entry{ entries[count++] };
            entry.physical_start = physical_start;
            entry.page_count = page_count;
            entry.memory_kind = memory_kind;
            entry.source_kind = WARREN_BOOT_MEMORY_SOURCE_UEFI;
            entry.source_type = source_type;
            entry.reserved = 0;
            entry.source_attributes = source_attributes;
            return boot_information_producer_error_t::success;
        }

        [[nodiscard]] boot_information_producer_error_t build_normalized_map(
            const boot_information_producer_input_t& input,
            const range_t* resources,
            warren_boot_memory_entry_t* work_entries,
            uint32_t work_entry_capacity,
            uint32_t& work_entry_count) noexcept
        {
            work_entry_count = 0;

            for (uint32_t source_index{ 0 }; source_index < input.source_descriptor_count; ++source_index)
            {
                const boot_information_source_descriptor_t& source{ input.source_descriptors[source_index] };
                uint64_t source_size{ 0 };
                uint64_t source_end{ 0 };
                static_cast<void>(multiply_without_overflow(source.page_count, k_page_size, source_size));
                static_cast<void>(add_without_overflow(source.physical_start, source_size, source_end));

                uint32_t default_kind{ 0 };
                static_cast<void>(map_uefi_memory_type(source.source_type, default_kind));
                uint64_t cursor{ source.physical_start };

                while (cursor < source_end)
                {
                    uint64_t segment_end{ source_end };
                    uint32_t memory_kind{ default_kind };

                    for (uint32_t resource_index{ 0 }; resource_index < k_resource_count; ++resource_index)
                    {
                        const range_t& resource{ resources[resource_index] };

                        if (cursor >= resource.begin && cursor < resource.end)
                        {
                            memory_kind = resource.memory_kind;
                            if (resource.end < segment_end) segment_end = resource.end;
                            break;
                        }

                        if (resource.begin > cursor)
                        {
                            if (resource.begin < segment_end) segment_end = resource.begin;
                            break;
                        }
                    }

                    const uint64_t page_count{ (segment_end - cursor) / k_page_size };
                    const boot_information_producer_error_t append_result{
                        append_entry(work_entries, work_entry_capacity, work_entry_count, cursor, page_count,
                                     memory_kind, source.source_type, source.source_attributes)
                    };

                    if (append_result != boot_information_producer_error_t::success)
                        return append_result;

                    cursor = segment_end;
                }
            }

            return boot_information_producer_error_t::success;
        }

        void clear_bytes(void* destination, uint32_t byte_count) noexcept
        {
            auto* bytes{ static_cast<uint8_t*>(destination) };

            for (uint32_t index{ 0 }; index < byte_count; ++index)
                bytes[index] = 0;
        }

        void copy_memory_entry(warren_boot_memory_entry_t& destination,
                               const warren_boot_memory_entry_t& source) noexcept
        {
            destination.physical_start = source.physical_start;
            destination.page_count = source.page_count;
            destination.memory_kind = source.memory_kind;
            destination.source_kind = source.source_kind;
            destination.source_type = source.source_type;
            destination.reserved = source.reserved;
            destination.source_attributes = source.source_attributes;
        }

        void copy_console(warren_boot_early_console_t& destination,
                          const warren_boot_early_console_t& source) noexcept
        {
            destination.kind = source.kind;
            destination.flags = source.flags;
            destination.physical_address = source.physical_address;
            destination.register_stride = source.register_stride;
            destination.register_width = source.register_width;
            destination.input_clock_hz = source.input_clock_hz;
            destination.baud_rate = source.baud_rate;
            destination.reserved_0 = source.reserved_0;

            for (uint32_t index{ 0 }; index < 3; ++index)
                destination.reserved_1[index] = source.reserved_1[index];
        }
    } // anonymous namespace

    boot_information_producer_error_t produce_boot_information(
        const boot_information_producer_input_t& input,
        void* output,
        uint32_t output_capacity,
        warren_boot_memory_entry_t* work_entries,
        uint32_t work_entry_capacity,
        boot_information_producer_output_t& result) noexcept
    {
        result = { 0, 0 };

        if (output == nullptr)
            return boot_information_producer_error_t::null_output;

        if ((reinterpret_cast<uintptr_t>(output) & 7U) != 0)
            return boot_information_producer_error_t::unaligned_output;

        if (output_capacity < WARREN_BOOT_INFORMATION_MAGIC_SIZE)
            return boot_information_producer_error_t::output_too_small;

        clear_bytes(output, WARREN_BOOT_INFORMATION_MAGIC_SIZE);

        if (input.source_descriptors == nullptr)
            return boot_information_producer_error_t::null_source_map;

        if (input.source_descriptor_count == 0)
            return boot_information_producer_error_t::empty_source_map;

        const boot_information_producer_error_t source_result{
            validate_source_map(input.source_descriptors, input.source_descriptor_count)
        };

        if (source_result != boot_information_producer_error_t::success)
            return source_result;

        range_t resources[k_resource_count]{ };

        if (!make_page_range(input.object_physical_start, input.object_allocation_size,
                             WARREN_BOOT_MEMORY_BOOT_INFORMATION, resources[0]))
            return boot_information_producer_error_t::invalid_object_range;

        if (!make_page_range(input.kernel_physical_start, input.kernel_physical_size,
                             WARREN_BOOT_MEMORY_KERNEL_IMAGE, resources[1]) ||
            input.kernel_load_bias > input.kernel_physical_start)
            return boot_information_producer_error_t::invalid_kernel_range;

        if (input.kernel_entry_physical_address < resources[1].begin ||
            input.kernel_entry_physical_address >= resources[1].end)
            return boot_information_producer_error_t::invalid_entry_address;

        if (!make_page_range(input.bootstrap_stack_physical_start, input.bootstrap_stack_size,
                             WARREN_BOOT_MEMORY_BOOTSTRAP_STACK, resources[2]) ||
            (resources[2].end & 15U) != 0)
            return boot_information_producer_error_t::invalid_stack_range;

        sort_ranges(resources, k_resource_count);

        for (uint32_t left{ 0 }; left < k_resource_count; ++left)
        {
            for (uint32_t right{ left + 1 }; right < k_resource_count; ++right)
            {
                if (ranges_overlap(resources[left], resources[right]))
                    return boot_information_producer_error_t::overlapping_resources;
            }

            if (!range_is_covered(resources[left], input.source_descriptors, input.source_descriptor_count))
                return boot_information_producer_error_t::resource_not_covered;
        }

        if (input.early_console != nullptr && !console_is_valid(*input.early_console))
            return boot_information_producer_error_t::invalid_console;

        if (work_entries == nullptr)
            return boot_information_producer_error_t::null_work_storage;

        uint32_t memory_entry_count{ 0 };
        
        const boot_information_producer_error_t map_result{
            build_normalized_map(input, resources, work_entries, work_entry_capacity, memory_entry_count)
        };

        if (map_result != boot_information_producer_error_t::success)
            return map_result;

        uint64_t memory_map_size{ 0 };

        if (!multiply_without_overflow(memory_entry_count, sizeof(warren_boot_memory_entry_t), memory_map_size))
            return boot_information_producer_error_t::object_size_overflow;

        uint64_t total_size{ WARREN_BOOT_INFORMATION_HEADER_SIZE };

        if (!add_without_overflow(total_size, memory_map_size, total_size) ||
            (input.early_console != nullptr &&
             !add_without_overflow(total_size, sizeof(warren_boot_early_console_t), total_size)) ||
            total_size > UINT32_MAX)
            return boot_information_producer_error_t::object_size_overflow;

        if (total_size > output_capacity || total_size > input.object_allocation_size)
            return boot_information_producer_error_t::output_too_small;

        const uint32_t exact_size{ static_cast<uint32_t>(total_size) };
        clear_bytes(output, exact_size);

        auto& header{ *static_cast<warren_boot_information_t*>(output) };
        constexpr uint8_t magic[WARREN_BOOT_INFORMATION_MAGIC_SIZE]{ 'W', 'A', 'R', 'R', 'E', 'N', 'B', 'I' };

        for (uint32_t index{ 0 }; index < WARREN_BOOT_INFORMATION_MAGIC_SIZE; ++index)
            header.magic[index] = magic[index];

        header.major = WARREN_BOOT_INFORMATION_MAJOR;
        header.minor = WARREN_BOOT_INFORMATION_MINOR;
        header.header_size = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        header.total_size = exact_size;
        header.page_size = WARREN_BOOT_INFORMATION_PAGE_SIZE;
        header.present_features = WARREN_BOOT_FEATURE_MEMORY_MAP;
        header.required_features = WARREN_BOOT_FEATURE_MEMORY_MAP;
        header.self_physical_address = input.object_physical_start;
        header.kernel_physical_start = input.kernel_physical_start;
        header.kernel_physical_size = input.kernel_physical_size;
        header.kernel_load_bias = input.kernel_load_bias;
        header.kernel_entry_physical_address = input.kernel_entry_physical_address;
        header.bootstrap_stack_physical_start = input.bootstrap_stack_physical_start;
        header.bootstrap_stack_size = input.bootstrap_stack_size;
        header.memory_map.offset = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        header.memory_map.count = memory_entry_count;
        header.memory_map.stride = sizeof(warren_boot_memory_entry_t);

        uint8_t* bytes{ static_cast<uint8_t*>(output) };

        warren_boot_memory_entry_t* output_entries{
            reinterpret_cast<warren_boot_memory_entry_t*>(bytes + header.memory_map.offset)
        };

        for (uint32_t index{ 0 }; index < memory_entry_count; ++index)
            copy_memory_entry(output_entries[index], work_entries[index]);

        if (input.early_console != nullptr)
        {
            header.present_features |= WARREN_BOOT_FEATURE_EARLY_CONSOLE;
            header.early_console.offset = static_cast<uint32_t>(
                WARREN_BOOT_INFORMATION_HEADER_SIZE + memory_map_size);
            header.early_console.count = 1;
            header.early_console.stride = sizeof(warren_boot_early_console_t);

            auto& console{
                *reinterpret_cast<warren_boot_early_console_t*>(bytes + header.early_console.offset)
            };
            copy_console(console, *input.early_console);
        }

        result = { exact_size, memory_entry_count };

        return boot_information_producer_error_t::success;
    }
} // namespace warren::boot
