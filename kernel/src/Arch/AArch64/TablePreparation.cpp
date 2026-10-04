//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/PageTables.h>

#include <stdint.h>

extern "C" [[gnu::visibility("hidden")]] uint64_t burrow_aarch64_transition_stage;
extern "C" [[gnu::visibility("hidden")]] burrow::core::transition_plan_t
    burrow_aarch64_transition_plan;
extern "C" [[gnu::visibility("hidden")]] burrow::core::validated_boot_information_t
    burrow_aarch64_validated_boot_information;

namespace {
    constexpr uint32_t k_unsupported_architecture_failure{ 75 };
    constexpr uint32_t k_table_construction_failure{ 78 };
    constexpr uint32_t k_activation_failure{ 79 };

    void zero_pages(uint64_t* writable, uint64_t page_count) noexcept
    {
        const uint64_t entry_count{
            page_count * burrow::arch::aarch64::k_page_table_entry_count
        };
        for (uint64_t index{ 0 }; index < entry_count; ++index)
            writable[index] = 0;
    }

    [[nodiscard]] bool unsupported(
        burrow::arch::aarch64::page_table_error_t error) noexcept
    {
        return error == burrow::arch::aarch64::page_table_error_t::unsupported_granule ||
            error == burrow::arch::aarch64::page_table_error_t::unsupported_physical_range;
    }

    [[nodiscard]] burrow::arch::aarch64::page_table_storage_t table_storage() noexcept
    {
        return {
            reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(
                burrow_aarch64_transition_plan.empty_root_physical_address.value)),
            burrow_aarch64_transition_plan.empty_root_physical_address,
            reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(
                burrow_aarch64_transition_plan.page_table_physical_start.value)),
            burrow_aarch64_transition_plan.page_table_physical_start,
            burrow_aarch64_transition_plan.page_table_page_count,
        };
    }
} // anonymous namespace

