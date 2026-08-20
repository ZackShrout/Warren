//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/PageTables.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using burrow::arch::aarch64::page_table_error_t;
    using burrow::arch::aarch64::page_table_storage_t;
    using burrow::arch::aarch64::activation_preflight_error_t;
    using burrow::arch::aarch64::activation_preflight_t;
    using burrow::arch::aarch64::translation_configuration_t;
    using burrow::core::transition_mapping_t;
    using burrow::core::transition_memory_type_t;
    using burrow::core::transition_plan_t;

    constexpr uint64_t k_feature_40_bit{ 2 };
    constexpr uint64_t k_image_physical{ 0x00200000 };
    constexpr uint64_t k_boot_physical{ 0x00100000 };
    constexpr uint64_t k_bootstrap_stack_physical{ 0x00300000 };
    constexpr uint64_t k_arena_physical{ 0x00400000 };
    constexpr uint64_t k_table_physical{ k_arena_physical + 17 * 4096 };
    constexpr uint32_t k_read{ burrow::core::k_transition_permission_read };
    constexpr uint32_t k_read_write{
        k_read | burrow::core::k_transition_permission_write
    };
    constexpr uint32_t k_read_execute{
        k_read | burrow::core::k_transition_permission_execute
    };
    constexpr uint32_t k_identity{
        burrow::core::k_transition_mapping_temporary_identity
    };
    constexpr uint64_t k_address_mask{ UINT64_C(0x0000fffffffff000) };

    struct fixture_t
    {
        transition_plan_t plan{};
        alignas(4096) uint64_t empty_root[512]{};
        alignas(4096) uint64_t tables[
            burrow::core::k_transition_table_page_count *
            burrow::arch::aarch64::k_page_table_entry_count
        ]{};
        page_table_storage_t storage{};
        translation_configuration_t configuration{};
    };

    void append_mapping(transition_plan_t& plan,
                        uint64_t physical,
                        uint64_t virtual_address,
                        uint64_t pages,
                        transition_memory_type_t memory_type,
                        uint32_t permissions,
                        uint32_t flags) noexcept
    {
        transition_mapping_t& mapping{ plan.mappings[plan.mapping_count++] };
        mapping.physical_start = { physical };
        mapping.virtual_start = { virtual_address };
        mapping.page_count = { pages };
        mapping.memory_type = memory_type;
        mapping.permissions = permissions;
        mapping.flags = flags;
        mapping.reserved = 0;
    }

    [[nodiscard]] transition_plan_t make_plan() noexcept
    {
        transition_plan_t plan{};
        plan.arena_physical_start = { k_arena_physical };
        plan.arena_page_count = { burrow::core::k_transition_arena_page_count };
        plan.empty_root_physical_address = { k_arena_physical };
        plan.early_stack_physical_start = { k_arena_physical + 4096 };
        plan.early_stack_page_count = { burrow::core::k_transition_stack_page_count };
        plan.page_table_physical_start = { k_table_physical };
        plan.page_table_page_count = { burrow::core::k_transition_table_page_count };
        plan.boot_information_physical_start = { k_boot_physical };
        plan.boot_information_page_count = { 1 };
        plan.bootstrap_stack_physical_start = { k_bootstrap_stack_physical };
        plan.bootstrap_stack_page_count = { 16 };
        plan.console_physical_address = {
            burrow::core::k_reference_pl011_physical_address
        };
        plan.early_stack_virtual_start = { burrow::core::k_early_stack_virtual_start };
        plan.early_stack_virtual_top = { burrow::core::k_early_stack_virtual_top };

        append_mapping(plan, k_image_physical, k_image_physical, 1,
                       transition_memory_type_t::normal, k_read, k_identity);
        append_mapping(plan, k_image_physical + 0x1000, k_image_physical + 0x1000, 2,
                       transition_memory_type_t::normal, k_read_execute, k_identity);
        append_mapping(plan, k_image_physical + 0x3000, k_image_physical + 0x3000, 1,
                       transition_memory_type_t::normal, k_read_write, k_identity);
        append_mapping(plan, k_image_physical, burrow::core::k_kernel_virtual_bias, 1,
                       transition_memory_type_t::normal, k_read, 0);
        append_mapping(plan, k_image_physical + 0x1000,
                       burrow::core::k_kernel_virtual_bias + 0x1000, 2,
                       transition_memory_type_t::normal, k_read_execute, 0);
        append_mapping(plan, k_image_physical + 0x3000,
                       burrow::core::k_kernel_virtual_bias + 0x3000, 1,
                       transition_memory_type_t::normal, k_read_write, 0);
        append_mapping(plan, k_boot_physical, k_boot_physical, 1,
                       transition_memory_type_t::normal, k_read, k_identity);
        append_mapping(plan, k_boot_physical,
                       burrow::core::k_direct_map_virtual_bias + k_boot_physical, 1,
                       transition_memory_type_t::normal, k_read, 0);
        append_mapping(plan, k_bootstrap_stack_physical, k_bootstrap_stack_physical, 16,
                       transition_memory_type_t::normal, k_read_write, k_identity);
        append_mapping(plan, k_arena_physical, k_arena_physical,
                       burrow::core::k_transition_arena_page_count,
                       transition_memory_type_t::normal, k_read_write, k_identity);
        append_mapping(plan, k_arena_physical,
                       burrow::core::k_direct_map_virtual_bias + k_arena_physical,
                       burrow::core::k_transition_arena_page_count,
                       transition_memory_type_t::normal, k_read_write, 0);
        append_mapping(plan, burrow::core::k_reference_pl011_physical_address,
                       burrow::core::k_reference_pl011_physical_address, 1,
                       transition_memory_type_t::device, k_read_write, k_identity);
        append_mapping(plan, burrow::core::k_reference_pl011_physical_address,
                       burrow::core::k_reference_pl011_virtual_address, 1,
                       transition_memory_type_t::device, k_read_write, 0);
        append_mapping(plan, k_arena_physical + 0x1000,
                       burrow::core::k_early_stack_virtual_start,
                       burrow::core::k_transition_stack_page_count,
                       transition_memory_type_t::normal, k_read_write, 0);
        return plan;
    }

    void initialize(fixture_t& fixture) noexcept
    {
        fixture.plan = make_plan();
        fixture.storage = {
            fixture.empty_root,
            { k_arena_physical },
            fixture.tables,
            { k_table_physical },
            { burrow::core::k_transition_table_page_count },
        };
    }

    [[nodiscard]] bool expect_u64(const char* name, uint64_t actual, uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected 0x%llx, received 0x%llx\n",
                     name,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] bool expect_error(const char* name,
                                    page_table_error_t actual,
                                    page_table_error_t expected) noexcept
    {
        return expect_u64(name, static_cast<uint32_t>(actual), static_cast<uint32_t>(expected));
    }

    [[nodiscard]] bool expect_preflight_error(
        const char* name,
        activation_preflight_error_t actual,
        activation_preflight_error_t expected) noexcept
    {
        return expect_u64(name, static_cast<uint32_t>(actual), static_cast<uint32_t>(expected));
    }

    [[nodiscard]] activation_preflight_t make_preflight() noexcept
    {
        return {
            { k_image_physical + 0x1000 },
            { k_bootstrap_stack_physical + 16 * 4096 },
            { k_boot_physical },
            { k_image_physical + 0x1000 },
            { burrow::core::k_kernel_virtual_bias + 0x1000 },
            { k_image_physical + 0x1800 },
            { burrow::core::k_kernel_virtual_bias + 0x1800 },
            { burrow::core::k_early_stack_virtual_top },
            { burrow::core::k_direct_map_virtual_bias + k_boot_physical },
            { burrow::core::k_direct_map_virtual_bias + k_arena_physical },
            { burrow::core::k_reference_pl011_virtual_address },
        };
    }

    [[nodiscard]] uint64_t* table_for_physical(fixture_t& fixture, uint64_t physical) noexcept
    {
        if (physical < k_table_physical) return nullptr;
        const uint64_t index{ (physical - k_table_physical) / 4096 };
        if (index >= fixture.configuration.table_page_count.value) return nullptr;
        return fixture.tables + index * 512;
    }

    [[nodiscard]] uint64_t* leaf_entry(fixture_t& fixture, uint64_t virtual_address) noexcept
    {
        const uint64_t root_physical{
            virtual_address >= UINT64_C(0xffff800000000000) ?
                fixture.configuration.ttbr1_root_physical_address.value :
                fixture.configuration.ttbr0_root_physical_address.value
        };
        uint64_t* table{ table_for_physical(fixture, root_physical) };
        if (table == nullptr) return nullptr;
        constexpr uint32_t shifts[]{ 39, 30, 21 };
        for (const uint32_t shift : shifts)
        {
            const uint64_t descriptor{ table[(virtual_address >> shift) & 0x1ff] };
            table = table_for_physical(fixture, descriptor & k_address_mask);
            if (table == nullptr) return nullptr;
        }
        return &table[(virtual_address >> 12) & 0x1ff];
    }

    [[nodiscard]] bool build(fixture_t& fixture, uint64_t features = k_feature_40_bit) noexcept
    {
        return burrow::arch::aarch64::build_page_tables(
                   features, fixture.plan, fixture.storage, fixture.configuration) ==
            page_table_error_t::success;
    }

    [[nodiscard]] bool run_reference_tests() noexcept
    {
        fixture_t fixture{};
        initialize(fixture);
        bool passed{ expect_error(
            "reference build",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, fixture.plan, fixture.storage, fixture.configuration),
            page_table_error_t::success) };
        passed &= expect_u64("TTBR0 root", fixture.configuration.ttbr0_root_physical_address.value,
                             k_table_physical);
        passed &= expect_u64("TTBR1 root", fixture.configuration.ttbr1_root_physical_address.value,
                             k_table_physical + 4096);
        passed &= expect_u64("MAIR", fixture.configuration.mair_el1,
                             burrow::arch::aarch64::k_mair_el1);
        passed &= expect_u64("TCR", fixture.configuration.tcr_el1,
                             burrow::arch::aarch64::k_tcr_el1_base | (UINT64_C(2) << 32));
        passed &= expect_u64("SCTLR", fixture.configuration.sctlr_el1,
                             burrow::arch::aarch64::k_sctlr_el1_owned);
        passed &= expect_u64("physical bits", fixture.configuration.physical_address_bits, 40);
        passed &= expect_error(
            "reference audit",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, fixture.plan, fixture.storage, fixture.configuration),
            page_table_error_t::success);

        uint64_t* text{ leaf_entry(
            fixture, burrow::core::k_kernel_virtual_bias + 0x1000) };
        uint64_t* device{ leaf_entry(
            fixture, burrow::core::k_reference_pl011_virtual_address) };
        passed &= expect_u64("text leaf present", text != nullptr, 1);
        passed &= expect_u64("device leaf present", device != nullptr, 1);
        if (text != nullptr)
        {
            constexpr uint64_t expected{
                (k_image_physical + 0x1000) | (UINT64_C(1) << 10) |
                (UINT64_C(2) << 6) | (UINT64_C(3) << 8) |
                (UINT64_C(1) << 54) | 3
            };
            passed &= expect_u64("text leaf policy", *text, expected);
        }
        if (device != nullptr)
        {
            constexpr uint64_t expected{
                burrow::core::k_reference_pl011_physical_address |
                (UINT64_C(1) << 2) | (UINT64_C(2) << 8) |
                (UINT64_C(1) << 10) | (UINT64_C(1) << 53) |
                (UINT64_C(1) << 54) | 3
            };
            passed &= expect_u64("device leaf policy", *device, expected);
        }
        uint64_t* lower_guard{ leaf_entry(
            fixture, burrow::core::k_early_stack_guard_start) };
        uint64_t* upper_guard{ leaf_entry(
            fixture, burrow::core::k_early_stack_virtual_top) };
        passed &= expect_u64("lower guard absent",
                             lower_guard != nullptr && *lower_guard == 0, 1);
        passed &= expect_u64("upper guard absent",
                             upper_guard != nullptr && *upper_guard == 0, 1);
        return passed;
    }

    [[nodiscard]] bool run_feature_and_boundary_tests() noexcept
    {
        bool passed{ true };
        fixture_t fixture{};
        initialize(fixture);
        passed &= expect_error(
            "unsupported 4K extension encoding",
            burrow::arch::aarch64::build_page_tables(
                UINT64_C(1) << 28, fixture.plan, fixture.storage, fixture.configuration),
            page_table_error_t::unsupported_granule);
        passed &= expect_u64("failed feature clears configuration",
                             fixture.configuration.table_page_count.value, 0);

        fixture_t invalid_range{};
        initialize(invalid_range);
        passed &= expect_error(
            "unsupported PARange encoding",
            burrow::arch::aarch64::build_page_tables(
                7, invalid_range.plan, invalid_range.storage, invalid_range.configuration),
            page_table_error_t::unsupported_physical_range);

        fixture_t capped_range{};
        initialize(capped_range);
        passed &= expect_error(
            "future PARange caps at 48 bits",
            burrow::arch::aarch64::build_page_tables(
                6, capped_range.plan, capped_range.storage, capped_range.configuration),
            page_table_error_t::success);
        passed &= expect_u64("capped physical bits",
                             capped_range.configuration.physical_address_bits, 48);
        passed &= expect_u64("capped IPS", capped_range.configuration.ips_encoding, 5);

        fixture_t physical_overflow{};
        initialize(physical_overflow);
        append_mapping(physical_overflow.plan, UINT64_C(0x100000000),
                       UINT64_C(0xfffff00000000000), 1,
                       transition_memory_type_t::normal, k_read, 0);
        passed &= expect_error(
            "planned physical page exceeds PARange",
            burrow::arch::aarch64::build_page_tables(
                0, physical_overflow.plan, physical_overflow.storage,
                physical_overflow.configuration),
            page_table_error_t::unsupported_physical_range);
        return passed;
    }

    [[nodiscard]] bool run_invalid_plan_tests() noexcept
    {
        bool passed{ true };
        fixture_t noncanonical{};
        initialize(noncanonical);
        noncanonical.plan.mappings[3].virtual_start.value = UINT64_C(0x0000800000000000);
        passed &= expect_error(
            "noncanonical virtual address",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, noncanonical.plan, noncanonical.storage,
                noncanonical.configuration),
            page_table_error_t::noncanonical_virtual_address);

        fixture_t misaligned{};
        initialize(misaligned);
        misaligned.plan.mappings[3].virtual_start.value++;
        passed &= expect_error(
            "misaligned mapping",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, misaligned.plan, misaligned.storage,
                misaligned.configuration),
            page_table_error_t::invalid_mapping_policy);

        fixture_t writable_executable{};
        initialize(writable_executable);
        writable_executable.plan.mappings[4].permissions |=
            burrow::core::k_transition_permission_write;
        passed &= expect_error(
            "writable executable request",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, writable_executable.plan, writable_executable.storage,
                writable_executable.configuration),
            page_table_error_t::invalid_mapping_policy);

        fixture_t conflict{};
        initialize(conflict);
        append_mapping(conflict.plan, k_image_physical,
                       burrow::core::k_kernel_virtual_bias, 1,
                       transition_memory_type_t::normal, k_read, 0);
        passed &= expect_error(
            "repeated identical mapping",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, conflict.plan, conflict.storage, conflict.configuration),
            page_table_error_t::mapping_conflict);

        fixture_t mmio_direct{};
        initialize(mmio_direct);
        append_mapping(mmio_direct.plan,
                       burrow::core::k_reference_pl011_physical_address,
                       burrow::core::k_direct_map_virtual_bias +
                           burrow::core::k_reference_pl011_physical_address,
                       1, transition_memory_type_t::device, k_read_write, 0);
        passed &= expect_error(
            "device mapping in direct-map window",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, mmio_direct.plan, mmio_direct.storage,
                mmio_direct.configuration),
            page_table_error_t::invalid_mapping_policy);

        fixture_t missing_arena{};
        initialize(missing_arena);
        missing_arena.plan.mappings[9].page_count.value--;
        passed &= expect_error(
            "missing complete arena identity",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, missing_arena.plan, missing_arena.storage,
                missing_arena.configuration),
            page_table_error_t::invalid_plan);

        fixture_t missing_boot_object{};
        initialize(missing_boot_object);
        missing_boot_object.plan.boot_information_page_count.value = 2;
        passed &= expect_error(
            "missing complete boot-information mapping",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, missing_boot_object.plan,
                missing_boot_object.storage, missing_boot_object.configuration),
            page_table_error_t::invalid_plan);

        fixture_t storage_dirty{};
        initialize(storage_dirty);
        storage_dirty.tables[200] = 1;
        passed &= expect_error(
            "caller storage must be zero",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, storage_dirty.plan, storage_dirty.storage,
                storage_dirty.configuration),
            page_table_error_t::storage_not_zero);

        fixture_t alias{};
        initialize(alias);
        append_mapping(alias.plan, k_image_physical + 0x1000,
                       UINT64_C(0xfffff00000000000), 1,
                       transition_memory_type_t::normal, k_read_write, 0);
        passed &= expect_error(
            "writable alias of executable page",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, alias.plan, alias.storage, alias.configuration),
            page_table_error_t::writable_executable_alias);

        fixture_t exhausted{};
        initialize(exhausted);
        append_mapping(exhausted.plan, UINT64_C(0x10000000),
                       UINT64_C(0xfffff00000000000), 45057,
                       transition_memory_type_t::normal, k_read, 0);
        passed &= expect_error(
            "table capacity exhaustion",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, exhausted.plan, exhausted.storage,
                exhausted.configuration),
            page_table_error_t::table_capacity_exceeded);

        fixture_t exact_capacity{};
        initialize(exact_capacity);
        append_mapping(exact_capacity.plan, UINT64_C(0x10000000),
                       UINT64_C(0xfffff00000000000), 45056,
                       transition_memory_type_t::normal, k_read, 0);
        passed &= expect_error(
            "table capacity exact fit",
            burrow::arch::aarch64::build_page_tables(
                k_feature_40_bit, exact_capacity.plan, exact_capacity.storage,
                exact_capacity.configuration),
            page_table_error_t::success);
        passed &= expect_u64("exact-fit table pages",
                             exact_capacity.configuration.table_page_count.value,
                             burrow::core::k_transition_table_page_count);
        return passed;
    }

    [[nodiscard]] bool run_audit_corruption_tests() noexcept
    {
        bool passed{ true };
        fixture_t block{};
        initialize(block);
        if (!build(block)) return false;
        uint64_t* root{ table_for_physical(
            block, block.configuration.ttbr0_root_physical_address.value) };
        if (root == nullptr) return false;
        uint64_t root_index{ 0 };
        while (root_index < 512 && root[root_index] == 0) ++root_index;
        if (root_index == 512) return false;
        root[root_index] &= ~UINT64_C(2);
        passed &= expect_error(
            "intermediate block descriptor",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, block.plan, block.storage, block.configuration),
            page_table_error_t::invalid_table_descriptor);

        fixture_t wrong_permission{};
        initialize(wrong_permission);
        if (!build(wrong_permission)) return false;
        uint64_t* text{ leaf_entry(
            wrong_permission, burrow::core::k_kernel_virtual_bias + 0x1000) };
        if (text == nullptr) return false;
        *text |= UINT64_C(1) << 53;
        passed &= expect_error(
            "wrong text permission",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, wrong_permission.plan, wrong_permission.storage,
                wrong_permission.configuration),
            page_table_error_t::missing_mapping);

        fixture_t guard{};
        initialize(guard);
        if (!build(guard)) return false;
        uint64_t* first_stack_page{ leaf_entry(
            guard, burrow::core::k_early_stack_virtual_start) };
        uint64_t* lower_guard{ leaf_entry(
            guard, burrow::core::k_early_stack_guard_start) };
        if (first_stack_page == nullptr || lower_guard == nullptr) return false;
        *lower_guard = *first_stack_page;
        passed &= expect_error(
            "mapped lower stack guard",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, guard.plan, guard.storage, guard.configuration),
            page_table_error_t::mapped_stack_guard);

        fixture_t unreachable{};
        initialize(unreachable);
        if (!build(unreachable)) return false;
        unreachable.tables[
            unreachable.configuration.table_page_count.value * 512
        ] = 3;
        passed &= expect_error(
            "nonzero unused table page",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, unreachable.plan, unreachable.storage,
                unreachable.configuration),
            page_table_error_t::unreachable_table);

        fixture_t nonempty_root{};
        initialize(nonempty_root);
        if (!build(nonempty_root)) return false;
        nonempty_root.empty_root[0] = 3;
        passed &= expect_error(
            "empty lower root contains descriptor",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, nonempty_root.plan, nonempty_root.storage,
                nonempty_root.configuration),
            page_table_error_t::invalid_table_descriptor);

        fixture_t changed_root{};
        initialize(changed_root);
        if (!build(changed_root)) return false;
        changed_root.configuration.ttbr0_root_physical_address.value += 4096;
        passed &= expect_error(
            "configuration root drift",
            burrow::arch::aarch64::audit_page_tables(
                k_feature_40_bit, changed_root.plan, changed_root.storage,
                changed_root.configuration),
            page_table_error_t::invalid_plan);
        return passed;
    }

    [[nodiscard]] bool run_activation_preflight_tests() noexcept
    {
        fixture_t fixture{};
        initialize(fixture);
        if (!build(fixture)) return false;

        bool passed{ expect_preflight_error(
            "complete activation preflight",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, make_preflight()),
            activation_preflight_error_t::success) };

        activation_preflight_t invalid_stack{ make_preflight() };
        invalid_stack.current_stack_pointer.value++;
        passed &= expect_preflight_error(
            "unaligned current stack",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, invalid_stack),
            activation_preflight_error_t::invalid_runtime_state);

        activation_preflight_t missing_current_pc{ make_preflight() };
        missing_current_pc.current_program_counter.value = k_image_physical;
        passed &= expect_preflight_error(
            "current PC is not executable",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, missing_current_pc),
            activation_preflight_error_t::missing_current_program_counter);

        activation_preflight_t missing_current_stack{ make_preflight() };
        missing_current_stack.current_stack_pointer.value =
            k_bootstrap_stack_physical;
        passed &= expect_preflight_error(
            "current stack is not writable",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, missing_current_stack),
            activation_preflight_error_t::missing_current_stack);

        activation_preflight_t missing_target{ make_preflight() };
        missing_target.target_program_counter_virtual_address.value =
            burrow::core::k_kernel_virtual_bias;
        passed &= expect_preflight_error(
            "higher continuation is not executable",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, missing_target),
            activation_preflight_error_t::missing_target_program_counter);

        activation_preflight_t wrong_vector{ make_preflight() };
        wrong_vector.stable_vectors_virtual_address.value += 0x800;
        passed &= expect_preflight_error(
            "stable vectors map the wrong physical page",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, wrong_vector),
            activation_preflight_error_t::missing_stable_vectors);

        activation_preflight_t wrong_boot_alias{ make_preflight() };
        wrong_boot_alias.boot_information_virtual_address.value += 4096;
        passed &= expect_preflight_error(
            "boot-information alias must be exact",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, fixture.plan, fixture.storage,
                fixture.configuration, wrong_boot_alias),
            activation_preflight_error_t::invalid_runtime_state);

        fixture_t corrupt{};
        initialize(corrupt);
        if (!build(corrupt)) return false;
        uint64_t* target{ leaf_entry(
            corrupt, burrow::core::k_kernel_virtual_bias + 0x1000) };
        if (target == nullptr) return false;
        *target |= UINT64_C(1) << 53;
        passed &= expect_preflight_error(
            "preflight rejects unaudited tables",
            burrow::arch::aarch64::preflight_activation(
                k_feature_40_bit, corrupt.plan, corrupt.storage,
                corrupt.configuration, make_preflight()),
            activation_preflight_error_t::invalid_tables);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_reference_tests();
    passed &= run_feature_and_boundary_tests();
    passed &= run_invalid_plan_tests();
    passed &= run_audit_corruption_tests();
    passed &= run_activation_preflight_tests();
    if (!passed) return 1;

    std::puts("Warren AArch64 page-table tests passed.");
    return 0;
}
