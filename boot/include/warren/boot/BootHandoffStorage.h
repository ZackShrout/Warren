//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stdint.h>

namespace warren::boot {
    constexpr uint64_t k_bootstrap_stack_page_count{ 16 };
    constexpr uint64_t k_memory_map_growth_descriptor_count{ 32 };
    constexpr uint64_t k_handoff_live_resource_count{ 3 };

    struct boot_handoff_storage_plan_t
    {
        uint64_t memory_map_page_count;
        uint64_t memory_map_capacity;
        uint64_t memory_descriptor_size;
        uint32_t source_descriptor_capacity;

        uint64_t work_page_count;
        uint64_t work_capacity;
        uint32_t work_entry_capacity;

        uint64_t object_page_count;
        uint64_t object_allocation_size;
        uint32_t object_required_capacity;
    };

    struct boot_handoff_page_allocation_t
    {
        uint64_t physical_start;
        uint8_t* writable_start;
    };

    using boot_handoff_allocate_pages_fn = uint64_t (*)(
        void* context,
        uint64_t page_count,
        boot_handoff_page_allocation_t& allocation) noexcept;

    using boot_handoff_free_pages_fn = uint64_t (*)(
        void* context,
        uint64_t physical_start,
        uint64_t page_count) noexcept;

    struct boot_handoff_page_allocator_t
    {
        void* context;
        boot_handoff_allocate_pages_fn allocate_pages;
        boot_handoff_free_pages_fn free_pages;
    };

    struct boot_handoff_allocation_t
    {
        uint64_t physical_start;
        uint64_t page_count;
        uint8_t* writable_start;
    };

    struct boot_handoff_storage_t
    {
        boot_handoff_storage_plan_t plan;
        boot_handoff_allocation_t bootstrap_stack;
        boot_handoff_allocation_t memory_map;
        boot_handoff_allocation_t work_entries;
        boot_handoff_allocation_t object;
    };

    enum class boot_handoff_storage_error_t : uint32_t
    {
        success = 0,
        invalid_memory_map_size = 1,
        invalid_descriptor_size = 2,
        arithmetic_overflow = 3,
        capacity_overflow = 4,
        invalid_plan = 5,
        invalid_allocator = 6,
        allocation_failed = 7,
        invalid_allocation = 8,
        overlapping_allocations = 9,
        cleanup_failed = 10,
        storage_not_empty = 11,
    };

    [[nodiscard]] boot_handoff_storage_error_t plan_boot_handoff_storage(
        uint64_t estimated_memory_map_size,
        uint64_t memory_descriptor_size,
        bool early_console_present,
        boot_handoff_storage_plan_t& plan) noexcept;

    [[nodiscard]] boot_handoff_storage_error_t allocate_boot_handoff_storage(
        const boot_handoff_storage_plan_t& plan,
        const boot_handoff_page_allocator_t& allocator,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept;

    // Cleanup is valid only before the first ExitBootServices() attempt.
    [[nodiscard]] boot_handoff_storage_error_t release_boot_handoff_storage(
        const boot_handoff_page_allocator_t& allocator,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept;
} // namespace warren::boot
