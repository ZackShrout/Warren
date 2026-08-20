//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/TransitionPlan.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using burrow::core::transition_image_layout_t;
    using burrow::core::transition_image_segment_t;
    using burrow::core::transition_plan_error_t;
    using burrow::core::transition_plan_t;
    using burrow::core::validated_boot_information_t;
    using warren::boot::boot_information_error_t;

    constexpr uint64_t k_object_physical{ 0x00100000 };
    constexpr uint64_t k_kernel_physical{ 0x00200000 };
    constexpr uint64_t k_stack_physical{ 0x00300000 };
    constexpr uint64_t k_usable_physical{ 0x00400000 };
    constexpr uint32_t k_memory_map_offset{ 0x100 };
    constexpr uint32_t k_console_offset{ 0x380 };
    constexpr uint32_t k_total_size{ 0x3c0 };

    struct fixture_t
    {
        alignas(8) uint8_t bytes[4096]{};
    };

    [[nodiscard]] warren_boot_information_t& header(fixture_t& fixture) noexcept
    {
        return *reinterpret_cast<warren_boot_information_t*>(fixture.bytes);
    }

    [[nodiscard]] warren_boot_memory_entry_t* entries(fixture_t& fixture) noexcept
    {
        return reinterpret_cast<warren_boot_memory_entry_t*>(fixture.bytes + k_memory_map_offset);
    }

    void set_entry(warren_boot_memory_entry_t& entry, uint64_t start, uint64_t pages,
                   uint32_t kind, uint64_t attributes = 8) noexcept
    {
        entry = {
            start,
            pages,
            kind,
            WARREN_BOOT_MEMORY_SOURCE_UEFI,
            7,
            0,
            attributes,
        };
    }

    [[nodiscard]] fixture_t make_fixture(uint64_t usable_pages = 256) noexcept
    {
        fixture_t fixture{};
        warren_boot_information_t& object{ header(fixture) };
        constexpr uint8_t magic[8]{ 'W', 'A', 'R', 'R', 'E', 'N', 'B', 'I' };
        for (uint32_t index{ 0 }; index < 8; ++index)
            object.magic[index] = magic[index];

        object.major = WARREN_BOOT_INFORMATION_MAJOR;
        object.minor = WARREN_BOOT_INFORMATION_MINOR;
        object.header_size = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        object.total_size = k_total_size;
        object.page_size = WARREN_BOOT_INFORMATION_PAGE_SIZE;
        object.present_features = WARREN_BOOT_FEATURE_MEMORY_MAP | WARREN_BOOT_FEATURE_EARLY_CONSOLE;
        object.required_features = WARREN_BOOT_FEATURE_MEMORY_MAP;
        object.self_physical_address = k_object_physical;
        object.kernel_physical_start = k_kernel_physical;
        object.kernel_physical_size = 4 * burrow::core::k_transition_page_size;
        object.kernel_load_bias = k_kernel_physical;
        object.kernel_entry_physical_address = k_kernel_physical + burrow::core::k_transition_page_size;
        object.bootstrap_stack_physical_start = k_stack_physical;
        object.bootstrap_stack_size = 16 * burrow::core::k_transition_page_size;
        object.memory_map = { k_memory_map_offset, 4, sizeof(warren_boot_memory_entry_t), 0 };
        object.early_console = { k_console_offset, 1, sizeof(warren_boot_early_console_t), 0 };

        warren_boot_memory_entry_t* map{ entries(fixture) };
        set_entry(map[0], k_object_physical, 1, WARREN_BOOT_MEMORY_BOOT_INFORMATION);
        set_entry(map[1], k_kernel_physical, 4, WARREN_BOOT_MEMORY_KERNEL_IMAGE);
        set_entry(map[2], k_stack_physical, 16, WARREN_BOOT_MEMORY_BOOTSTRAP_STACK);
        set_entry(map[3], k_usable_physical, usable_pages, WARREN_BOOT_MEMORY_USABLE);

        auto* console{ reinterpret_cast<warren_boot_early_console_t*>(fixture.bytes + k_console_offset) };
        console->kind = WARREN_BOOT_CONSOLE_PL011;
        console->flags = WARREN_BOOT_CONSOLE_OUTPUT;
        console->physical_address = burrow::core::k_reference_pl011_physical_address;
        console->register_stride = 4;
        console->register_width = 32;
        return fixture;
    }

    [[nodiscard]] transition_image_segment_t make_segment(
        uint64_t relative_start,
        uint64_t page_count,
        uint32_t permissions) noexcept
    {
        return {
            { k_kernel_physical + relative_start },
            { relative_start },
            { page_count },
            permissions,
            0,
        };
    }

    [[nodiscard]] transition_image_layout_t make_image_layout() noexcept
    {
        transition_image_layout_t layout{};
        layout.segment_count = 3;
        layout.segments[0] = make_segment(0, 1, burrow::core::k_transition_permission_read);
        layout.segments[1] = make_segment(
            0x1000,
            2,
            burrow::core::k_transition_permission_read | burrow::core::k_transition_permission_execute);
        layout.segments[2] = make_segment(
            0x3000,
            1,
            burrow::core::k_transition_permission_read | burrow::core::k_transition_permission_write);
        return layout;
    }

    [[nodiscard]] bool consume(fixture_t& fixture, validated_boot_information_t& view) noexcept
    {
        return burrow::core::consume_boot_information(
                   fixture.bytes,
                   k_total_size,
                   k_object_physical,
                   view) == boot_information_error_t::success;
    }

    [[nodiscard]] bool expect_u64(const char* name, uint64_t actual, uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected 0x%llx, received 0x%llx\n", name,
                     static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] bool expect_fixture_equal(const char* name,
                                            const fixture_t& actual,
                                            const fixture_t& expected) noexcept
    {
        for (uint32_t index{ 0 }; index < sizeof(actual.bytes); ++index)
        {
            if (actual.bytes[index] == expected.bytes[index]) continue;

            std::fprintf(stderr, "FAIL: %s: byte 0x%x changed from 0x%x to 0x%x\n",
                         name,
                         index,
                         expected.bytes[index],
                         actual.bytes[index]);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool expect_plan_result(const char* name,
                                          const validated_boot_information_t& view,
                                          const transition_image_layout_t& layout,
                                          transition_plan_error_t expected,
                                          transition_plan_t* captured_plan = nullptr) noexcept
    {
        transition_plan_t plan{};
        const transition_plan_error_t actual{
            burrow::core::plan_aarch64_transition(view, layout, plan)
        };
        if (captured_plan != nullptr) *captured_plan = plan;
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected %u, received %u\n", name,
                     static_cast<uint32_t>(expected), static_cast<uint32_t>(actual));
        return false;
    }

    [[nodiscard]] bool run_consumer_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        validated_boot_information_t view{};
        const boot_information_error_t result{
            burrow::core::consume_boot_information(
                fixture.bytes,
                k_total_size,
                k_object_physical,
                view)
        };
        passed &= expect_u64("consumer result", static_cast<uint32_t>(result),
                             static_cast<uint32_t>(boot_information_error_t::success));
        passed &= expect_u64("consumer byte count", view.byte_count.value, k_total_size);
        passed &= expect_u64("consumer map count", view.memory_entry_count, 4);
        passed &= expect_u64("consumer console", view.console_physical_address.value,
                             burrow::core::k_reference_pl011_physical_address);

        header(fixture).reserved[0] = 1;
        view.object = &header(fixture);
        const boot_information_error_t invalid_result{
            burrow::core::consume_boot_information(
                fixture.bytes,
                k_total_size,
                k_object_physical,
                view)
        };
        passed &= expect_u64("consumer forwards complete validation", static_cast<uint32_t>(invalid_result),
                             static_cast<uint32_t>(boot_information_error_t::nonzero_reserved));
        passed &= expect_u64("invalid consumer clears object", reinterpret_cast<uintptr_t>(view.object), 0);
        passed &= expect_u64("invalid consumer clears map", reinterpret_cast<uintptr_t>(view.memory_entries), 0);
        return passed;
    }

    [[nodiscard]] bool run_reference_plan_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        const fixture_t original_fixture{ fixture };
        validated_boot_information_t view{};
        if (!consume(fixture, view)) return false;

        transition_plan_t plan{};
        passed &= expect_plan_result("reference plan", view, make_image_layout(),
                                     transition_plan_error_t::success, &plan);
        passed &= expect_u64("reference mapping count", plan.mapping_count, 14);
        passed &= expect_u64("arena start", plan.arena_physical_start.value, k_usable_physical);
        passed &= expect_u64("arena pages", plan.arena_page_count.value, 128);
        passed &= expect_u64("empty root", plan.empty_root_physical_address.value, k_usable_physical);
        passed &= expect_u64("stack backing", plan.early_stack_physical_start.value, k_usable_physical + 0x1000);
        passed &= expect_u64("stack pages", plan.early_stack_page_count.value, 16);
        passed &= expect_u64("table backing", plan.page_table_physical_start.value, k_usable_physical + 0x11000);
        passed &= expect_u64("table pages", plan.page_table_page_count.value, 111);
        passed &= expect_u64("stack virtual start", plan.early_stack_virtual_start.value,
                             burrow::core::k_early_stack_virtual_start);
        passed &= expect_u64("stack virtual top", plan.early_stack_virtual_top.value,
                             burrow::core::k_early_stack_virtual_top);
        passed &= expect_u64("read-only identity physical", plan.mappings[0].physical_start.value,
                             k_kernel_physical);
        passed &= expect_u64("read-only stable virtual", plan.mappings[3].virtual_start.value,
                             burrow::core::k_kernel_virtual_bias);
        passed &= expect_u64("console device type", static_cast<uint32_t>(plan.mappings[12].memory_type),
                             static_cast<uint32_t>(burrow::core::transition_memory_type_t::device));
        passed &= expect_u64("owned stack virtual", plan.mappings[13].virtual_start.value,
                             burrow::core::k_early_stack_virtual_start);
        passed &= expect_fixture_equal("consumer and planner preserve protocol object",
                                       fixture,
                                       original_fixture);
        return passed;
    }

    [[nodiscard]] bool run_arena_selection_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture(64) };
        warren_boot_information_t& object{ header(fixture) };
        object.memory_map.count = 5;
        warren_boot_memory_entry_t* map{ entries(fixture) };
        set_entry(map[4], k_usable_physical + 64 * 4096, 64, WARREN_BOOT_MEMORY_USABLE);
        validated_boot_information_t view{};
        passed &= consume(fixture, view);
        passed &= expect_plan_result("matching fragmented exact fit", view, make_image_layout(),
                                     transition_plan_error_t::success);

        map[4].source_attributes = 9;
        passed &= consume(fixture, view);
        passed &= expect_plan_result("fragment attributes differ", view, make_image_layout(),
                                     transition_plan_error_t::transition_arena_exhausted);

        fixture = make_fixture(127);
        passed &= consume(fixture, view);
        passed &= expect_plan_result("one page short", view, make_image_layout(),
                                     transition_plan_error_t::transition_arena_exhausted);

        fixture = make_fixture(128);
        passed &= consume(fixture, view);
        transition_plan_t exact_plan{};
        passed &= expect_plan_result("arena exact fit", view, make_image_layout(),
                                     transition_plan_error_t::success, &exact_plan);
        passed &= expect_u64("exact fit address", exact_plan.arena_physical_start.value, k_usable_physical);

        fixture = make_fixture();
        map = entries(fixture);
        object.memory_map.count = 5;
        set_entry(map[4], k_usable_physical, 256, WARREN_BOOT_MEMORY_USABLE);
        set_entry(map[3], 0x00180000, 128, WARREN_BOOT_MEMORY_USABLE);
        warren_boot_memory_entry_t kernel{ map[1] };
        warren_boot_memory_entry_t stack{ map[2] };
        map[1] = map[3];
        map[2] = kernel;
        map[3] = stack;
        passed &= consume(fixture, view);
        transition_plan_t deterministic_plan{};
        passed &= expect_plan_result("lowest deterministic run", view, make_image_layout(),
                                     transition_plan_error_t::success, &deterministic_plan);
        passed &= expect_u64("lowest selected", deterministic_plan.arena_physical_start.value, 0x00180000);

        fixture = make_fixture();
        passed &= consume(fixture, view);
        map = entries(fixture);
        set_entry(map[0], k_object_physical, 129, WARREN_BOOT_MEMORY_USABLE);
        transition_plan_t exclusion_plan{};
        passed &= expect_plan_result("live object subtracted defensively", view, make_image_layout(),
                                     transition_plan_error_t::success, &exclusion_plan);
        passed &= expect_u64("object exclusion selected next page", exclusion_plan.arena_physical_start.value,
                             k_object_physical + 0x1000);
        return passed;
    }

    [[nodiscard]] bool run_layout_and_capacity_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        validated_boot_information_t view{};
        if (!consume(fixture, view)) return false;

        transition_image_layout_t layout{ make_image_layout() };
        layout.segments[1].physical_start = layout.segments[0].physical_start;
        layout.segments[1].image_relative_start = layout.segments[0].image_relative_start;
        passed &= expect_plan_result("overlapping image segments", view, layout,
                                     transition_plan_error_t::invalid_image_layout);

        layout = make_image_layout();
        layout.segments[2].permissions |= burrow::core::k_transition_permission_execute;
        passed &= expect_plan_result("writable executable image", view, layout,
                                     transition_plan_error_t::invalid_image_layout);

        layout = {};
        layout.segment_count = 4;
        for (uint32_t index{ 0 }; index < 4; ++index)
            layout.segments[index] = make_segment(index * 0x1000, 1, burrow::core::k_transition_permission_read);
        transition_plan_t exact_plan{};
        passed &= expect_plan_result("mapping capacity exact fit", view, layout,
                                     transition_plan_error_t::success, &exact_plan);
        passed &= expect_u64("mapping capacity count", exact_plan.mapping_count,
                             burrow::core::k_transition_mapping_capacity);

        header(fixture).kernel_physical_size = 5 * 4096;
        entries(fixture)[1].page_count = 5;
        passed &= consume(fixture, view);
        layout.segment_count = 5;
        layout.segments[4] = make_segment(0x4000, 1, burrow::core::k_transition_permission_read);
        transition_plan_t failed_plan{};
        passed &= expect_plan_result("one segment exceeds mapping capacity", view, layout,
                                     transition_plan_error_t::mapping_capacity_exceeded, &failed_plan);
        passed &= expect_u64("failed plan remains empty", failed_plan.mapping_count, 0);
        return passed;
    }

    [[nodiscard]] bool run_checked_boundary_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        validated_boot_information_t view{};
        if (!consume(fixture, view)) return false;

        view.console_physical_address.value += 0x1000;
        passed &= expect_plan_result("unexpected PL011 aperture", view, make_image_layout(),
                                     transition_plan_error_t::invalid_console_aperture);

        passed &= consume(fixture, view);
        warren_boot_memory_entry_t* map{ entries(fixture) };
        map[3].physical_start = UINT64_C(0xfffffffffffff000);
        map[3].page_count = 2;
        passed &= expect_plan_result("overflowing memory entry", view, make_image_layout(),
                                     transition_plan_error_t::invalid_memory_map);

        fixture = make_fixture(128);
        passed &= consume(fixture, view);
        map = entries(fixture);
        map[3].physical_start = burrow::core::k_direct_map_physical_limit;
        passed &= expect_plan_result("arena outside direct-map ceiling", view, make_image_layout(),
                                     transition_plan_error_t::physical_range_overflow);

        fixture = make_fixture();
        passed &= consume(fixture, view);
        transition_image_layout_t layout{ make_image_layout() };
        layout.segments[0].physical_start.value += 1;
        passed &= expect_plan_result("misaligned image segment", view, layout,
                                     transition_plan_error_t::invalid_image_layout);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_consumer_tests();
    passed &= run_reference_plan_tests();
    passed &= run_arena_selection_tests();
    passed &= run_layout_and_capacity_tests();
    passed &= run_checked_boundary_tests();

    if (!passed) return 1;

    std::puts("Warren transition-plan tests passed.");
    return 0;
}