extern "C" {
    [[gnu::used]] burrow::arch::aarch64::translation_configuration_t
        burrow_aarch64_translation_configuration{};
    [[gnu::used]] uint32_t burrow_aarch64_table_build_error{};
    [[gnu::used]] uint32_t burrow_aarch64_table_audit_error{};
    [[gnu::used]] uint32_t burrow_aarch64_activation_preflight_error{};
    [[gnu::used]] uint64_t burrow_aarch64_id_aa64mmfr0_el1{};
    [[gnu::used]] uint64_t burrow_aarch64_empty_root_physical_address{};

    uint32_t burrow_aarch64_build_transition_tables(uint64_t id_aa64mmfr0_el1) noexcept
    {
        burrow_aarch64_transition_stage = 4;
        burrow_aarch64_id_aa64mmfr0_el1 = id_aa64mmfr0_el1;

        constexpr uint64_t table_offset{ 17 * burrow::core::k_transition_page_size };
        if (burrow_aarch64_transition_plan.arena_physical_start.value == 0 ||
            burrow_aarch64_transition_plan.arena_physical_start.value >
                UINT64_MAX - table_offset ||
            burrow_aarch64_transition_plan.empty_root_physical_address.value !=
                burrow_aarch64_transition_plan.arena_physical_start.value ||
            burrow_aarch64_transition_plan.page_table_physical_start.value !=
                burrow_aarch64_transition_plan.arena_physical_start.value + table_offset ||
            burrow_aarch64_transition_plan.empty_root_physical_address.value == 0 ||
            burrow_aarch64_transition_plan.page_table_physical_start.value == 0 ||
            (burrow_aarch64_transition_plan.empty_root_physical_address.value &
             (burrow::core::k_transition_page_size - 1)) != 0 ||
            (burrow_aarch64_transition_plan.page_table_physical_start.value &
             (burrow::core::k_transition_page_size - 1)) != 0 ||
            burrow_aarch64_transition_plan.page_table_page_count.value !=
                burrow::core::k_transition_table_page_count)
        {
            burrow_aarch64_table_build_error = static_cast<uint32_t>(
                burrow::arch::aarch64::page_table_error_t::invalid_plan);
            return k_table_construction_failure;
        }

        auto* empty_root{ reinterpret_cast<uint64_t*>(
            static_cast<uintptr_t>(
                burrow_aarch64_transition_plan.empty_root_physical_address.value)) };
        auto* table_pages{ reinterpret_cast<uint64_t*>(
            static_cast<uintptr_t>(
                burrow_aarch64_transition_plan.page_table_physical_start.value)) };
        zero_pages(empty_root, 1);
        zero_pages(table_pages, burrow::core::k_transition_table_page_count);

        const burrow::arch::aarch64::page_table_storage_t storage{
            empty_root,
            burrow_aarch64_transition_plan.empty_root_physical_address,
            table_pages,
            burrow_aarch64_transition_plan.page_table_physical_start,
            burrow_aarch64_transition_plan.page_table_page_count,
        };
        const burrow::arch::aarch64::page_table_error_t build_result{
            burrow::arch::aarch64::build_page_tables(
                id_aa64mmfr0_el1,
                burrow_aarch64_transition_plan,
                storage,
                burrow_aarch64_translation_configuration)
        };
        burrow_aarch64_table_build_error = static_cast<uint32_t>(build_result);
        if (build_result != burrow::arch::aarch64::page_table_error_t::success)
            return unsupported(build_result) ? k_unsupported_architecture_failure :
                                               k_table_construction_failure;

        const burrow::arch::aarch64::page_table_error_t audit_result{
            burrow::arch::aarch64::audit_page_tables(
                id_aa64mmfr0_el1,
                burrow_aarch64_transition_plan,
                storage,
                burrow_aarch64_translation_configuration)
        };
        burrow_aarch64_table_audit_error = static_cast<uint32_t>(audit_result);
        if (audit_result != burrow::arch::aarch64::page_table_error_t::success)
            return unsupported(audit_result) ? k_unsupported_architecture_failure :
                                               k_table_construction_failure;

        burrow_aarch64_empty_root_physical_address =
            burrow_aarch64_transition_plan.empty_root_physical_address.value;
        burrow_aarch64_transition_stage = 5;
        return 0;
    }

    uint32_t burrow_aarch64_preflight_activation(
        uint64_t current_program_counter,
        uint64_t current_stack_pointer,
        uint64_t target_program_counter_physical,
        uint64_t target_program_counter_virtual,
        uint64_t stable_vectors_physical,
        uint64_t stable_vectors_virtual) noexcept
    {
        const uint64_t boot_physical{
            burrow_aarch64_validated_boot_information.physical_address.value
        };
        if (boot_physical > UINT64_MAX - burrow::core::k_direct_map_virtual_bias ||
            burrow_aarch64_transition_plan.arena_physical_start.value >
                UINT64_MAX - burrow::core::k_direct_map_virtual_bias)
        {
            burrow_aarch64_activation_preflight_error = static_cast<uint32_t>(
                burrow::arch::aarch64::activation_preflight_error_t::invalid_runtime_state);
            return k_activation_failure;
        }

        const burrow::arch::aarch64::activation_preflight_t preflight{
            { current_program_counter },
            { current_stack_pointer },
            { boot_physical },
            { target_program_counter_physical },
            { target_program_counter_virtual },
            { stable_vectors_physical },
            { stable_vectors_virtual },
            burrow_aarch64_transition_plan.early_stack_virtual_top,
            { burrow::core::k_direct_map_virtual_bias + boot_physical },
            { burrow::core::k_direct_map_virtual_bias +
                burrow_aarch64_transition_plan.arena_physical_start.value },
            { burrow::core::k_reference_pl011_virtual_address },
            { burrow::core::k_reference_gic_distributor_virtual_address },
            { burrow::core::k_reference_gic_redistributor_virtual_address },
        };
        const burrow::arch::aarch64::activation_preflight_error_t result{
            burrow::arch::aarch64::preflight_activation(
                burrow_aarch64_id_aa64mmfr0_el1,
                burrow_aarch64_transition_plan,
                table_storage(),
                burrow_aarch64_translation_configuration,
                preflight)
        };
        burrow_aarch64_activation_preflight_error = static_cast<uint32_t>(result);
        return result == burrow::arch::aarch64::activation_preflight_error_t::success ?
            0 : k_activation_failure;
    }
}
