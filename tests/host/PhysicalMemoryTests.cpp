//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/PhysicalMemory.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint32_t k_entry_capacity{ 140 };
    constexpr uint64_t k_object_physical{ 0x00100000 };

    struct fixture_t
    {
        warren_boot_information_t object{};
        warren_boot_memory_entry_t entries[k_entry_capacity]{};
        uint32_t entry_count{};
    };

    struct console_capture_t
    {
        char bytes[320]{};
        uint32_t byte_count{};
        uint32_t fail_at{ UINT32_MAX };
    };

    [[nodiscard]] bool capture_byte(void* context, uint8_t byte) noexcept
    {
        if (context == nullptr) return false;
        auto& capture{ *static_cast<console_capture_t*>(context) };
        if (capture.byte_count == capture.fail_at ||
            capture.byte_count >= sizeof(capture.bytes) - 1)
            return false;
        capture.bytes[capture.byte_count++] = static_cast<char>(byte);
        return true;
    }

    void set_entry(
        warren_boot_memory_entry_t& entry,
        uint64_t physical_start,
        uint64_t page_count,
        uint32_t memory_kind = WARREN_BOOT_MEMORY_USABLE,
        uint64_t source_attributes = 8) noexcept
    {
        entry = {
            physical_start,
            page_count,
            memory_kind,
            WARREN_BOOT_MEMORY_SOURCE_UEFI,
            7,
            0,
            source_attributes,
        };
    }

    [[nodiscard]] fixture_t make_reference_fixture() noexcept
    {
        fixture_t fixture{};
        fixture.object.total_size = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        fixture.object.self_physical_address = k_object_physical;
        fixture.entry_count = 2;
        set_entry(fixture.entries[0], 0, 64, WARREN_BOOT_MEMORY_USABLE, 8);
        set_entry(
            fixture.entries[1],
            64 * burrow::core::k_transition_page_size,
            64,
            WARREN_BOOT_MEMORY_USABLE,
            9);
        return fixture;
    }

    [[nodiscard]] burrow::core::validated_boot_information_t view(
        fixture_t& fixture) noexcept
    {
        return {
            &fixture.object,
            { fixture.object.self_physical_address },
            { fixture.object.total_size },
            fixture.entries,
            fixture.entry_count,
            0,
            { 0 },
        };
    }

    [[nodiscard]] bool expect_u64(
        const char* name,
        uint64_t actual,
        uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(
            stderr,
            "FAIL: %s: expected 0x%llx, received 0x%llx\n",
            name,
            static_cast<unsigned long long>(expected),
            static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] bool expect_error(
        const char* name,
        burrow::core::physical_memory_error_t actual,
        burrow::core::physical_memory_error_t expected) noexcept
    {
        return expect_u64(
            name,
            static_cast<uint32_t>(actual),
            static_cast<uint32_t>(expected));
    }

    [[nodiscard]] bool initialize_reference(
        fixture_t& fixture,
        burrow::core::physical_memory_state_t& state) noexcept
    {
        return burrow::core::initialize_physical_memory(
                   view(fixture),
                   { 0x00020000 },
                   { 4 },
                   state) ==
            burrow::core::physical_memory_error_t::success;
    }

    [[nodiscard]] bool run_inventory_tests() noexcept
    {
        using burrow::core::physical_memory_error_t;

        fixture_t fixture{ make_reference_fixture() };
        burrow::core::physical_memory_state_t state{};
        bool passed{ expect_error(
            "reference inventory",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x00020000 }, { 4 }, state),
            physical_memory_error_t::success) };
        passed &= expect_u64("state marker", state.magic,
                             burrow::core::k_physical_memory_state_magic);
        passed &= expect_u64("extent count", state.extent_count, 3);
        passed &= expect_u64("allocatable pages", state.total_page_count.value,
                             123);
        passed &= expect_u64("excluded page zero and arena",
                             state.excluded_page_count.value, 5);
        passed &= expect_u64("first extent start",
                             state.extents[0].physical_start.value, 0x1000);
        passed &= expect_u64("first extent pages",
                             state.extents[0].page_count.value, 31);
        passed &= expect_u64("post-arena extent start",
                             state.extents[1].physical_start.value, 0x24000);
        passed &= expect_u64("post-arena extent pages",
                             state.extents[1].page_count.value, 28);
        passed &= expect_u64("source boundary retained",
                             state.extents[2].source_attributes, 9);

        fixture = make_reference_fixture();
        set_entry(
            fixture.entries[1],
            burrow::core::k_direct_map_physical_limit - 0x1000,
            2);
        state = {};
        passed &= expect_error(
            "direct-map ceiling truncation",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x20000 }, { 4 }, state),
            physical_memory_error_t::success);
        passed &= expect_u64("unmanaged high pages",
                             state.unmanaged_page_count.value, 1);
        passed &= expect_u64("ceiling extent start",
                             state.extents[2].physical_start.value,
                             burrow::core::k_direct_map_physical_limit - 0x1000);

        fixture = make_reference_fixture();
        set_entry(fixture.entries[0], 0, 64, WARREN_BOOT_MEMORY_RESERVED);
        state.magic = UINT64_MAX;
        passed &= expect_error(
            "arena must remain usable",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x20000 }, { 4 }, state),
            physical_memory_error_t::invalid_transition_arena);
        passed &= expect_u64("failed inventory remains unpublished",
                             state.magic, 0);

        fixture = make_reference_fixture();
        fixture.entries[1].physical_start = 0x30000;
        passed &= expect_error(
            "overlapping source map",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x20000 }, { 4 }, state),
            physical_memory_error_t::invalid_memory_map);
        return passed;
    }

    [[nodiscard]] bool run_capacity_tests() noexcept
    {
        using burrow::core::physical_memory_error_t;

        fixture_t fixture{};
        fixture.object.total_size = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        fixture.object.self_physical_address = k_object_physical;
        fixture.entry_count =
            burrow::core::k_physical_memory_extent_capacity + 1;
        set_entry(fixture.entries[0], 0x1000, 1);
        for (uint32_t index{ 0 };
             index < burrow::core::k_physical_memory_extent_capacity;
             ++index)
        {
            set_entry(
                fixture.entries[index + 1],
                (index + 2) * burrow::core::k_transition_page_size,
                1,
                WARREN_BOOT_MEMORY_USABLE,
                index + 1);
        }

        burrow::core::physical_memory_state_t state{};
        bool passed{ expect_error(
            "exact extent capacity",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x1000 }, { 1 }, state),
            physical_memory_error_t::success) };
        passed &= expect_u64("exact extent count", state.extent_count,
                             burrow::core::k_physical_memory_extent_capacity);

        fixture.entry_count++;
        set_entry(
            fixture.entries[fixture.entry_count - 1],
            (burrow::core::k_physical_memory_extent_capacity + 2) *
                burrow::core::k_transition_page_size,
            1,
            WARREN_BOOT_MEMORY_USABLE,
            0x100);
        passed &= expect_error(
            "extent capacity exceeded",
            burrow::core::initialize_physical_memory(
                view(fixture), { 0x1000 }, { 1 }, state),
            physical_memory_error_t::extent_capacity_exceeded);
        passed &= expect_u64("capacity failure clears marker", state.magic, 0);
        passed &= expect_u64("capacity failure clears count",
                             state.extent_count, 0);
        return passed;
    }

    [[nodiscard]] bool run_allocation_tests() noexcept
    {
        using burrow::core::physical_memory_error_t;

        fixture_t fixture{ make_reference_fixture() };
        burrow::core::physical_memory_state_t state{};
        if (!initialize_reference(fixture, state)) return false;

        burrow::core::boot_allocation_t allocation{};
        bool passed{ expect_error(
            "aligned allocation",
            burrow::core::allocate_boot_pages(
                state, { 4 }, { 4 }, allocation),
            physical_memory_error_t::success) };
        passed &= expect_u64("lowest aligned address",
                             allocation.physical_start.value, 0x4000);
        passed &= expect_u64("requested pages", allocation.page_count.value, 4);
        passed &= expect_u64("padding charged", allocation.consumed_page_count.value,
                             7);
        passed &= expect_u64("state consumed pages",
                             state.consumed_page_count.value, 7);
        passed &= expect_u64("state allocated pages",
                             state.allocated_page_count.value, 4);

        const burrow::core::physical_memory_state_t before_failure{ state };
        allocation.physical_start = { UINT64_MAX };
        passed &= expect_error(
            "invalid non-power-of-two alignment",
            burrow::core::allocate_boot_pages(
                state, { 1 }, { 3 }, allocation),
            physical_memory_error_t::invalid_request);
        passed &= expect_u64("failed allocation clears output",
                             allocation.physical_start.value, 0);
        passed &= expect_u64("failed allocation preserves state",
                             std::memcmp(&state, &before_failure, sizeof(state)), 0);

        for (uint32_t index{ 0 }; index < state.extent_count; ++index)
            state.extents[index].consumed_page_count =
                state.extents[index].page_count;
        state.consumed_page_count = state.total_page_count;
        state.allocated_page_count = state.total_page_count;
        passed &= expect_error(
            "allocator exhaustion",
            burrow::core::allocate_boot_pages(
                state, { 1 }, { 1 }, allocation),
            physical_memory_error_t::exhausted);

        state.magic = 0;
        passed &= expect_error(
            "unpublished state rejected",
            burrow::core::allocate_boot_pages(
                state, { 1 }, { 1 }, allocation),
            physical_memory_error_t::invalid_state);
        return passed;
    }

    [[nodiscard]] bool run_report_tests() noexcept
    {
        using burrow::core::physical_memory_error_t;

        fixture_t fixture{ make_reference_fixture() };
        burrow::core::physical_memory_state_t state{};
        if (!initialize_reference(fixture, state)) return false;
        burrow::core::boot_allocation_t allocation{};
        if (burrow::core::allocate_boot_pages(
                state, { 4 }, { 4 }, allocation) !=
            physical_memory_error_t::success)
            return false;

        console_capture_t capture{};
        const burrow::drivers::console_writer_t console{
            &capture,
            capture_byte,
        };
        bool passed{ expect_error(
            "memory report",
            burrow::core::report_physical_memory(state, allocation, console),
            physical_memory_error_t::success) };
        passed &= expect_u64(
            "memory report content",
            std::strcmp(
                capture.bytes,
                "BURROW_MEMORY_V1:extents=3:pages=123:"
                "arena=0x0000000000020000:arena_pages=4:"
                "boot_alloc=0x0000000000004000:boot_pages=4:remaining=116\r\n"),
            0);

        capture = {};
        capture.fail_at = 12;
        passed &= expect_error(
            "memory report output failure",
            burrow::core::report_physical_memory(state, allocation, console),
            physical_memory_error_t::output_failure);

        state.extents[0].consumed_page_count.value =
            state.extents[0].page_count.value + 1;
        passed &= expect_error(
            "corrupt state report",
            burrow::core::report_physical_memory(state, allocation, console),
            physical_memory_error_t::invalid_state);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_inventory_tests();
    passed &= run_capacity_tests();
    passed &= run_allocation_tests();
    passed &= run_report_tests();
    if (!passed) return 1;

    std::puts("Warren physical-memory tests passed.");
    return 0;
}
