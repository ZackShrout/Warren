//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BurrowLoader.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::burrow_load_error_name;
    using warren::boot::burrow_load_error_t;
    using warren::boot::burrow_load_plan_t;
    using warren::boot::burrow_loaded_image_t;
    using warren::boot::byte_source_t;
    using warren::boot::materialize_burrow_image;
    using warren::boot::plan_burrow_image;

    constexpr uint64_t k_file_size{ 0x3000 };
    constexpr uint64_t k_program_offset{ 0x40 };
    constexpr uint64_t k_program_size{ 0x38 };
    constexpr uint64_t k_text_offset{ 0x1000 };
    constexpr uint64_t k_data_offset{ 0x2000 };
    constexpr uint64_t k_dynamic_offset{ 0x2020 };
    constexpr uint64_t k_relocation_offset{ 0x200 };
    constexpr uint64_t k_allocation_size{ 0x3000 };
    constexpr uint64_t k_artifact_destination_capacity{ 1024 * 1024 };

    alignas(4096) uint8_t g_artifact_destination[
        k_artifact_destination_capacity]{};

    constexpr uint64_t program_offset(uint64_t index) noexcept
    {
        return k_program_offset + index * k_program_size;
    }

    struct fixture_t
    {
        uint8_t bytes[k_file_size]{};
        uint64_t dynamic_size{ 0 };
        uint64_t relocation_count{ 0 };
    };

    struct source_context_t
    {
        const uint8_t* bytes;
        uint64_t byte_count;
        bool fail_reads;
        uint64_t fail_begin;
        uint64_t fail_end;
    };

    void write_u16(uint8_t* bytes, uint64_t offset, uint16_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 2; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8U));
    }

    void write_u32(uint8_t* bytes, uint64_t offset, uint32_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 4; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8U));
    }

    void write_u64(uint8_t* bytes, uint64_t offset, uint64_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 8; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8U));
    }

    [[nodiscard]] uint64_t read_u64(
        const uint8_t* bytes,
        uint64_t offset) noexcept
    {
        uint64_t value{ 0 };
        for (uint32_t index{ 0 }; index < 8; ++index)
            value |= static_cast<uint64_t>(bytes[offset + index]) << (index * 8U);
        return value;
    }

    void write_program(
        fixture_t& fixture,
        uint64_t index,
        uint32_t type,
        uint32_t flags,
        uint64_t offset,
        uint64_t virtual_address,
        uint64_t file_size,
        uint64_t memory_size,
        uint64_t alignment) noexcept
    {
        const uint64_t output{ program_offset(index) };
        write_u32(fixture.bytes, output, type);
        write_u32(fixture.bytes, output + 4, flags);
        write_u64(fixture.bytes, output + 8, offset);
        write_u64(fixture.bytes, output + 16, virtual_address);
        write_u64(fixture.bytes, output + 24, virtual_address);
        write_u64(fixture.bytes, output + 32, file_size);
        write_u64(fixture.bytes, output + 40, memory_size);
        write_u64(fixture.bytes, output + 48, alignment);
    }

    void write_dynamic_entry(
        fixture_t& fixture,
        uint64_t index,
        uint64_t tag,
        uint64_t value) noexcept
    {
        const uint64_t offset{ k_dynamic_offset + index * 16 };
        write_u64(fixture.bytes, offset, tag);
        write_u64(fixture.bytes, offset + 8, value);
    }

    void write_relocation(
        fixture_t& fixture,
        uint64_t index,
        uint64_t target,
        uint32_t type,
        uint32_t symbol,
        uint64_t addend) noexcept
    {
        const uint64_t offset{ k_relocation_offset + index * 24 };
        write_u64(fixture.bytes, offset, target);
        write_u64(
            fixture.bytes,
            offset + 8,
            static_cast<uint64_t>(symbol) << 32U | type);
        write_u64(fixture.bytes, offset + 16, addend);
    }

    [[nodiscard]] fixture_t make_fixture(uint64_t relocation_count = 0) noexcept
    {
        fixture_t fixture{};
        fixture.relocation_count = relocation_count;

        fixture.bytes[0] = 0x7f;
        fixture.bytes[1] = 'E';
        fixture.bytes[2] = 'L';
        fixture.bytes[3] = 'F';
        fixture.bytes[4] = 2;
        fixture.bytes[5] = 1;
        fixture.bytes[6] = 1;
        write_u16(fixture.bytes, 16, 3);
        write_u16(fixture.bytes, 18, 183);
        write_u32(fixture.bytes, 20, 1);
        write_u64(fixture.bytes, 24, k_text_offset);
        write_u64(fixture.bytes, 32, k_program_offset);
        write_u16(fixture.bytes, 52, 64);
        write_u16(fixture.bytes, 54, 56);
        write_u16(fixture.bytes, 56, 4);
        write_u16(fixture.bytes, 58, 64);

        uint64_t dynamic_entry_count{ 6 };
        if (relocation_count != 0)
            dynamic_entry_count += 3;
        fixture.dynamic_size = dynamic_entry_count * 16;

        write_program(fixture, 0, 1, 4, 0, 0, 0x300, 0x300, 0x1000);
        write_program(
            fixture,
            1,
            1,
            5,
            k_text_offset,
            k_text_offset,
            0x20,
            0x20,
            0x1000);
        write_program(
            fixture,
            2,
            1,
            6,
            k_data_offset,
            k_data_offset,
            k_dynamic_offset + fixture.dynamic_size - k_data_offset,
            0x300,
            0x1000);
        write_program(
            fixture,
            3,
            2,
            6,
            k_dynamic_offset,
            k_dynamic_offset,
            fixture.dynamic_size,
            fixture.dynamic_size,
            8);

        write_dynamic_entry(fixture, 0, 6, 0x190);
        write_dynamic_entry(fixture, 1, 11, 24);
        write_dynamic_entry(fixture, 2, 5, 0x1c4);
        write_dynamic_entry(fixture, 3, 10, 1);
        write_dynamic_entry(fixture, 4, 0x6ffffef5, 0x1a8);
        uint64_t dynamic_index{ 5 };
        if (relocation_count != 0)
        {
            write_dynamic_entry(
                fixture,
                dynamic_index++,
                7,
                k_relocation_offset);
            write_dynamic_entry(
                fixture,
                dynamic_index++,
                8,
                relocation_count * 24);
            write_dynamic_entry(fixture, dynamic_index++, 9, 24);
        }
        write_dynamic_entry(fixture, dynamic_index, 0, 0);

        for (uint64_t index{ 0 }; index < 0x20; ++index)
            fixture.bytes[k_text_offset + index] =
                static_cast<uint8_t>(0x80 + index);
        for (uint64_t index{ 0 }; index < 0x20; ++index)
            fixture.bytes[k_data_offset + index] =
                static_cast<uint8_t>(0x40 + index);

        for (uint64_t index{ 0 }; index < relocation_count; ++index)
        {
            write_relocation(
                fixture,
                index,
                k_data_offset + index * 8,
                1027,
                0,
                k_text_offset + index * 8);
        }
        return fixture;
    }

    [[nodiscard]] bool source_read(
        void* opaque_context,
        uint64_t offset,
        uint8_t* destination,
        uint64_t byte_count) noexcept
    {
        auto& context{ *static_cast<source_context_t*>(opaque_context) };
        if (offset > context.byte_count ||
            byte_count > context.byte_count - offset)
            return false;

        if (context.fail_reads && offset < context.fail_end &&
            context.fail_begin < offset + byte_count)
            return false;

        for (uint64_t index{ 0 }; index < byte_count; ++index)
            destination[index] = context.bytes[offset + index];
        return true;
    }

    [[nodiscard]] bool file_source_read(
        void* opaque_context,
        uint64_t offset,
        uint8_t* destination,
        uint64_t byte_count) noexcept
    {
        auto* file{ static_cast<std::FILE*>(opaque_context) };
        if (file == nullptr || offset > static_cast<uint64_t>(LONG_MAX) ||
            byte_count > static_cast<uint64_t>(SIZE_MAX))
            return false;
        if (std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0)
            return false;

        return std::fread(
                   destination,
                   1,
                   static_cast<size_t>(byte_count),
                   file) == byte_count;
    }

    [[nodiscard]] byte_source_t make_source(
        const fixture_t& fixture,
        source_context_t& context) noexcept
    {
        context = { fixture.bytes, k_file_size, false, 0, 0 };
        return { &context, k_file_size, source_read };
    }

    [[nodiscard]] bool expect_plan_result(
        const char* name,
        const fixture_t& fixture,
        burrow_load_error_t expected,
        uint64_t source_size = k_file_size,
        uint64_t fail_begin = 0,
        uint64_t fail_end = 0) noexcept
    {
        source_context_t context{
            fixture.bytes,
            source_size,
            fail_end > fail_begin,
            fail_begin,
            fail_end
        };
        const byte_source_t source{ &context, source_size, source_read };
        burrow_load_plan_t plan{};
        const burrow_load_error_t actual{ plan_burrow_image(source, plan) };
        if (actual == expected)
            return true;

        std::fprintf(
            stderr,
            "FAIL: %s: expected %s, received %s\n",
            name,
            burrow_load_error_name(expected),
            burrow_load_error_name(actual));
        return false;
    }

    [[nodiscard]] bool expect_true(const char* name, bool condition) noexcept
    {
        if (condition)
            return true;
        std::fprintf(stderr, "FAIL: %s\n", name);
        return false;
    }

    [[nodiscard]] bool run_positive_plan_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        source_context_t context{};
        const byte_source_t source{ make_source(fixture, context) };
        burrow_load_plan_t plan{};
        passed &= expect_true(
            "zero-relocation plan accepted",
            plan_burrow_image(source, plan) == burrow_load_error_t::success);
        passed &= expect_true(
            "plan records source and allocation sizes",
            plan.source_size == k_file_size &&
            plan.allocation_size == k_allocation_size);
        passed &= expect_true(
            "plan canonicalizes segment classes",
            plan.segments[0].virtual_address == 0 &&
            plan.segments[1].virtual_address == k_text_offset &&
            plan.segments[2].virtual_address == k_data_offset);
        passed &= expect_true(
            "plan records entry and dynamic table",
            plan.entry_virtual_address == k_text_offset &&
            plan.dynamic_virtual_address == k_dynamic_offset &&
            plan.dynamic_size == fixture.dynamic_size);
        passed &= expect_true(
            "zero-relocation plan is explicit",
            plan.relocations.entry_count == 0 &&
            plan.relocations.byte_count == 0);

        fixture = make_fixture(2);
        const byte_source_t relocated_source{ make_source(fixture, context) };
        passed &= expect_true(
            "multiple relative relocations accepted",
            plan_burrow_image(relocated_source, plan) ==
                burrow_load_error_t::success &&
            plan.relocations.entry_count == 2 &&
            plan.relocations.file_offset == k_relocation_offset);

        fixture = make_fixture(1);
        const byte_source_t single_source{ make_source(fixture, context) };
        passed &= expect_true(
            "one relative relocation accepted",
            plan_burrow_image(single_source, plan) ==
                burrow_load_error_t::success &&
            plan.relocations.entry_count == 1);
        return passed;
    }

    [[nodiscard]] bool run_materialization_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture(2) };
        source_context_t context{};
        const byte_source_t source{ make_source(fixture, context) };
        burrow_load_plan_t plan{};
        passed &= expect_true(
            "materialization fixture plans",
            plan_burrow_image(source, plan) == burrow_load_error_t::success);

        alignas(4096) uint8_t destination[k_allocation_size];
        for (uint64_t index{ 0 }; index < sizeof(destination); ++index)
            destination[index] = 0xa5;
        burrow_loaded_image_t image{};
        constexpr uint64_t physical_start{ 0x00400000 };
        passed &= expect_true(
            "materialization succeeds",
            materialize_burrow_image(
                source,
                plan,
                physical_start,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::success);
        passed &= expect_true(
            "materialization returns physical result",
            image.physical_start == physical_start &&
            image.physical_size == sizeof(destination) &&
            image.load_bias == physical_start &&
            image.entry_physical_address == physical_start + k_text_offset);
        passed &= expect_true(
            "first relocation applied",
            read_u64(destination, k_data_offset) ==
                physical_start + k_text_offset);
        passed &= expect_true(
            "second relocation applied",
            read_u64(destination, k_data_offset + 8) ==
                physical_start + k_text_offset + 8);
        passed &= expect_true(
            "text bytes copied",
            destination[k_text_offset] == fixture.bytes[k_text_offset] &&
            destination[k_text_offset + 0x1f] ==
                fixture.bytes[k_text_offset + 0x1f]);
        passed &= expect_true(
            "BSS and allocation gaps remain zero",
            destination[k_data_offset + 0x200] == 0 &&
            destination[0x1800] == 0 &&
            destination[k_allocation_size - 1] == 0);

        write_relocation(
            fixture,
            0,
            k_data_offset,
            1,
            0,
            k_text_offset);
        passed &= expect_true(
            "materialization revalidates changed relocation bytes",
            materialize_burrow_image(
                source,
                plan,
                physical_start,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::invalid_relocation_type);

        fixture = make_fixture();
        const byte_source_t zero_source{ make_source(fixture, context) };
        passed &= expect_true(
            "zero-relocation fixture plans",
            plan_burrow_image(zero_source, plan) ==
                burrow_load_error_t::success);
        passed &= expect_true(
            "second injected physical base succeeds",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800000,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::success &&
            image.entry_physical_address == 0x00801000);

        passed &= expect_true(
            "null destination rejected",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800000,
                nullptr,
                sizeof(destination),
                image) == burrow_load_error_t::invalid_destination);
        passed &= expect_true(
            "undersized destination rejected",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800000,
                destination,
                sizeof(destination) - 4096,
                image) == burrow_load_error_t::invalid_destination);
        passed &= expect_true(
            "unaligned destination rejected",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800000,
                destination + 1,
                sizeof(destination),
                image) == burrow_load_error_t::invalid_destination);
        passed &= expect_true(
            "unaligned physical base rejected",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800001,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::invalid_load_bias);
        passed &= expect_true(
            "overflowing physical extent rejected",
            materialize_burrow_image(
                zero_source,
                plan,
                UINT64_MAX - 0xfff,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::physical_extent_overflow);

        context.fail_reads = true;
        context.fail_begin = k_text_offset;
        context.fail_end = k_text_offset + 1;
        passed &= expect_true(
            "load-byte read failure is reported",
            materialize_burrow_image(
                zero_source,
                plan,
                0x00800000,
                destination,
                sizeof(destination),
                image) == burrow_load_error_t::source_read_failed);
        return passed;
    }

    [[nodiscard]] bool run_header_rejection_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        fixture.bytes[0] = 0;
        passed &= expect_plan_result(
            "ELF magic",
            fixture,
            burrow_load_error_t::invalid_elf_magic);
        fixture = make_fixture();
        fixture.bytes[4] = 1;
        passed &= expect_plan_result(
            "ELF class",
            fixture,
            burrow_load_error_t::unsupported_elf_class);
        fixture = make_fixture();
        fixture.bytes[5] = 2;
        passed &= expect_plan_result(
            "byte order",
            fixture,
            burrow_load_error_t::unsupported_byte_order);
        fixture = make_fixture();
        fixture.bytes[6] = 0;
        passed &= expect_plan_result(
            "identification version",
            fixture,
            burrow_load_error_t::invalid_elf_version);
        fixture = make_fixture();
        fixture.bytes[7] = 3;
        passed &= expect_plan_result(
            "OS ABI",
            fixture,
            burrow_load_error_t::unsupported_elf_abi);
        fixture = make_fixture();
        write_u16(fixture.bytes, 16, 2);
        passed &= expect_plan_result(
            "ELF type",
            fixture,
            burrow_load_error_t::unsupported_elf_type);
        fixture = make_fixture();
        write_u16(fixture.bytes, 18, 62);
        passed &= expect_plan_result(
            "machine",
            fixture,
            burrow_load_error_t::unsupported_machine);
        fixture = make_fixture();
        write_u32(fixture.bytes, 48, 1);
        passed &= expect_plan_result(
            "machine flags",
            fixture,
            burrow_load_error_t::invalid_machine_flags);
        fixture = make_fixture();
        write_u16(fixture.bytes, 52, 63);
        passed &= expect_plan_result(
            "ELF header size",
            fixture,
            burrow_load_error_t::invalid_elf_header_size);
        fixture = make_fixture();
        write_u16(fixture.bytes, 54, 55);
        passed &= expect_plan_result(
            "program header size",
            fixture,
            burrow_load_error_t::invalid_program_header_size);
        fixture = make_fixture();
        write_u16(fixture.bytes, 56, 0);
        passed &= expect_plan_result(
            "empty program table",
            fixture,
            burrow_load_error_t::invalid_program_table);
        fixture = make_fixture();
        write_u16(fixture.bytes, 56, 17);
        passed &= expect_plan_result(
            "program count capacity",
            fixture,
            burrow_load_error_t::too_many_program_headers);
        fixture = make_fixture();
        write_u64(fixture.bytes, 32, UINT64_MAX - 16);
        passed &= expect_plan_result(
            "program table overflow",
            fixture,
            burrow_load_error_t::invalid_program_table);
        fixture = make_fixture();
        passed &= expect_plan_result(
            "ELF header source failure",
            fixture,
            burrow_load_error_t::source_read_failed,
            k_file_size,
            0,
            1);
        fixture = make_fixture();
        passed &= expect_plan_result(
            "program header source failure",
            fixture,
            burrow_load_error_t::source_read_failed,
            k_file_size,
            k_program_offset,
            k_program_offset + 1);
        return passed;
    }

    [[nodiscard]] bool run_segment_rejection_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        write_u32(fixture.bytes, program_offset(1), 4);
        passed &= expect_plan_result(
            "unsupported program type",
            fixture,
            burrow_load_error_t::unsupported_program_type);
        fixture = make_fixture();
        write_u32(fixture.bytes, program_offset(1) + 4, 7);
        passed &= expect_plan_result(
            "writable executable load",
            fixture,
            burrow_load_error_t::invalid_load_permissions);
        fixture = make_fixture();
        write_u32(fixture.bytes, program_offset(1) + 4, 4);
        passed &= expect_plan_result(
            "duplicate load class",
            fixture,
            burrow_load_error_t::invalid_load_permissions);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 32, 0);
        passed &= expect_plan_result(
            "empty load",
            fixture,
            burrow_load_error_t::invalid_load_size);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 32, 0x30);
        passed &= expect_plan_result(
            "file larger than memory",
            fixture,
            burrow_load_error_t::invalid_load_size);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 48, 0x2000);
        passed &= expect_plan_result(
            "load alignment",
            fixture,
            burrow_load_error_t::invalid_load_alignment);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 8, 0x1008);
        write_u64(fixture.bytes, program_offset(1) + 16, 0x1008);
        write_u64(fixture.bytes, program_offset(1) + 24, 0x1008);
        passed &= expect_plan_result(
            "non-page-aligned load start",
            fixture,
            burrow_load_error_t::invalid_load_alignment);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 8, 0x1008);
        passed &= expect_plan_result(
            "load congruence",
            fixture,
            burrow_load_error_t::invalid_load_congruence);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 8, k_file_size);
        passed &= expect_plan_result(
            "load file bounds",
            fixture,
            burrow_load_error_t::invalid_load_file_range);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 16, 0x80000000);
        write_u64(fixture.bytes, program_offset(1) + 24, 0x80000000);
        passed &= expect_plan_result(
            "load address limit",
            fixture,
            burrow_load_error_t::invalid_load_address_range);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 24, 0x5000);
        passed &= expect_plan_result(
            "physical and virtual mismatch",
            fixture,
            burrow_load_error_t::invalid_load_physical_address);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 16, 0);
        write_u64(fixture.bytes, program_offset(1) + 24, 0);
        passed &= expect_plan_result(
            "load page overlap",
            fixture,
            burrow_load_error_t::overlapping_load_segments);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(1) + 8, 0);
        passed &= expect_plan_result(
            "load file page overlap",
            fixture,
            burrow_load_error_t::overlapping_load_segments);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(0) + 32, 0x100);
        passed &= expect_plan_result(
            "program table outside first load",
            fixture,
            burrow_load_error_t::invalid_load_span);
        fixture = make_fixture();
        write_u32(fixture.bytes, program_offset(1), 0);
        passed &= expect_plan_result(
            "missing load segment",
            fixture,
            burrow_load_error_t::invalid_load_segment_count);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(0) + 16, 0x3000);
        write_u64(fixture.bytes, program_offset(0) + 24, 0x3000);
        passed &= expect_plan_result(
            "load span must begin at zero",
            fixture,
            burrow_load_error_t::invalid_load_span);
        fixture = make_fixture();
        write_u64(fixture.bytes, 24, k_data_offset);
        passed &= expect_plan_result(
            "entry must be executable",
            fixture,
            burrow_load_error_t::invalid_entry);
        return passed;
    }

    [[nodiscard]] bool run_dynamic_rejection_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture() };
        write_u32(fixture.bytes, program_offset(3), 0);
        passed &= expect_plan_result(
            "missing dynamic segment",
            fixture,
            burrow_load_error_t::missing_dynamic_segment);
        fixture = make_fixture();
        write_u16(fixture.bytes, 56, 5);
        write_program(
            fixture,
            4,
            2,
            6,
            k_dynamic_offset,
            k_dynamic_offset,
            fixture.dynamic_size,
            fixture.dynamic_size,
            8);
        passed &= expect_plan_result(
            "duplicate dynamic segment",
            fixture,
            burrow_load_error_t::duplicate_dynamic_segment);
        fixture = make_fixture();
        write_u32(fixture.bytes, program_offset(3) + 4, 4);
        passed &= expect_plan_result(
            "dynamic permissions",
            fixture,
            burrow_load_error_t::invalid_dynamic_segment);
        fixture = make_fixture();
        write_u64(fixture.bytes, program_offset(3) + 40, fixture.dynamic_size + 16);
        passed &= expect_plan_result(
            "dynamic file and memory sizes",
            fixture,
            burrow_load_error_t::invalid_dynamic_segment);
        fixture = make_fixture();
        write_dynamic_entry(fixture, 5, 6, 0x190);
        passed &= expect_plan_result(
            "duplicate dynamic tag",
            fixture,
            burrow_load_error_t::duplicate_dynamic_tag);
        fixture = make_fixture();
        write_dynamic_entry(fixture, 0, 1, 0);
        passed &= expect_plan_result(
            "forbidden dynamic request",
            fixture,
            burrow_load_error_t::forbidden_dynamic_tag);
        fixture = make_fixture();
        write_dynamic_entry(fixture, 0, 0x70000001, 0);
        passed &= expect_plan_result(
            "unsupported dynamic request",
            fixture,
            burrow_load_error_t::unsupported_dynamic_tag);
        fixture = make_fixture();
        write_dynamic_entry(fixture, 5, 6, 0x190);
        write_u64(fixture.bytes, program_offset(3) + 32, 5 * 16);
        write_u64(fixture.bytes, program_offset(3) + 40, 5 * 16);
        passed &= expect_plan_result(
            "unterminated dynamic table",
            fixture,
            burrow_load_error_t::unterminated_dynamic_table);
        fixture = make_fixture();
        passed &= expect_plan_result(
            "dynamic table source failure",
            fixture,
            burrow_load_error_t::source_read_failed,
            k_file_size,
            k_dynamic_offset,
            k_dynamic_offset + 1);
        return passed;
    }

    [[nodiscard]] bool run_relocation_rejection_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{ make_fixture(1) };
        write_dynamic_entry(fixture, 7, 0, 0);
        passed &= expect_plan_result(
            "incomplete relocation metadata",
            fixture,
            burrow_load_error_t::incomplete_relocation_table);
        fixture = make_fixture(1);
        write_dynamic_entry(fixture, 6, 8, 25);
        passed &= expect_plan_result(
            "relocation table size",
            fixture,
            burrow_load_error_t::invalid_relocation_table_size);
        fixture = make_fixture(1);
        write_dynamic_entry(
            fixture,
            6,
            8,
            (warren::boot::k_burrow_maximum_relocations + 1ULL) * 24ULL);
        passed &= expect_plan_result(
            "relocation count capacity",
            fixture,
            burrow_load_error_t::invalid_relocation_table_size);
        fixture = make_fixture(1);
        write_dynamic_entry(fixture, 7, 9, 16);
        passed &= expect_plan_result(
            "relocation entry width",
            fixture,
            burrow_load_error_t::invalid_relocation_entry_size);
        fixture = make_fixture(1);
        write_dynamic_entry(fixture, 5, 7, 0x1800);
        passed &= expect_plan_result(
            "relocation table outside file-backed loads",
            fixture,
            burrow_load_error_t::invalid_relocation_table_range);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_data_offset, 1, 0, k_text_offset);
        passed &= expect_plan_result(
            "relocation type",
            fixture,
            burrow_load_error_t::invalid_relocation_type);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_data_offset, 1027, 1, k_text_offset);
        passed &= expect_plan_result(
            "relocation symbol",
            fixture,
            burrow_load_error_t::invalid_relocation_symbol);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_data_offset, 1027, 0, UINT64_MAX);
        passed &= expect_plan_result(
            "negative relocation addend",
            fixture,
            burrow_load_error_t::invalid_relocation_addend);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_data_offset, 1027, 0, 0x1800);
        passed &= expect_plan_result(
            "relocation addend outside loaded storage",
            fixture,
            burrow_load_error_t::invalid_relocation_addend);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_data_offset + 1, 1027, 0, k_text_offset);
        passed &= expect_plan_result(
            "unaligned relocation target",
            fixture,
            burrow_load_error_t::invalid_relocation_target);
        fixture = make_fixture(1);
        write_relocation(fixture, 0, k_text_offset, 1027, 0, k_text_offset);
        passed &= expect_plan_result(
            "relocation target outside writable storage",
            fixture,
            burrow_load_error_t::invalid_relocation_target);
        fixture = make_fixture(2);
        write_relocation(fixture, 1, k_data_offset, 1027, 0, k_text_offset);
        passed &= expect_plan_result(
            "duplicate relocation target",
            fixture,
            burrow_load_error_t::duplicate_relocation_target);
        fixture = make_fixture(1);
        passed &= expect_plan_result(
            "relocation table source failure",
            fixture,
            burrow_load_error_t::source_read_failed,
            k_file_size,
            k_relocation_offset,
            k_relocation_offset + 1);
        return passed;
    }

    [[nodiscard]] bool run_generated_artifact_test(const char* path) noexcept
    {
        std::FILE* file{ std::fopen(path, "rb") };
        if (file == nullptr)
        {
            std::fprintf(stderr, "FAIL: cannot open generated artifact: %s\n", path);
            return false;
        }
        if (std::fseek(file, 0, SEEK_END) != 0)
        {
            std::fclose(file);
            return false;
        }
        const long file_size{ std::ftell(file) };
        if (file_size <= 0)
        {
            std::fclose(file);
            return false;
        }

        const byte_source_t source{
            file,
            static_cast<uint64_t>(file_size),
            file_source_read
        };
        burrow_load_plan_t plan{};
        burrow_load_error_t result{ plan_burrow_image(source, plan) };
        if (result != burrow_load_error_t::success)
        {
            std::fprintf(
                stderr,
                "FAIL: generated artifact plan: %s\n",
                burrow_load_error_name(result));
            std::fclose(file);
            return false;
        }
        if (plan.allocation_size > sizeof(g_artifact_destination))
        {
            std::fprintf(stderr, "FAIL: generated artifact exceeds host fixture capacity\n");
            std::fclose(file);
            return false;
        }

        burrow_loaded_image_t image{};
        result = materialize_burrow_image(
            source,
            plan,
            0x01000000,
            g_artifact_destination,
            plan.allocation_size,
            image);
        std::fclose(file);
        if (result != burrow_load_error_t::success)
        {
            std::fprintf(
                stderr,
                "FAIL: generated artifact materialization: %s\n",
                burrow_load_error_name(result));
            return false;
        }

        return expect_true(
            "generated artifact returns live physical extent",
            image.physical_start == 0x01000000 &&
            image.physical_size == plan.allocation_size &&
            image.entry_physical_address >= image.physical_start &&
            image.entry_physical_address <
                image.physical_start + image.physical_size);
    }
}

int main(int argument_count, char** arguments)
{
    bool passed{ true };
    passed &= run_positive_plan_tests();
    passed &= run_materialization_tests();
    passed &= run_header_rejection_tests();
    passed &= run_segment_rejection_tests();
    passed &= run_dynamic_rejection_tests();
    passed &= run_relocation_rejection_tests();
    if (argument_count == 2)
        passed &= run_generated_artifact_test(arguments[1]);
    else if (argument_count != 1)
    {
        std::fputs("usage: WarrenBurrowLoaderTests [burrow-runtime.elf]\n", stderr);
        return 2;
    }

    if (!passed)
        return 1;

    std::puts("Warren Burrow loader tests passed.");
    return 0;
}
