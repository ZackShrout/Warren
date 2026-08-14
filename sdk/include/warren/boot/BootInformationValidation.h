//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stdint.h>

namespace warren::boot {
    enum class boot_information_error_t : uint32_t
    {
        success = 0,
        null_object = 1,
        unaligned_object = 2,
        truncated_fixed_header = 3,
        invalid_magic = 4,
        unsupported_major = 5,
        invalid_header_size = 6,
        invalid_total_size = 7,
        invalid_page_size = 8,
        self_address_mismatch = 9,
        nonzero_reserved = 10,
        invalid_feature_masks = 11,
        unsupported_required_feature = 12,
        missing_memory_map = 13,
        invalid_kernel_range = 14,
        invalid_entry_address = 15,
        invalid_stack_range = 16,
        overlapping_physical_resources = 17,
        invalid_optional_resource = 18,
        invalid_section_descriptor = 19,
        section_out_of_bounds = 20,
        overlapping_sections = 21,
        nonzero_padding = 22,
        invalid_memory_map = 23,
        unsupported_memory_source = 24,
        missing_resource_coverage = 25,
        invalid_utf8 = 26,
        invalid_console = 27,
        invalid_framebuffer = 28,
    };

    [[nodiscard]] boot_information_error_t validate_boot_information(const void* object, uint32_t readable_size,
                                                                     uint64_t physical_address) noexcept;
} // namespace warren::boot
