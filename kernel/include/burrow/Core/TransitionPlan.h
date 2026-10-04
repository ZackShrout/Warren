//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootInformation.h>
#include <warren/boot/BootInformationValidation.h>

#include <stddef.h>
#include <stdint.h>

namespace burrow::core {
    constexpr uint64_t k_transition_page_size{ 4096 };
    constexpr uint64_t k_transition_arena_page_count{ 128 };
    constexpr uint64_t k_transition_stack_page_count{ 16 };
    constexpr uint64_t k_transition_table_page_count{ 111 };
    constexpr uint32_t k_transition_mapping_capacity{ 18 };
    constexpr uint32_t k_transition_image_segment_capacity{ 5 };

    constexpr uint64_t k_direct_map_virtual_bias{ UINT64_C(0xffff800000000000) };
    constexpr uint64_t k_direct_map_physical_limit{ UINT64_C(0x0000400000000000) };
    constexpr uint64_t k_mmio_virtual_start{ UINT64_C(0xffffc00000000000) };
    constexpr uint64_t k_early_stack_guard_start{ UINT64_C(0xffffd00000000000) };
    constexpr uint64_t k_early_stack_virtual_start{ UINT64_C(0xffffd00000001000) };
    constexpr uint64_t k_early_stack_virtual_top{ UINT64_C(0xffffd00000011000) };
    constexpr uint64_t k_kernel_virtual_bias{ UINT64_C(0xffffffff80000000) };
    constexpr uint64_t k_kernel_image_relative_limit{ UINT64_C(0x80000000) };
    constexpr uint64_t k_reference_pl011_physical_address{ UINT64_C(0x09000000) };
    constexpr uint64_t k_reference_pl011_virtual_address{ UINT64_C(0xffffc00009000000) };
    constexpr uint64_t k_reference_gic_distributor_physical_address{
        UINT64_C(0x08000000)
    };
    constexpr uint64_t k_reference_gic_distributor_virtual_address{
        UINT64_C(0xffffc00008000000)
    };
    constexpr uint64_t k_reference_gic_distributor_page_count{ 16 };
    constexpr uint64_t k_reference_gic_redistributor_physical_address{
        UINT64_C(0x080a0000)
    };
    constexpr uint64_t k_reference_gic_redistributor_virtual_address{
        UINT64_C(0xffffc000080a0000)
    };
    constexpr uint64_t k_reference_gic_redistributor_page_count{ 32 };

    struct physical_address_t
    {
        uint64_t value;
    };

    struct virtual_address_t
    {
        uint64_t value;
    };

    struct byte_count_t
    {
        uint64_t value;
    };

    struct page_count_t
    {
        uint64_t value;
    };

    enum class transition_memory_type_t : uint32_t
    {
        normal = 0,
        device = 1,
    };

    constexpr uint32_t k_transition_permission_read{ 0x01 };
    constexpr uint32_t k_transition_permission_write{ 0x02 };
    constexpr uint32_t k_transition_permission_execute{ 0x04 };
    constexpr uint32_t k_transition_mapping_temporary_identity{ 0x01 };

    struct validated_boot_information_t
    {
        const warren_boot_information_t* object;
        physical_address_t physical_address;
        byte_count_t byte_count;
        const warren_boot_memory_entry_t* memory_entries;
        uint32_t memory_entry_count;
        uint32_t reserved;
        physical_address_t console_physical_address;
    };

    struct transition_image_segment_t
    {
        physical_address_t physical_start;
        virtual_address_t image_relative_start;
        page_count_t page_count;
        uint32_t permissions;
        uint32_t reserved;
    };

    struct transition_image_layout_t
    {
        transition_image_segment_t segments[k_transition_image_segment_capacity];
        uint32_t segment_count;
        uint32_t reserved;
    };

    struct transition_mapping_t
    {
        physical_address_t physical_start;
        virtual_address_t virtual_start;
        page_count_t page_count;
        transition_memory_type_t memory_type;
        uint32_t permissions;
        uint32_t flags;
        uint32_t reserved;
    };

    struct transition_plan_t
    {
        transition_mapping_t mappings[k_transition_mapping_capacity];
        uint32_t mapping_count;
        uint32_t reserved;
        physical_address_t arena_physical_start;
        page_count_t arena_page_count;
        physical_address_t empty_root_physical_address;
        physical_address_t early_stack_physical_start;
        page_count_t early_stack_page_count;
        physical_address_t page_table_physical_start;
        page_count_t page_table_page_count;
        physical_address_t boot_information_physical_start;
        page_count_t boot_information_page_count;
        physical_address_t bootstrap_stack_physical_start;
        page_count_t bootstrap_stack_page_count;
        physical_address_t console_physical_address;
        virtual_address_t early_stack_virtual_start;
        virtual_address_t early_stack_virtual_top;
    };

    enum class transition_plan_error_t : uint32_t
    {
        success = 0,
        invalid_validated_view = 1,
        invalid_image_layout = 2,
        invalid_console_aperture = 3,
        invalid_memory_map = 4,
        physical_range_overflow = 5,
        virtual_range_overflow = 6,
        transition_arena_exhausted = 7,
        mapping_capacity_exceeded = 8,
        mapping_overlap = 9,
    };

    [[nodiscard]] warren::boot::boot_information_error_t consume_boot_information(
        const void* object,
        uint32_t readable_size,
        uint64_t physical_address,
        validated_boot_information_t& view) noexcept;

    [[nodiscard]] transition_plan_error_t plan_aarch64_transition(
        const validated_boot_information_t& view,
        const transition_image_layout_t& image_layout,
        transition_plan_t& plan) noexcept;
} // namespace burrow::core

static_assert(sizeof(burrow::core::physical_address_t) == 8);
static_assert(sizeof(burrow::core::virtual_address_t) == 8);
static_assert(sizeof(burrow::core::byte_count_t) == 8);
static_assert(sizeof(burrow::core::page_count_t) == 8);
static_assert(sizeof(burrow::core::transition_mapping_t) == 40);
static_assert(alignof(burrow::core::transition_mapping_t) == 8);
static_assert(offsetof(burrow::core::transition_plan_t, page_table_page_count) == 0x308);
