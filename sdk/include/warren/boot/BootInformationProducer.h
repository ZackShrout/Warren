//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootInformation.h>

#include <stdint.h>

namespace warren::boot {
    enum class uefi_memory_type_t : uint32_t
    {
        reserved = 0,
        loader_code = 1,
        loader_data = 2,
        boot_services_code = 3,
        boot_services_data = 4,
        runtime_services_code = 5,
        runtime_services_data = 6,
        conventional = 7,
        unusable = 8,
        acpi_reclaim = 9,
        acpi_nvs = 10,
        memory_mapped_io = 11,
        memory_mapped_io_port_space = 12,
        pal_code = 13,
        persistent = 14,
        unaccepted = 15,
    };

    struct boot_information_source_descriptor_t
    {
        uint64_t physical_start;
        uint64_t page_count;
        uint32_t source_type;
        uint64_t source_attributes;
    };

    struct boot_information_producer_input_t
    {
        const boot_information_source_descriptor_t* source_descriptors;
        uint32_t source_descriptor_count;

        uint64_t object_physical_start;
        uint64_t object_allocation_size;

        uint64_t kernel_physical_start;
        uint64_t kernel_physical_size;
        uint64_t kernel_load_bias;
        uint64_t kernel_entry_physical_address;

        uint64_t bootstrap_stack_physical_start;
        uint64_t bootstrap_stack_size;

        const warren_boot_early_console_t* early_console;
    };

    struct boot_information_producer_output_t
    {
        uint32_t total_size;
        uint32_t memory_entry_count;
    };

    enum class boot_information_producer_error_t : uint32_t
    {
        success = 0,
        null_output = 1,
        unaligned_output = 2,
        output_too_small = 3,
        null_source_map = 4,
        empty_source_map = 5,
        invalid_source_descriptor = 6,
        unsupported_source_type = 7,
        invalid_object_range = 8,
        invalid_kernel_range = 9,
        invalid_entry_address = 10,
        invalid_stack_range = 11,
        overlapping_resources = 12,
        resource_not_covered = 13,
        invalid_console = 14,
        null_work_storage = 15,
        insufficient_work_capacity = 16,
        object_size_overflow = 17,
    };

    [[nodiscard]] boot_information_producer_error_t produce_boot_information(
        const boot_information_producer_input_t& input,
        void* output,
        uint32_t output_capacity,
        warren_boot_memory_entry_t* work_entries,
        uint32_t work_entry_capacity,
        boot_information_producer_output_t& result) noexcept;
} // namespace warren::boot
