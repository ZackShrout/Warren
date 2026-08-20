//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/TransitionPlan.h>

#include <stdint.h>

namespace {
    constexpr uint32_t k_complete_validation_failure{ 74 };
    constexpr uint32_t k_transition_planning_failure{ 77 };

    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_read_only_start[];
    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_read_only_end[];
    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_text_start[];
    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_text_end[];
    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_writable_start[];
    extern "C" [[gnu::visibility("hidden")]] const uint8_t burrow_writable_end[];

    [[nodiscard]] bool make_segment(
        const uint8_t* begin_symbol,
        const uint8_t* end_symbol,
        uint64_t load_bias,
        uint32_t permissions,
        burrow::core::transition_image_segment_t& segment) noexcept
    {
        const uint64_t begin{ reinterpret_cast<uintptr_t>(begin_symbol) };
        const uint64_t end{ reinterpret_cast<uintptr_t>(end_symbol) };
        if (end <= begin || end > UINT64_MAX - (burrow::core::k_transition_page_size - 1))
            return false;

        const uint64_t page_begin{ begin & ~(burrow::core::k_transition_page_size - 1) };
        const uint64_t page_end{
            (end + burrow::core::k_transition_page_size - 1) &
            ~(burrow::core::k_transition_page_size - 1)
        };
        if (page_begin < load_bias || page_end <= page_begin)
            return false;

        segment.physical_start = { page_begin };
        segment.image_relative_start = { page_begin - load_bias };
        segment.page_count = { (page_end - page_begin) / burrow::core::k_transition_page_size };
        segment.permissions = permissions;
        segment.reserved = 0;
        return true;
    }
} // anonymous namespace

extern "C" [[gnu::visibility("hidden")]] uint64_t burrow_aarch64_transition_stage;

extern "C" {
    [[gnu::used]] burrow::core::validated_boot_information_t burrow_aarch64_validated_boot_information{};
    [[gnu::used]] burrow::core::transition_plan_t burrow_aarch64_transition_plan{};
    [[gnu::used]] uint32_t burrow_aarch64_validation_error{};
    [[gnu::used]] uint32_t burrow_aarch64_planning_error{};

    uint32_t burrow_aarch64_prepare_transition(
        const void* object,
        uint32_t readable_size,
        uint64_t physical_address) noexcept
    {
        const warren::boot::boot_information_error_t validation_result{
            burrow::core::consume_boot_information(
                object,
                readable_size,
                physical_address,
                burrow_aarch64_validated_boot_information)
        };
        burrow_aarch64_validation_error = static_cast<uint32_t>(validation_result);
        if (validation_result != warren::boot::boot_information_error_t::success)
            return k_complete_validation_failure;

        burrow_aarch64_transition_stage = 2;

        const auto* header{ static_cast<const warren_boot_information_t*>(object) };
        burrow::core::transition_image_layout_t image_layout{};
        image_layout.segment_count = 3;
        const bool layout_valid{
            make_segment(
                burrow_read_only_start,
                burrow_read_only_end,
                header->kernel_load_bias,
                burrow::core::k_transition_permission_read,
                image_layout.segments[0]) &&
            make_segment(
                burrow_text_start,
                burrow_text_end,
                header->kernel_load_bias,
                burrow::core::k_transition_permission_read | burrow::core::k_transition_permission_execute,
                image_layout.segments[1]) &&
            make_segment(
                burrow_writable_start,
                burrow_writable_end,
                header->kernel_load_bias,
                burrow::core::k_transition_permission_read | burrow::core::k_transition_permission_write,
                image_layout.segments[2])
        };
        if (!layout_valid)
        {
            burrow_aarch64_planning_error = static_cast<uint32_t>(
                burrow::core::transition_plan_error_t::invalid_image_layout);
            return k_transition_planning_failure;
        }

        const burrow::core::transition_plan_error_t planning_result{
            burrow::core::plan_aarch64_transition(
                burrow_aarch64_validated_boot_information,
                image_layout,
                burrow_aarch64_transition_plan)
        };
        burrow_aarch64_planning_error = static_cast<uint32_t>(planning_result);
        if (planning_result != burrow::core::transition_plan_error_t::success)
            return k_transition_planning_failure;

        return 0;
    }
}
