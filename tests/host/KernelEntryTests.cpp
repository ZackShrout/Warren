//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/KernelEntry.h>

#include <warren/boot/BootInformation.h>

#include <cstdint>
#include <cstdio>

namespace
{
    constexpr uint64_t k_boot_physical{ 0x00100000 };
    constexpr uint64_t k_kernel_physical{ 0x00200000 };
    constexpr uint64_t k_stack_physical{ 0x00300000 };
    constexpr uint64_t k_arena_physical{ 0x00400000 };
    constexpr uint64_t k_boot_virtual{ UINT64_C(0xffff800000100000) };
    constexpr uint64_t k_witness_virtual{ UINT64_C(0xffffffff80005000) };
    constexpr uint32_t k_map_offset{ 0x100 };
    constexpr uint32_t k_map_count{ 3 };
    constexpr uint32_t k_total_size{
        k_map_offset + k_map_count * sizeof(warren_boot_memory_entry_t)
    };

    struct fixture_t
    {
        alignas(8) uint8_t bytes[512]{};
    };

    [[nodiscard]] warren_boot_information_t& header(fixture_t& fixture) noexcept
    {
        return *reinterpret_cast<warren_boot_information_t*>(fixture.bytes);
    }

    [[nodiscard]] warren_boot_memory_entry_t* entries(fixture_t& fixture) noexcept
    {
        return reinterpret_cast<warren_boot_memory_entry_t*>(fixture.bytes + k_map_offset);
    }

    void set_entry(warren_boot_memory_entry_t& entry,
                   uint64_t physical_start,
                   uint64_t pages,
                   uint32_t kind) noexcept
    {
        entry = {
            physical_start,
            pages,
            kind,
            WARREN_BOOT_MEMORY_SOURCE_UEFI,
            7,
            0,
            8,
        };
    }

    [[nodiscard]] fixture_t make_fixture() noexcept
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
        object.present_features = WARREN_BOOT_FEATURE_MEMORY_MAP;
        object.required_features = WARREN_BOOT_FEATURE_MEMORY_MAP;
        object.self_physical_address = k_boot_physical;
        object.kernel_physical_start = k_kernel_physical;
        object.kernel_physical_size = 4 * 4096;
        object.kernel_load_bias = k_kernel_physical;
        object.kernel_entry_physical_address = k_kernel_physical + 4096;
        object.bootstrap_stack_physical_start = k_stack_physical;
        object.bootstrap_stack_size = 16 * 4096;
        object.memory_map = {
            k_map_offset,
            k_map_count,
            sizeof(warren_boot_memory_entry_t),
            0,
        };

