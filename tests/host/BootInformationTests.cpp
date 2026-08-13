//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformationValidation.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::boot_information_error_t;
    using warren::boot::validate_boot_information;

    constexpr uint32_t k_buffer_size{ 1024 };
    constexpr uint64_t k_object_physical_address{ 0x00100000 };

    struct fixture_t
    {
        alignas(8) uint8_t bytes[k_buffer_size]{};
        uint32_t total_size{ 0 };
    };

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

    void write_section(
        fixture_t& fixture,
        uint32_t descriptor_offset,
        uint32_t section_offset,
        uint32_t count,
        uint32_t stride) noexcept
    {
        write_u32(fixture.bytes, descriptor_offset + 0x00, section_offset);
        write_u32(fixture.bytes, descriptor_offset + 0x04, count);
        write_u32(fixture.bytes, descriptor_offset + 0x08, stride);
        write_u32(fixture.bytes, descriptor_offset + 0x0c, 0);
    }

    void write_memory_entry(
        fixture_t& fixture,
        uint32_t offset,
        uint64_t physical_start,
        uint64_t page_count,
        uint32_t memory_kind) noexcept
    {
        write_u64(fixture.bytes, offset + 0x00, physical_start);
        write_u64(fixture.bytes, offset + 0x08, page_count);
        write_u32(fixture.bytes, offset + 0x10, memory_kind);
        write_u32(fixture.bytes, offset + 0x14, 1);
        write_u32(fixture.bytes, offset + 0x18, 7);
        write_u32(fixture.bytes, offset + 0x1c, 0);
        write_u64(fixture.bytes, offset + 0x20, 8);
    }

    fixture_t make_minimal_fixture() noexcept
    {
        fixture_t fixture{};
        fixture.total_size = 376;

        constexpr uint8_t magic[8]{ 'W', 'A', 'R', 'R', 'E', 'N', 'B', 'I' };
        for (uint32_t index{ 0 }; index < 8; ++index)
            fixture.bytes[index] = magic[index];

        write_u16(fixture.bytes, 0x008, 1);
        write_u16(fixture.bytes, 0x00a, 0);
        write_u32(fixture.bytes, 0x00c, 256);
        write_u32(fixture.bytes, 0x010, fixture.total_size);
        write_u32(fixture.bytes, 0x014, 4096);
        write_u64(fixture.bytes, 0x018, 0x0001);
        write_u64(fixture.bytes, 0x020, 0x0001);
        write_u64(fixture.bytes, 0x028, k_object_physical_address);
        write_u64(fixture.bytes, 0x030, 0x00200000);
        write_u64(fixture.bytes, 0x038, 0x00002000);
        write_u64(fixture.bytes, 0x040, 0x00100000);
        write_u64(fixture.bytes, 0x048, 0x00200000);
        write_u64(fixture.bytes, 0x050, 0x00300000);
        write_u64(fixture.bytes, 0x058, 0x00004000);

        write_section(fixture, 0x088, 0x100, 3, 40);
        write_memory_entry(fixture, 0x100, k_object_physical_address, 1, 3);
        write_memory_entry(fixture, 0x128, 0x00200000, 2, 4);
        write_memory_entry(fixture, 0x150, 0x00300000, 4, 5);
        return fixture;
    }

    fixture_t make_representative_fixture() noexcept
    {
        fixture_t fixture{ make_minimal_fixture() };
        fixture.total_size = 560;
        write_u32(fixture.bytes, 0x010, fixture.total_size);
        write_u64(fixture.bytes, 0x018, 0x007f);

        write_u64(fixture.bytes, 0x060, 0x00400000);
        write_u64(fixture.bytes, 0x068, 0x00001000);
        write_u64(fixture.bytes, 0x070, 0x00600000);
        write_u64(fixture.bytes, 0x078, 0x00500000);
        write_u64(fixture.bytes, 0x080, 0x00001000);

        write_section(fixture, 0x088, 0x100, 4, 40);
        write_memory_entry(fixture, 0x178, 0x00400000, 1, 6);

        constexpr uint8_t command_line[]{
            'c', 'o', 'n', 's', 'o', 'l', 'e', '=', 'p', 'l', '0', '1', '1'
        };
        write_section(fixture, 0x098, 0x1a0, 13, 1);
        for (uint32_t index{ 0 }; index < 13; ++index)
            fixture.bytes[0x1a0 + index] = command_line[index];

        write_section(fixture, 0x0a8, 0x1b0, 1, 64);
        write_u32(fixture.bytes, 0x1b0 + 0x00, 1);
        write_u32(fixture.bytes, 0x1b0 + 0x04, 3);
        write_u64(fixture.bytes, 0x1b0 + 0x08, 0x09000000);
        write_u32(fixture.bytes, 0x1b0 + 0x10, 4);
        write_u32(fixture.bytes, 0x1b0 + 0x14, 32);
        write_u64(fixture.bytes, 0x1b0 + 0x18, 24000000);
        write_u32(fixture.bytes, 0x1b0 + 0x20, 115200);

        write_section(fixture, 0x0b8, 0x1f0, 1, 64);
        write_u64(fixture.bytes, 0x1f0 + 0x00, 0x10000000);
        write_u64(fixture.bytes, 0x1f0 + 0x08, 1920000);
        write_u32(fixture.bytes, 0x1f0 + 0x10, 800);
        write_u32(fixture.bytes, 0x1f0 + 0x14, 600);
        write_u32(fixture.bytes, 0x1f0 + 0x18, 800);
        write_u32(fixture.bytes, 0x1f0 + 0x1c, 2);
        return fixture;
    }

    bool expect_result(
        const char* name,
        const fixture_t& fixture,
        boot_information_error_t expected,
        uint32_t readable_size = 0) noexcept
    {
        const uint32_t effective_size{
            readable_size == 0 ? fixture.total_size : readable_size
        };
        const boot_information_error_t actual{ validate_boot_information(
            fixture.bytes,
            effective_size,
            k_object_physical_address) };
        if (actual == expected)
            return true;

        std::fprintf(
            stderr,
            "FAIL: %s: expected result %u, received %u\n",
            name,
            static_cast<uint32_t>(expected),
            static_cast<uint32_t>(actual));
        return false;
    }

    bool expect_direct_result(
        const char* name,
        const void* object,
        uint32_t readable_size,
        uint64_t physical_address,
        boot_information_error_t expected) noexcept
    {
        const boot_information_error_t actual{ validate_boot_information(
            object,
            readable_size,
            physical_address) };
        if (actual == expected)
            return true;

        std::fprintf(
            stderr,
            "FAIL: %s: expected result %u, received %u\n",
            name,
            static_cast<uint32_t>(expected),
            static_cast<uint32_t>(actual));
        return false;
    }

    bool run_core_header_tests() noexcept
    {
        bool passed{ true };
        passed &= expect_result(
            "minimal object",
            make_minimal_fixture(),
            boot_information_error_t::success);
        passed &= expect_result(
            "representative object",
            make_representative_fixture(),
            boot_information_error_t::success);

        fixture_t fixture{ make_minimal_fixture() };
        write_u16(fixture.bytes, 0x00a, 1);
        write_u64(fixture.bytes, 0x018, UINT64_C(1) << 63U | 1U);
        passed &= expect_result(
            "compatible greater minor with unknown optional feature",
            fixture,
            boot_information_error_t::success);

        fixture = make_minimal_fixture();
        for (uint32_t index{ 120 }; index > 0; --index)
            fixture.bytes[0x108 + index - 1] = fixture.bytes[0x100 + index - 1];
        for (uint32_t index{ 0 }; index < 8; ++index)
            fixture.bytes[0x100 + index] = 0xa5;
        fixture.total_size = 384;
        write_u16(fixture.bytes, 0x00a, 1);
        write_u32(fixture.bytes, 0x00c, 264);
        write_u32(fixture.bytes, 0x010, fixture.total_size);
        write_u32(fixture.bytes, 0x088, 264);
        passed &= expect_result(
            "compatible extended minor header",
            fixture,
            boot_information_error_t::success);

        passed &= expect_direct_result(
            "null object",
            nullptr,
            256,
            k_object_physical_address,
            boot_information_error_t::null_object);
        fixture = make_minimal_fixture();
        passed &= expect_direct_result(
            "unaligned object",
            fixture.bytes + 1,
            fixture.total_size - 1,
            k_object_physical_address,
            boot_information_error_t::unaligned_object);
        passed &= expect_result(
            "truncated fixed header",
            fixture,
            boot_information_error_t::truncated_fixed_header,
            255);

        fixture = make_minimal_fixture();
        fixture.bytes[0] = 'X';
        passed &= expect_result(
            "invalid magic", fixture, boot_information_error_t::invalid_magic);

        fixture = make_minimal_fixture();
        write_u16(fixture.bytes, 0x008, 2);
        passed &= expect_result(
            "unsupported major", fixture, boot_information_error_t::unsupported_major);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x00c, 248);
        passed &= expect_result(
            "short header size", fixture, boot_information_error_t::invalid_header_size);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x010, 377);
        fixture.total_size = 377;
        passed &= expect_result(
            "unaligned total size", fixture, boot_information_error_t::invalid_total_size);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x010, fixture.total_size + 8);
        passed &= expect_result(
            "object exceeds readable bound",
            fixture,
            boot_information_error_t::invalid_total_size);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x014, 16384);
        passed &= expect_result(
            "wrong page unit", fixture, boot_information_error_t::invalid_page_size);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x028, k_object_physical_address + 8);
        passed &= expect_result(
            "self address mismatch",
            fixture,
            boot_information_error_t::self_address_mismatch);

        fixture = make_minimal_fixture();
        fixture.bytes[0x0c8] = 1;
        passed &= expect_result(
            "nonzero fixed reserved byte",
            fixture,
            boot_information_error_t::nonzero_reserved);
        return passed;
    }

    bool run_feature_and_resource_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_minimal_fixture() };
        write_u64(fixture.bytes, 0x020, 3);
        passed &= expect_result(
            "required feature is not present",
            fixture,
            boot_information_error_t::invalid_feature_masks);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x018, UINT64_C(1) << 63U | 1U);
        write_u64(fixture.bytes, 0x020, UINT64_C(1) << 63U | 1U);
        passed &= expect_result(
            "unknown required feature",
            fixture,
            boot_information_error_t::unsupported_required_feature);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x018, 0);
        write_u64(fixture.bytes, 0x020, 0);
        passed &= expect_result(
            "missing mandatory memory map",
            fixture,
            boot_information_error_t::missing_memory_map);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x030, 0x00200001);
        passed &= expect_result(
            "unaligned kernel",
            fixture,
            boot_information_error_t::invalid_kernel_range);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x030, UINT64_C(0xfffffffffffff000));
        write_u64(fixture.bytes, 0x038, 0x2000);
        passed &= expect_result(
            "overflowing kernel extent",
            fixture,
            boot_information_error_t::invalid_kernel_range);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x040, 0x00300000);
        passed &= expect_result(
            "load bias exceeds kernel runtime address",
            fixture,
            boot_information_error_t::invalid_kernel_range);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x048, 0x00202000);
        passed &= expect_result(
            "entry outside kernel",
            fixture,
            boot_information_error_t::invalid_entry_address);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x058, 0);
        passed &= expect_result(
            "empty bootstrap stack",
            fixture,
            boot_information_error_t::invalid_stack_range);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x050, 0x00200000);
        passed &= expect_result(
            "overlapping kernel and stack",
            fixture,
            boot_information_error_t::overlapping_physical_resources);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x060, 0x00400000);
        passed &= expect_result(
            "absent initial image has fields",
            fixture,
            boot_information_error_t::invalid_optional_resource);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x018, 0x0005);
        write_u64(fixture.bytes, 0x060, 0x00400000);
        passed &= expect_result(
            "present initial image has no size",
            fixture,
            boot_information_error_t::invalid_optional_resource);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x018, 0x0021);
        write_u64(fixture.bytes, 0x070, 0x00600008);
        passed &= expect_result(
            "misaligned ACPI RSDP",
            fixture,
            boot_information_error_t::invalid_optional_resource);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x018, 0x0041);
        write_u64(fixture.bytes, 0x078, UINT64_C(0xfffffffffffffff8));
        write_u64(fixture.bytes, 0x080, 16);
        passed &= expect_result(
            "overflowing device-tree extent",
            fixture,
            boot_information_error_t::invalid_optional_resource);
        return passed;
    }

    bool run_section_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_minimal_fixture() };
        write_u32(fixture.bytes, 0x088, 257);
        passed &= expect_result(
            "unaligned memory-map section",
            fixture,
            boot_information_error_t::invalid_section_descriptor);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x08c, UINT32_MAX);
        passed &= expect_result(
            "oversized memory-map count",
            fixture,
            boot_information_error_t::section_out_of_bounds);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x094, 1);
        passed &= expect_result(
            "nonzero descriptor reserved field",
            fixture,
            boot_information_error_t::invalid_section_descriptor);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x098, 256);
        passed &= expect_result(
            "absent section has descriptor data",
            fixture,
            boot_information_error_t::invalid_section_descriptor);

        fixture = make_representative_fixture();
        write_u32(fixture.bytes, 0x098, 256);
        passed &= expect_result(
            "overlapping contained sections",
            fixture,
            boot_information_error_t::overlapping_sections);

        fixture = make_representative_fixture();
        fixture.bytes[0x1ad] = 1;
        passed &= expect_result(
            "nonzero alignment padding",
            fixture,
            boot_information_error_t::nonzero_padding);
        return passed;
    }

    bool run_record_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_minimal_fixture() };
        write_u64(fixture.bytes, 0x128, k_object_physical_address);
        passed &= expect_result(
            "unordered memory map",
            fixture,
            boot_information_error_t::invalid_memory_map);

        fixture = make_minimal_fixture();
        write_u64(fixture.bytes, 0x108, 0);
        passed &= expect_result(
            "zero-page memory entry",
            fixture,
            boot_information_error_t::invalid_memory_map);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x100 + 0x14, 2);
        passed &= expect_result(
            "unknown memory source namespace",
            fixture,
            boot_information_error_t::unsupported_memory_source);

        fixture = make_minimal_fixture();
        write_u32(fixture.bytes, 0x128 + 0x10, 0);
        passed &= expect_result(
            "kernel absent from normalized map",
            fixture,
            boot_information_error_t::missing_resource_coverage);

        fixture = make_representative_fixture();
        fixture.bytes[0x1a0] = 0xff;
        passed &= expect_result(
            "invalid command-line UTF-8",
            fixture,
            boot_information_error_t::invalid_utf8);

        fixture = make_representative_fixture();
        write_u32(fixture.bytes, 0x1b0 + 0x10, 8);
        passed &= expect_result(
            "invalid PL011 record",
            fixture,
            boot_information_error_t::invalid_console);

        fixture = make_representative_fixture();
        write_u64(fixture.bytes, 0x1f0 + 0x08, 1024);
        passed &= expect_result(
            "undersized framebuffer",
            fixture,
            boot_information_error_t::invalid_framebuffer);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_core_header_tests();
    passed &= run_feature_and_resource_tests();
    passed &= run_section_tests();
    passed &= run_record_tests();

    if (!passed)
        return 1;

    std::puts("Warren boot-information protocol tests passed.");
    return 0;
}
