//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/AArch64Handoff.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::aarch64_executable_range_t;
    using warren::boot::aarch64_handoff_arguments_t;
    using warren::boot::aarch64_handoff_error_t;
    using warren::boot::boot_handoff_storage_plan_t;
    using warren::boot::boot_handoff_storage_t;
    using warren::boot::burrow_load_plan_t;
    using warren::boot::burrow_loaded_image_t;
    using warren::boot::burrow_segment_class_t;
    using warren::boot::plan_boot_handoff_storage;
    using warren::boot::prepare_aarch64_handoff;

    struct fixture_t
    {
        alignas(4096) uint8_t stack[65536]{};
        alignas(4096) uint8_t memory_map[8192]{};
        alignas(4096) uint8_t source[8192]{};
        alignas(4096) uint8_t work[12288]{};
        alignas(4096) uint8_t object[12288]{};
        burrow_load_plan_t load_plan{};
        burrow_loaded_image_t image{};
        boot_handoff_storage_t storage{};
        warren_boot_early_console_t console{};
    };

    void initialize_fixture(fixture_t& fixture) noexcept
    {
        boot_handoff_storage_plan_t storage_plan{};
        static_cast<void>(plan_boot_handoff_storage(4000, 40, true, storage_plan));
        fixture.storage = {
            storage_plan,
            { 0x00100000, 16, fixture.stack },
            { 0x00200000, storage_plan.memory_map_page_count, fixture.memory_map },
            { 0x00300000, storage_plan.source_page_count, fixture.source },
            { 0x00400000, storage_plan.work_page_count, fixture.work },
            { 0x00500000, storage_plan.object_page_count, fixture.object },
        };
        fixture.load_plan.segments[1] = {
            0x2000,
            0x2000,
            0x1800,
            0x3000,
            burrow_segment_class_t::executable,
        };
        fixture.load_plan.allocation_size = 0x10000;
        fixture.image = { 0x00600000, 0x10000, 0x00600000, 0x00602100 };
        fixture.console.kind = WARREN_BOOT_CONSOLE_PL011;
        fixture.console.flags = WARREN_BOOT_CONSOLE_OUTPUT;
        fixture.console.physical_address = 0x09000000;
        fixture.console.register_stride = 4;
        fixture.console.register_width = 32;
    }

    bool expect_error(const char* name, aarch64_handoff_error_t actual,
                      aarch64_handoff_error_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected %u, received %u\n", name,
                     static_cast<uint32_t>(expected), static_cast<uint32_t>(actual));
        return false;
    }

    bool expect_u64(const char* name, uint64_t actual, uint64_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected 0x%016llx, received 0x%016llx\n", name,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        return false;
    }

    aarch64_handoff_error_t prepare(fixture_t& fixture,
                                    aarch64_executable_range_t& range,
                                    aarch64_handoff_arguments_t& arguments) noexcept
    {
        return prepare_aarch64_handoff(
            fixture.load_plan,
            fixture.image,
            fixture.storage,
            fixture.console,
            range,
            arguments);
    }

    bool run_success_test() noexcept
    {
        fixture_t fixture{};
        initialize_fixture(fixture);
        aarch64_executable_range_t range{};
        aarch64_handoff_arguments_t arguments{};
        bool passed{ expect_error(
            "reference handoff", prepare(fixture, range, arguments),
            aarch64_handoff_error_t::success) };
        passed &= expect_u64("executable begin", range.physical_begin, 0x00602000);
        passed &= expect_u64("executable end", range.physical_end, 0x00605000);
        passed &= expect_u64("boot information argument",
                             arguments.boot_information_physical_address, 0x00500000);
        passed &= expect_u64("entry argument", arguments.entry_physical_address, 0x00602100);
        passed &= expect_u64("stack top argument", arguments.bootstrap_stack_top, 0x00110000);
        passed &= expect_u64("PL011 argument", arguments.pl011_physical_address, 0x09000000);
        return passed;
    }

    bool run_failure_tests() noexcept
    {
        bool passed{ true };
        aarch64_executable_range_t range{};
        aarch64_handoff_arguments_t arguments{};
        fixture_t fixture{};
        initialize_fixture(fixture);
        fixture.storage.plan.memory_map_capacity = 4096;
        passed &= expect_error("invalid storage", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_storage);

        initialize_fixture(fixture);
        fixture.image.physical_size = 0;
        passed &= expect_error("empty image", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_loaded_image);

        initialize_fixture(fixture);
        fixture.load_plan.segments[1].segment_class = burrow_segment_class_t::read_only;
        passed &= expect_error("wrong executable class", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_executable_segment);

        initialize_fixture(fixture);
        fixture.image.physical_start = UINT64_MAX - 0x1fff;
        fixture.image.physical_size = 0x1000;
        fixture.image.load_bias = fixture.image.physical_start;
        fixture.load_plan.allocation_size = fixture.image.physical_size;
        passed &= expect_error("executable start overflow", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::executable_range_overflow);

        initialize_fixture(fixture);
        fixture.load_plan.segments[1].virtual_address = 0x20000;
        passed &= expect_error("executable outside image", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::executable_range_outside_image);

        initialize_fixture(fixture);
        fixture.image.entry_physical_address = 0x00601000;
        passed &= expect_error("entry outside executable range", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_entry);

        initialize_fixture(fixture);
        fixture.storage.bootstrap_stack.physical_start = UINT64_MAX - 0xfff;
        passed &= expect_error("stack top overflow", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_stack);

        initialize_fixture(fixture);
        fixture.storage.object.physical_start += 4;
        passed &= expect_error("misaligned boot information", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_boot_information);

        initialize_fixture(fixture);
        fixture.console.flags = 0;
        passed &= expect_error("console without output", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_console);

        initialize_fixture(fixture);
        fixture.console.physical_address = UINT64_MAX - 3;
        passed &= expect_error("console register overflow", prepare(fixture, range, arguments),
                               aarch64_handoff_error_t::invalid_console);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_success_test();
    passed &= run_failure_tests();

    if (!passed) return 1;

    std::puts("Warren AArch64 handoff preparation tests passed.");
    return 0;
}