        warren_boot_memory_entry_t* map{ entries(fixture) };
        set_entry(map[0], k_boot_physical, 1, WARREN_BOOT_MEMORY_BOOT_INFORMATION);
        set_entry(map[1], k_kernel_physical, 4, WARREN_BOOT_MEMORY_KERNEL_IMAGE);
        set_entry(map[2], k_stack_physical, 16, WARREN_BOOT_MEMORY_BOOTSTRAP_STACK);
        return fixture;
    }

    [[nodiscard]] burrow::core::KernelEntryContext make_context(
        uint32_t initial_exception_level = 1) noexcept
    {
        return {
            burrow::core::k_kernel_entry_abi_major,
            burrow::core::k_kernel_entry_context_size,
            burrow::core::k_kernel_entry_flags,
            k_boot_virtual,
            k_boot_physical,
            k_total_size,
            initial_exception_level,
            k_arena_physical,
            burrow::core::k_kernel_entry_arena_page_count,
            k_witness_virtual,
        };
    }

    [[nodiscard]] bool expect_u64(const char* name,
                                  uint64_t actual,
                                  uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected 0x%llx, received 0x%llx\n",
                     name,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] uint32_t enter(const burrow::core::KernelEntryContext& context,
                                 const fixture_t& fixture,
                                 uint64_t& witness) noexcept
    {
        return burrow::core::validate_kernel_entry(
            context, fixture.bytes, &witness);
    }

    [[nodiscard]] bool expect_rejected(
        const char* name,
        const burrow::core::KernelEntryContext& context,
        const fixture_t& fixture) noexcept
    {
        uint64_t witness{ UINT64_C(0x1122334455667788) };
        const uint32_t result{ enter(context, fixture, witness) };
        return expect_u64(name, result, 0) &&
            expect_u64("rejection preserves witness", witness,
                       UINT64_C(0x1122334455667788));
    }

    [[nodiscard]] bool run_success_tests() noexcept
    {
        fixture_t fixture{ make_fixture() };
        uint64_t witness{ 0 };
        bool passed{ expect_u64(
            "EL1 context result",
            enter(make_context(1), fixture, witness),
            burrow::core::k_kernel_entry_success) };
        passed &= expect_u64("EL1 retained witness", witness,
                             burrow::core::k_kernel_entry_witness);

        witness = 0;
        passed &= expect_u64(
            "EL2 context result",
            enter(make_context(2), fixture, witness),
            burrow::core::k_kernel_entry_success);
        passed &= expect_u64("EL2 retained witness", witness,
                             burrow::core::k_kernel_entry_witness);
        return passed;
    }

    [[nodiscard]] bool run_context_rejection_tests() noexcept
    {
        const fixture_t fixture{ make_fixture() };
        bool passed{ true };

        burrow::core::KernelEntryContext context{ make_context() };
        context.abi_major++;
        passed &= expect_rejected("ABI major", context, fixture);
        context = make_context();
        context.structure_size--;
        passed &= expect_rejected("structure size", context, fixture);
        context = make_context();
        context.flags ^= 1;
        passed &= expect_rejected("flags", context, fixture);
        context = make_context();
        context.boot_information_address = k_boot_physical;
        passed &= expect_rejected("lower boot address", context, fixture);
        context = make_context();
        context.boot_information_physical_address |= UINT64_C(1) << 63;
        passed &= expect_rejected("upper physical boot value", context, fixture);
        context = make_context();
        context.boot_information_byte_count--;
        passed &= expect_rejected("inexact boot byte count", context, fixture);
        context = make_context(3);
        passed &= expect_rejected("initial exception level", context, fixture);
        context = make_context();
        context.transition_arena_physical_address++;
        passed &= expect_rejected("arena alignment", context, fixture);
        context = make_context();
        context.transition_arena_page_count--;
        passed &= expect_rejected("arena page count", context, fixture);
        context = make_context();
        context.retained_witness_address = 0x5000;
        passed &= expect_rejected("lower witness address", context, fixture);
        context = make_context();
        context.retained_witness_address++;
        passed &= expect_rejected("witness alignment", context, fixture);
        return passed;
    }

    [[nodiscard]] bool run_object_rejection_tests() noexcept
    {
        bool passed{ true };
        fixture_t invalid_object{ make_fixture() };
        header(invalid_object).reserved[0] = 1;
        passed &= expect_rejected("boot-information revalidation",
                                  make_context(), invalid_object);

        fixture_t wrong_self{ make_fixture() };
        header(wrong_self).self_physical_address += 4096;
        passed &= expect_rejected("boot physical witness", make_context(), wrong_self);

        uint64_t witness{ UINT64_C(0x8877665544332211) };
        const burrow::core::KernelEntryContext context{ make_context() };
        passed &= expect_u64(
            "null readable object",
            burrow::core::validate_kernel_entry(context, nullptr, &witness), 0);
        passed &= expect_u64(
            "null writable witness",
            burrow::core::validate_kernel_entry(context, invalid_object.bytes, nullptr), 0);
        passed &= expect_u64("null inputs preserve witness", witness,
                             UINT64_C(0x8877665544332211));
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_success_tests();
    passed &= run_context_rejection_tests();
    passed &= run_object_rejection_tests();
    if (!passed) return 1;

    std::puts("Warren kernel-entry tests passed.");
    return 0;
}
