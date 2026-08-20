//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Core/TransitionPlan.h>

#include <stdint.h>

namespace burrow::arch::aarch64 {
    constexpr uint64_t k_page_table_entry_count{ 512 };
    constexpr uint64_t k_mair_el1{ UINT64_C(0x00000000000000ff) };
    constexpr uint64_t k_tcr_el1_base{ UINT64_C(0x00000000b5103510) };
    constexpr uint64_t k_sctlr_el1_owned{ UINT64_C(0x0000000030d8181d) };

    struct page_table_storage_t
    {
        uint64_t* empty_root_writable;
        core::physical_address_t empty_root_physical_address;
        uint64_t* table_pages_writable;
        core::physical_address_t table_pages_physical_address;
        core::page_count_t table_page_capacity;
    };

    struct translation_configuration_t
    {
        core::physical_address_t ttbr0_root_physical_address;
        core::physical_address_t ttbr1_root_physical_address;
        core::page_count_t table_page_count;
        uint64_t mair_el1;
        uint64_t tcr_el1;
        uint64_t sctlr_el1;
        uint32_t physical_address_bits;
        uint32_t ips_encoding;
    };

    enum class page_table_error_t : uint32_t
    {
        success = 0,
        invalid_plan = 1,
        invalid_storage = 2,
        storage_not_zero = 3,
        unsupported_granule = 4,
        unsupported_physical_range = 5,
        noncanonical_virtual_address = 6,
        physical_range_overflow = 7,
        virtual_range_overflow = 8,
        invalid_mapping_policy = 9,
        mapping_conflict = 10,
        table_capacity_exceeded = 11,
        invalid_table_descriptor = 12,
        invalid_page_descriptor = 13,
        unreachable_table = 14,
        unexpected_leaf = 15,
        missing_mapping = 16,
        mapped_stack_guard = 17,
        writable_executable_alias = 18,
    };

    [[nodiscard]] page_table_error_t build_page_tables(
        uint64_t id_aa64mmfr0_el1,
        const core::transition_plan_t& plan,
        const page_table_storage_t& storage,
        translation_configuration_t& configuration) noexcept;

    [[nodiscard]] page_table_error_t audit_page_tables(
        uint64_t id_aa64mmfr0_el1,
        const core::transition_plan_t& plan,
        const page_table_storage_t& storage,
        const translation_configuration_t& configuration) noexcept;
} // namespace burrow::arch::aarch64

static_assert(sizeof(burrow::arch::aarch64::translation_configuration_t) == 56);
static_assert(alignof(burrow::arch::aarch64::translation_configuration_t) == 8);
