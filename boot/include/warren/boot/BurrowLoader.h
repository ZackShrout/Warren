//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stdint.h>

namespace warren::boot {
    constexpr uint64_t k_burrow_page_size{ 4096 };
    constexpr uint64_t k_burrow_image_limit{ 0x80000000 };
    constexpr uint32_t k_burrow_load_segment_count{ 3 };
    constexpr uint32_t k_burrow_maximum_relocations{ 256 };

    using byte_source_read_t = bool (*)(void* context, uint64_t offset, uint8_t* destination,
                                        uint64_t byte_count) noexcept;

    struct byte_source_t
    {
        void* context;
        uint64_t byte_count;
        byte_source_read_t read;
    };

    enum class burrow_segment_class_t : uint32_t
    {
        read_only = 0,
        executable = 1,
        writable = 2
    };

    struct burrow_load_segment_t
    {
        uint64_t file_offset;
        uint64_t virtual_address;
        uint64_t file_size;
        uint64_t memory_size;
        burrow_segment_class_t segment_class;
    };

    struct burrow_relocation_table_t
    {
        uint64_t file_offset;
        uint64_t virtual_address;
        uint64_t byte_count;
        uint64_t entry_count;
    };

    struct burrow_load_plan_t
    {
        uint64_t source_size;
        burrow_load_segment_t segments[k_burrow_load_segment_count];
        uint64_t allocation_size;
        uint64_t entry_virtual_address;
        uint64_t dynamic_virtual_address;
        uint64_t dynamic_size;
        burrow_relocation_table_t relocations;
    };

    struct burrow_loaded_image_t
    {
        uint64_t physical_start;
        uint64_t physical_size;
        uint64_t load_bias;
        uint64_t entry_physical_address;
    };

    enum class burrow_load_error_t : uint32_t
    {
        success = 0,
        invalid_source,
        source_read_failed,
        invalid_elf_magic,
        unsupported_elf_class,
        unsupported_byte_order,
        invalid_elf_version,
        unsupported_elf_abi,
        unsupported_elf_type,
        unsupported_machine,
        invalid_machine_flags,
        invalid_elf_header_size,
        invalid_program_header_size,
        invalid_program_table,
        too_many_program_headers,
        unsupported_program_type,
        invalid_load_segment_count,
        invalid_load_permissions,
        invalid_load_size,
        invalid_load_alignment,
        invalid_load_congruence,
        invalid_load_file_range,
        invalid_load_address_range,
        invalid_load_physical_address,
        overlapping_load_segments,
        invalid_load_span,
        missing_dynamic_segment,
        duplicate_dynamic_segment,
        invalid_dynamic_segment,
        invalid_entry,
        unterminated_dynamic_table,
        duplicate_dynamic_tag,
        forbidden_dynamic_tag,
        unsupported_dynamic_tag,
        incomplete_relocation_table,
        invalid_relocation_entry_size,
        invalid_relocation_table_size,
        invalid_relocation_table_range,
        invalid_relocation_type,
        invalid_relocation_symbol,
        invalid_relocation_addend,
        invalid_relocation_target,
        duplicate_relocation_target,
        allocation_span_overflow,
        invalid_destination,
        invalid_load_bias,
        physical_extent_overflow
    };

    [[nodiscard]] burrow_load_error_t plan_burrow_image(const byte_source_t& source, burrow_load_plan_t& plan) noexcept;

    [[nodiscard]] burrow_load_error_t materialize_burrow_image(const byte_source_t& source,
                                                               const burrow_load_plan_t& plan, uint64_t physical_start,
                                                               uint8_t* destination, uint64_t destination_size,
                                                               burrow_loaded_image_t& image) noexcept;

    [[nodiscard]] const char* burrow_load_error_name(burrow_load_error_t error) noexcept;
} // namespace warren::boot
