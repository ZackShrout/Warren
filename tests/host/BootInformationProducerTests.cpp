//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformationProducer.h>
#include <warren/boot/BootInformationValidation.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::boot_information_producer_error_t;
    using warren::boot::boot_information_producer_input_t;
    using warren::boot::boot_information_producer_output_t;
    using warren::boot::boot_information_source_descriptor_t;
    using warren::boot::boot_information_error_t;
    using warren::boot::produce_boot_information;
    using warren::boot::uefi_memory_type_t;
    using warren::boot::validate_boot_information;

    constexpr uint32_t k_buffer_size{ 4096 };
    constexpr uint32_t k_work_entry_capacity{ 64 };
    constexpr uint64_t k_source_start{ 0x00100000 };
    constexpr uint64_t k_object_start{ 0x00110000 };
    constexpr uint64_t k_kernel_start{ 0x00120000 };
    constexpr uint64_t k_stack_start{ 0x00130000 };
    constexpr uint64_t k_source_attributes{ UINT64_C(0x8000000000000008) };

    struct output_fixture_t
    {
        alignas(8) uint8_t bytes[k_buffer_size]{};
        warren_boot_memory_entry_t work_entries[k_work_entry_capacity]{};
        boot_information_producer_output_t result{};
    };

    [[nodiscard]] constexpr uint32_t source_type(uefi_memory_type_t type) noexcept
    {
        return static_cast<uint32_t>(type);
    }

    void fill_bytes(uint8_t* bytes, uint32_t count, uint8_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < count; ++index)
            bytes[index] = value;
    }

    void write_u16(uint8_t* bytes, uint32_t offset, uint16_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 2; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
    }

    void write_u32(uint8_t* bytes, uint32_t offset, uint32_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 4; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
    }

    void write_u64(uint8_t* bytes, uint32_t offset, uint64_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 8; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
    }

    void write_section(uint8_t* bytes, uint32_t descriptor_offset, uint32_t section_offset,
                       uint32_t count, uint32_t stride) noexcept
    {
        write_u32(bytes, descriptor_offset + 0x00, section_offset);
        write_u32(bytes, descriptor_offset + 0x04, count);
        write_u32(bytes, descriptor_offset + 0x08, stride);
    }

    void write_memory_entry(uint8_t* bytes, uint32_t offset, uint64_t physical_start, uint64_t page_count,
                            uint32_t memory_kind, uint32_t retained_source_type,
                            uint64_t retained_attributes) noexcept
    {
        write_u64(bytes, offset + 0x00, physical_start);
        write_u64(bytes, offset + 0x08, page_count);
        write_u32(bytes, offset + 0x10, memory_kind);
        write_u32(bytes, offset + 0x14, WARREN_BOOT_MEMORY_SOURCE_UEFI);
        write_u32(bytes, offset + 0x18, retained_source_type);
        write_u64(bytes, offset + 0x20, retained_attributes);
    }

    [[nodiscard]] warren_boot_early_console_t make_console() noexcept
    {
        warren_boot_early_console_t console{};
        console.kind = WARREN_BOOT_CONSOLE_PL011;
        console.flags = WARREN_BOOT_CONSOLE_OUTPUT;
        console.physical_address = 0x09000000;
        console.register_stride = 4;
        console.register_width = 32;
        console.input_clock_hz = 24000000;
        console.baud_rate = 115200;
        return console;
    }

    [[nodiscard]] boot_information_producer_input_t make_input(
        const boot_information_source_descriptor_t* descriptors,
        uint32_t descriptor_count,
        const warren_boot_early_console_t* console) noexcept
    {
        return {
            descriptors,
            descriptor_count,
            k_object_start,
            0x1000,
            k_kernel_start,
            0x2000,
            0x20000,
            k_kernel_start + 0x100,
            k_stack_start,
            0x10000,
            console,
        };
    }

    bool expect_error(const char* name, boot_information_producer_error_t actual,
                      boot_information_producer_error_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected producer result %u, received %u\n", name,
                     static_cast<uint32_t>(expected), static_cast<uint32_t>(actual));
        return false;
    }

    bool expect_u32(const char* name, uint32_t actual, uint32_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected 0x%08x, received 0x%08x\n", name, expected, actual);
        return false;
    }

    bool expect_u64(const char* name, uint64_t actual, uint64_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected 0x%016llx, received 0x%016llx\n", name,
                     static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
        return false;
    }

    bool expect_bytes(const char* name, const uint8_t* actual, const uint8_t* expected, uint32_t count) noexcept
    {
        for (uint32_t index{ 0 }; index < count; ++index)
        {
            if (actual[index] == expected[index]) continue;

            std::fprintf(stderr, "FAIL: %s: byte 0x%x expected 0x%02x, received 0x%02x\n", name, index,
                         expected[index], actual[index]);
            return false;
        }

        return true;
    }

    bool expect_invalidated(const char* name, const output_fixture_t& fixture) noexcept
    {
        for (uint32_t index{ 0 }; index < WARREN_BOOT_INFORMATION_MAGIC_SIZE; ++index)
        {
            if (fixture.bytes[index] == 0) continue;

            std::fprintf(stderr, "FAIL: %s: failed output retained a valid-looking magic prefix\n", name);
            return false;
        }

        return true;
    }

    boot_information_producer_error_t produce(const boot_information_producer_input_t& input,
                                               output_fixture_t& fixture,
                                               uint32_t output_capacity = k_buffer_size,
                                               uint32_t work_capacity = k_work_entry_capacity) noexcept
    {
        fill_bytes(fixture.bytes, k_buffer_size, 0xa5);
        fixture.result = { UINT32_MAX, UINT32_MAX };
        return produce_boot_information(input, fixture.bytes, output_capacity, fixture.work_entries,
                                        work_capacity, fixture.result);
    }

    bool run_canonical_fixture_test() noexcept
    {
        const boot_information_source_descriptor_t descriptors[]{
            { k_source_start, 0x50, source_type(uefi_memory_type_t::conventional), k_source_attributes },
        };
        const warren_boot_early_console_t console{ make_console() };
        const boot_information_producer_input_t input{ make_input(descriptors, 1, &console) };
        output_fixture_t fixture{};
        const boot_information_producer_error_t producer_result{ produce(input, fixture) };
        bool passed{ expect_error("canonical production", producer_result,
                                  boot_information_producer_error_t::success) };

        constexpr uint32_t expected_entry_count{ 7 };
        constexpr uint32_t expected_total_size{
            WARREN_BOOT_INFORMATION_HEADER_SIZE +
            expected_entry_count * sizeof(warren_boot_memory_entry_t) + sizeof(warren_boot_early_console_t)
        };
        passed &= expect_u32("canonical entry count", fixture.result.memory_entry_count, expected_entry_count);
        passed &= expect_u32("canonical total size", fixture.result.total_size, expected_total_size);

        alignas(8) uint8_t expected[k_buffer_size]{};
        constexpr uint8_t magic[8]{ 'W', 'A', 'R', 'R', 'E', 'N', 'B', 'I' };
        for (uint32_t index{ 0 }; index < 8; ++index)
            expected[index] = magic[index];

        write_u16(expected, 0x008, WARREN_BOOT_INFORMATION_MAJOR);
        write_u16(expected, 0x00a, WARREN_BOOT_INFORMATION_MINOR);
        write_u32(expected, 0x00c, WARREN_BOOT_INFORMATION_HEADER_SIZE);
        write_u32(expected, 0x010, expected_total_size);
        write_u32(expected, 0x014, WARREN_BOOT_INFORMATION_PAGE_SIZE);
        write_u64(expected, 0x018, WARREN_BOOT_FEATURE_MEMORY_MAP | WARREN_BOOT_FEATURE_EARLY_CONSOLE);
        write_u64(expected, 0x020, WARREN_BOOT_FEATURE_MEMORY_MAP);
        write_u64(expected, 0x028, k_object_start);
        write_u64(expected, 0x030, k_kernel_start);
        write_u64(expected, 0x038, 0x2000);
        write_u64(expected, 0x040, 0x20000);
        write_u64(expected, 0x048, k_kernel_start + 0x100);
        write_u64(expected, 0x050, k_stack_start);
        write_u64(expected, 0x058, 0x10000);
        write_section(expected, 0x088, 0x100, expected_entry_count, sizeof(warren_boot_memory_entry_t));
        write_section(expected, 0x0a8, 0x218, 1, sizeof(warren_boot_early_console_t));

        write_memory_entry(expected, 0x100, 0x00100000, 0x10, WARREN_BOOT_MEMORY_USABLE,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x128, 0x00110000, 0x01, WARREN_BOOT_MEMORY_BOOT_INFORMATION,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x150, 0x00111000, 0x0f, WARREN_BOOT_MEMORY_USABLE,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x178, 0x00120000, 0x02, WARREN_BOOT_MEMORY_KERNEL_IMAGE,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x1a0, 0x00122000, 0x0e, WARREN_BOOT_MEMORY_USABLE,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x1c8, 0x00130000, 0x10, WARREN_BOOT_MEMORY_BOOTSTRAP_STACK,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);
        write_memory_entry(expected, 0x1f0, 0x00140000, 0x10, WARREN_BOOT_MEMORY_USABLE,
                           source_type(uefi_memory_type_t::conventional), k_source_attributes);

        write_u32(expected, 0x218 + 0x00, WARREN_BOOT_CONSOLE_PL011);
        write_u32(expected, 0x218 + 0x04, WARREN_BOOT_CONSOLE_OUTPUT);
        write_u64(expected, 0x218 + 0x08, 0x09000000);
        write_u32(expected, 0x218 + 0x10, 4);
        write_u32(expected, 0x218 + 0x14, 32);
        write_u64(expected, 0x218 + 0x18, 24000000);
        write_u32(expected, 0x218 + 0x20, 115200);

        passed &= expect_bytes("canonical independent bytes", fixture.bytes, expected, expected_total_size);
        passed &= expect_u32("canonical consumer validation",
                             static_cast<uint32_t>(validate_boot_information(
                                 fixture.bytes, fixture.result.total_size, k_object_start)),
                             static_cast<uint32_t>(boot_information_error_t::success));
        return passed;
    }

    bool run_type_mapping_test() noexcept
    {
        constexpr uint32_t type_count{ 16 };
        boot_information_source_descriptor_t descriptors[type_count]{};

        for (uint32_t index{ 0 }; index < type_count; ++index)
        {
            descriptors[index] = {
                k_source_start + static_cast<uint64_t>(index) * WARREN_BOOT_INFORMATION_PAGE_SIZE,
                1,
                index,
                UINT64_C(0x1000000000000000) | index,
            };
        }

        boot_information_producer_input_t input{ make_input(descriptors, type_count, nullptr) };
        input.object_physical_start = k_source_start + 0x1000;
        input.kernel_physical_start = k_source_start + 0x2000;
        input.kernel_physical_size = 0x1000;
        input.kernel_load_bias = 0x1000;
        input.kernel_entry_physical_address = input.kernel_physical_start;
        input.bootstrap_stack_physical_start = k_source_start + 0x3000;
        input.bootstrap_stack_size = 0x1000;

        output_fixture_t fixture{};
        bool passed{ expect_error("all type production", produce(input, fixture),
                                  boot_information_producer_error_t::success) };
        passed &= expect_u32("all types remain distinct", fixture.result.memory_entry_count, type_count);

        const uint32_t expected_kinds[type_count]{
            WARREN_BOOT_MEMORY_RESERVED,
            WARREN_BOOT_MEMORY_BOOT_INFORMATION,
            WARREN_BOOT_MEMORY_KERNEL_IMAGE,
            WARREN_BOOT_MEMORY_BOOTSTRAP_STACK,
            WARREN_BOOT_MEMORY_FIRMWARE_RECLAIMABLE,
            WARREN_BOOT_MEMORY_FIRMWARE_RUNTIME,
            WARREN_BOOT_MEMORY_FIRMWARE_RUNTIME,
            WARREN_BOOT_MEMORY_USABLE,
            WARREN_BOOT_MEMORY_UNUSABLE,
            WARREN_BOOT_MEMORY_ACPI_RECLAIMABLE,
            WARREN_BOOT_MEMORY_ACPI_NVS,
            WARREN_BOOT_MEMORY_MMIO,
            WARREN_BOOT_MEMORY_MMIO,
            WARREN_BOOT_MEMORY_RESERVED,
            WARREN_BOOT_MEMORY_PERSISTENT,
            WARREN_BOOT_MEMORY_RESERVED,
        };

        const auto* header{ reinterpret_cast<const warren_boot_information_t*>(fixture.bytes) };
        const auto* entries{ reinterpret_cast<const warren_boot_memory_entry_t*>(
            fixture.bytes + header->memory_map.offset) };

        for (uint32_t index{ 0 }; index < type_count; ++index)
        {
            passed &= expect_u32("mapped memory kind", entries[index].memory_kind, expected_kinds[index]);
            passed &= expect_u32("retained source type", entries[index].source_type, index);
            passed &= expect_u64("retained source attributes", entries[index].source_attributes,
                                 UINT64_C(0x1000000000000000) | index);
        }

        passed &= expect_u64("memory-only present features", header->present_features,
                             WARREN_BOOT_FEATURE_MEMORY_MAP);
        passed &= expect_u64("memory-only required features", header->required_features,
                             WARREN_BOOT_FEATURE_MEMORY_MAP);
        passed &= expect_u32("absent console descriptor offset", header->early_console.offset, 0);
        passed &= expect_u32("absent console descriptor count", header->early_console.count, 0);
        passed &= expect_u32("absent console descriptor stride", header->early_console.stride, 0);
        passed &= expect_u32("all types consumer validation",
                             static_cast<uint32_t>(validate_boot_information(
                                 fixture.bytes, fixture.result.total_size, input.object_physical_start)),
                             static_cast<uint32_t>(boot_information_error_t::success));
        return passed;
    }

    bool run_coalescing_test() noexcept
    {
        const boot_information_source_descriptor_t descriptors[]{
            { 0x00100000, 0x10, source_type(uefi_memory_type_t::conventional), 8 },
            { 0x00110000, 0x01, source_type(uefi_memory_type_t::conventional), 8 },
            { 0x00111000, 0x0f, source_type(uefi_memory_type_t::conventional), 8 },
            { 0x00120000, 0x02, source_type(uefi_memory_type_t::conventional), 16 },
            { 0x00122000, 0x2e, source_type(uefi_memory_type_t::conventional), 8 },
        };
        const boot_information_producer_input_t input{ make_input(descriptors, 5, nullptr) };
        output_fixture_t fixture{};
        bool passed{ expect_error("coalescing production", produce(input, fixture),
                                  boot_information_producer_error_t::success) };
        passed &= expect_u32("coalesced entry count", fixture.result.memory_entry_count, 7);

        const auto* header{ reinterpret_cast<const warren_boot_information_t*>(fixture.bytes) };
        const auto* entries{ reinterpret_cast<const warren_boot_memory_entry_t*>(
            fixture.bytes + header->memory_map.offset) };
        passed &= expect_u64("same metadata coalesces across source boundary", entries[0].page_count, 0x10);
        passed &= expect_u64("resource retains source metadata", entries[1].page_count, 1);
        passed &= expect_u64("same metadata resumes after resource", entries[2].page_count, 0x0f);
        passed &= expect_u64("different attributes prevent coalescing", entries[3].page_count, 2);
        return passed;
    }

    bool run_cross_descriptor_overlay_test() noexcept
    {
        const boot_information_source_descriptor_t descriptors[]{
            { 0x00100000, 0x21, source_type(uefi_memory_type_t::conventional), 8 },
            { 0x00121000, 0x2f, source_type(uefi_memory_type_t::conventional), 16 },
        };
        boot_information_producer_input_t input{ make_input(descriptors, 2, nullptr) };
        input.kernel_physical_size = 0x3000;
        output_fixture_t fixture{};
        bool passed{ expect_error("cross-descriptor overlay production", produce(input, fixture),
                                  boot_information_producer_error_t::success) };

        const auto* header{ reinterpret_cast<const warren_boot_information_t*>(fixture.bytes) };
        const auto* entries{ reinterpret_cast<const warren_boot_memory_entry_t*>(
            fixture.bytes + header->memory_map.offset) };
        passed &= expect_u32("cross-descriptor entry count", fixture.result.memory_entry_count, 8);
        passed &= expect_u32("first kernel overlay kind", entries[3].memory_kind,
                             WARREN_BOOT_MEMORY_KERNEL_IMAGE);
        passed &= expect_u64("first kernel overlay pages", entries[3].page_count, 1);
        passed &= expect_u64("first kernel overlay attributes", entries[3].source_attributes, 8);
        passed &= expect_u32("second kernel overlay kind", entries[4].memory_kind,
                             WARREN_BOOT_MEMORY_KERNEL_IMAGE);
        passed &= expect_u64("second kernel overlay pages", entries[4].page_count, 2);
        passed &= expect_u64("second kernel overlay attributes", entries[4].source_attributes, 16);
        passed &= expect_u32("cross-descriptor consumer validation",
                             static_cast<uint32_t>(validate_boot_information(
                                 fixture.bytes, fixture.result.total_size, input.object_physical_start)),
                             static_cast<uint32_t>(boot_information_error_t::success));
        return passed;
    }

    bool run_source_failure_tests() noexcept
    {
        bool passed{ true };
        const warren_boot_early_console_t console{ make_console() };
        boot_information_source_descriptor_t descriptors[]{
            { k_source_start, 0x50, source_type(uefi_memory_type_t::conventional), 8 },
            { k_source_start + 0x50000, 1, source_type(uefi_memory_type_t::reserved), 0 },
        };
        boot_information_producer_input_t input{ make_input(descriptors, 1, &console) };
        output_fixture_t fixture{};

        input.source_descriptors = nullptr;
        passed &= expect_error("null source map", produce(input, fixture),
                               boot_information_producer_error_t::null_source_map);
        passed &= expect_invalidated("null source map invalidation", fixture);

        input = make_input(descriptors, 0, &console);
        passed &= expect_error("empty source map", produce(input, fixture),
                               boot_information_producer_error_t::empty_source_map);

        input = make_input(descriptors, 1, &console);
        descriptors[0].page_count = 0;
        passed &= expect_error("zero source pages", produce(input, fixture),
                               boot_information_producer_error_t::invalid_source_descriptor);
        descriptors[0].page_count = 0x50;

        descriptors[0].physical_start++;
        passed &= expect_error("misaligned source", produce(input, fixture),
                               boot_information_producer_error_t::invalid_source_descriptor);
        descriptors[0].physical_start = k_source_start;

        descriptors[0].physical_start = UINT64_C(0xfffffffffffff000);
        descriptors[0].page_count = 2;
        passed &= expect_error("overflowing source", produce(input, fixture),
                               boot_information_producer_error_t::invalid_source_descriptor);
        descriptors[0] = { k_source_start, 0x50, source_type(uefi_memory_type_t::conventional), 8 };

        input.source_descriptor_count = 2;
        descriptors[1].physical_start = k_source_start + 0x40000;
        passed &= expect_error("overlapping source descriptors", produce(input, fixture),
                               boot_information_producer_error_t::invalid_source_descriptor);
        descriptors[1].physical_start = k_source_start - 0x1000;
        passed &= expect_error("unordered source descriptors", produce(input, fixture),
                               boot_information_producer_error_t::invalid_source_descriptor);

        input.source_descriptor_count = 1;
        descriptors[0].source_type = 16;
        passed &= expect_error("unknown source type", produce(input, fixture),
                               boot_information_producer_error_t::unsupported_source_type);
        descriptors[0].source_type = 0x70000000;
        passed &= expect_error("OEM-reserved source type", produce(input, fixture),
                               boot_information_producer_error_t::unsupported_source_type);
        descriptors[0].source_type = 0x80000000;
        passed &= expect_error("OS-reserved source type", produce(input, fixture),
                               boot_information_producer_error_t::unsupported_source_type);
        return passed;
    }

    bool run_resource_failure_tests() noexcept
    {
        bool passed{ true };
        const warren_boot_early_console_t console{ make_console() };
        boot_information_source_descriptor_t descriptors[]{
            { k_source_start, 0x50, source_type(uefi_memory_type_t::conventional), 8 },
        };
        boot_information_producer_input_t input{ make_input(descriptors, 1, &console) };
        output_fixture_t fixture{};

        input.object_physical_start++;
        passed &= expect_error("misaligned object", produce(input, fixture),
                               boot_information_producer_error_t::invalid_object_range);

        input = make_input(descriptors, 1, &console);
        input.kernel_physical_size = 0;
        passed &= expect_error("empty kernel", produce(input, fixture),
                               boot_information_producer_error_t::invalid_kernel_range);

        input = make_input(descriptors, 1, &console);
        input.kernel_load_bias = input.kernel_physical_start + 1;
        passed &= expect_error("invalid load bias", produce(input, fixture),
                               boot_information_producer_error_t::invalid_kernel_range);

        input = make_input(descriptors, 1, &console);
        input.kernel_entry_physical_address = input.kernel_physical_start + input.kernel_physical_size;
        passed &= expect_error("entry outside kernel", produce(input, fixture),
                               boot_information_producer_error_t::invalid_entry_address);

        input = make_input(descriptors, 1, &console);
        input.bootstrap_stack_size = 0;
        passed &= expect_error("empty stack", produce(input, fixture),
                               boot_information_producer_error_t::invalid_stack_range);

        input = make_input(descriptors, 1, &console);
        input.bootstrap_stack_physical_start = input.kernel_physical_start;
        input.bootstrap_stack_size = 0x1000;
        passed &= expect_error("overlapping live resources", produce(input, fixture),
                               boot_information_producer_error_t::overlapping_resources);

        input = make_input(descriptors, 1, &console);
        descriptors[0].page_count = 0x20;
        passed &= expect_error("resource outside source map", produce(input, fixture),
                               boot_information_producer_error_t::resource_not_covered);

        const boot_information_source_descriptor_t gapped_descriptors[]{
            { k_source_start, 0x10, source_type(uefi_memory_type_t::conventional), 8 },
            { k_source_start + 0x11000, 0x3f, source_type(uefi_memory_type_t::conventional), 8 },
        };
        input = make_input(gapped_descriptors, 2, &console);
        passed &= expect_error("gap beneath object", produce(input, fixture),
                               boot_information_producer_error_t::resource_not_covered);

        warren_boot_early_console_t invalid_console{ console };
        invalid_console.register_stride = 8;
        input = make_input(descriptors, 1, &invalid_console);
        descriptors[0].page_count = 0x50;
        passed &= expect_error("invalid optional console", produce(input, fixture),
                               boot_information_producer_error_t::invalid_console);
        return passed;
    }

    bool run_capacity_and_atomicity_tests() noexcept
    {
        bool passed{ true };
        const warren_boot_early_console_t console{ make_console() };
        const boot_information_source_descriptor_t descriptors[]{
            { k_source_start, 0x50, source_type(uefi_memory_type_t::conventional), 8 },
        };
        const boot_information_producer_input_t input{ make_input(descriptors, 1, &console) };
        output_fixture_t fixture{};

        fill_bytes(fixture.bytes, k_buffer_size, 0xa5);
        passed &= expect_error(
            "null output",
            produce_boot_information(input, nullptr, k_buffer_size, fixture.work_entries,
                                     k_work_entry_capacity, fixture.result),
            boot_information_producer_error_t::null_output);

        passed &= expect_error(
            "unaligned output",
            produce_boot_information(input, fixture.bytes + 1, k_buffer_size - 1, fixture.work_entries,
                                     k_work_entry_capacity, fixture.result),
            boot_information_producer_error_t::unaligned_output);

        passed &= expect_error("short fixed output", produce(input, fixture, 7),
                               boot_information_producer_error_t::output_too_small);

        fill_bytes(fixture.bytes, k_buffer_size, 0xa5);
        passed &= expect_error(
            "null work storage",
            produce_boot_information(input, fixture.bytes, k_buffer_size, nullptr,
                                     k_work_entry_capacity, fixture.result),
            boot_information_producer_error_t::null_work_storage);
        passed &= expect_invalidated("null work invalidation", fixture);
        passed &= expect_u32("null work clears result size", fixture.result.total_size, 0);
        passed &= expect_u32("null work clears result count", fixture.result.memory_entry_count, 0);

        passed &= expect_error("insufficient work capacity", produce(input, fixture, k_buffer_size, 6),
                               boot_information_producer_error_t::insufficient_work_capacity);
        passed &= expect_invalidated("work capacity invalidation", fixture);
        passed &= expect_u32("work failure clears result size", fixture.result.total_size, 0);

        passed &= expect_error("exact work and output capacity", produce(input, fixture, 600, 7),
                               boot_information_producer_error_t::success);
        passed &= expect_u32("exact capacity result size", fixture.result.total_size, 600);

        passed &= expect_error("insufficient exact output", produce(input, fixture, 599),
                               boot_information_producer_error_t::output_too_small);
        passed &= expect_invalidated("output capacity invalidation", fixture);
        passed &= expect_u32("output failure clears result size", fixture.result.total_size, 0);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_canonical_fixture_test();
    passed &= run_type_mapping_test();
    passed &= run_coalescing_test();
    passed &= run_cross_descriptor_overlay_test();
    passed &= run_source_failure_tests();
    passed &= run_resource_failure_tests();
    passed &= run_capacity_and_atomicity_tests();

    if (!passed) return 1;

    std::puts("Warren boot-information producer tests passed.");
    return 0;
}
