//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Core/TransitionPlan.h>
#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::core {
    constexpr uint32_t k_physical_memory_extent_capacity{ 64 };
    constexpr uint64_t k_physical_memory_state_magic{
        UINT64_C(0x425552524f574d31)
    };

    struct physical_memory_extent_t
    {
        physical_address_t physical_start;
        page_count_t page_count;
        page_count_t consumed_page_count;
        uint32_t source_kind;
        uint32_t source_type;
        uint64_t source_attributes;
    };

    struct physical_memory_state_t
    {
        physical_memory_extent_t extents[k_physical_memory_extent_capacity];
        uint32_t extent_count;
        uint32_t reserved;
        page_count_t total_page_count;
        page_count_t consumed_page_count;
        page_count_t allocated_page_count;
        page_count_t excluded_page_count;
        page_count_t unmanaged_page_count;
        physical_address_t transition_arena_physical_start;
        page_count_t transition_arena_page_count;
        uint64_t magic;
    };

    struct boot_allocation_t
    {
        physical_address_t physical_start;
        page_count_t page_count;
        page_count_t alignment_page_count;
        page_count_t consumed_page_count;
    };

    enum class physical_memory_error_t : uint32_t
    {
        success = 0,
        invalid_validated_view = 1,
        invalid_transition_arena = 2,
        invalid_memory_map = 3,
        extent_capacity_exceeded = 4,
        no_usable_memory = 5,
        invalid_state = 6,
        invalid_request = 7,
        exhausted = 8,
        output_failure = 9,
    };

    [[nodiscard]] physical_memory_error_t initialize_physical_memory(
        const validated_boot_information_t& view,
        physical_address_t transition_arena_physical_start,
        page_count_t transition_arena_page_count,
        physical_memory_state_t& state) noexcept;

    [[nodiscard]] physical_memory_error_t allocate_boot_pages(
        physical_memory_state_t& state,
        page_count_t page_count,
        page_count_t alignment_page_count,
        boot_allocation_t& allocation) noexcept;

    [[nodiscard]] physical_memory_error_t report_physical_memory(
        const physical_memory_state_t& state,
        const boot_allocation_t& allocation,
        const drivers::console_writer_t& console) noexcept;
} // namespace burrow::core

static_assert(sizeof(burrow::core::physical_memory_extent_t) == 40);
static_assert(alignof(burrow::core::physical_memory_extent_t) == 8);
static_assert(sizeof(burrow::core::boot_allocation_t) == 32);
