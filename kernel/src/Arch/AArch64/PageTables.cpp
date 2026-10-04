//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/PageTables.h>

#include <stdint.h>

namespace burrow::arch::aarch64 {
    namespace {
        constexpr uint64_t k_page_mask{ core::k_transition_page_size - 1 };
        constexpr uint64_t k_address_mask{ UINT64_C(0x0000fffffffff000) };
        constexpr uint64_t k_descriptor_valid{ UINT64_C(1) << 0 };
        constexpr uint64_t k_descriptor_table_or_page{ UINT64_C(1) << 1 };
        constexpr uint64_t k_table_uxn{ UINT64_C(1) << 60 };
        constexpr uint64_t k_access_flag{ UINT64_C(1) << 10 };
        constexpr uint64_t k_page_read_only{ UINT64_C(2) << 6 };
        constexpr uint64_t k_shareability_inner{ UINT64_C(3) << 8 };
        constexpr uint64_t k_shareability_outer{ UINT64_C(2) << 8 };
        constexpr uint64_t k_privileged_execute_never{ UINT64_C(1) << 53 };
        constexpr uint64_t k_unprivileged_execute_never{ UINT64_C(1) << 54 };
        constexpr uint64_t k_table_allowed_mask{
            k_address_mask | k_table_uxn | k_descriptor_valid | k_descriptor_table_or_page
        };
        constexpr uint64_t k_page_allowed_mask{
            k_address_mask | (UINT64_C(7) << 2) | (UINT64_C(3) << 6) |
            (UINT64_C(3) << 8) | k_access_flag | k_privileged_execute_never |
            k_unprivileged_execute_never | k_descriptor_valid | k_descriptor_table_or_page
        };
        constexpr uint64_t k_lower_canonical_end{ UINT64_C(0x0000800000000000) };
        constexpr uint64_t k_upper_canonical_start{ UINT64_C(0xffff800000000000) };
        constexpr uint64_t k_mmio_virtual_end{ UINT64_C(0xffffd00000000000) };
        constexpr uint32_t k_known_permissions{
            core::k_transition_permission_read |
            core::k_transition_permission_write |
            core::k_transition_permission_execute
        };

        struct feature_configuration_t
        {
            uint32_t physical_address_bits;
            uint32_t ips_encoding;
            uint64_t physical_limit;
            uint64_t tcr_el1;
        };

        struct build_context_t
        {
            const page_table_storage_t& storage;
            uint64_t physical_limit;
            uint64_t next_page;
        };

        struct walk_context_t
        {
            const page_table_storage_t& storage;
            const translation_configuration_t& configuration;
            uint64_t* visited;
            uint64_t leaf_count;
        };

        struct lookup_result_t
        {
            page_table_error_t error;
            bool present;
            uint64_t descriptor;
        };

        enum class mapping_check_t
        {
            success,
            missing,
            invalid,
        };

        void clear_configuration(translation_configuration_t& configuration) noexcept
        {
            configuration.ttbr0_root_physical_address = { 0 };
            configuration.ttbr1_root_physical_address = { 0 };
            configuration.table_page_count = { 0 };
            configuration.mair_el1 = 0;
            configuration.tcr_el1 = 0;
            configuration.sctlr_el1 = 0;
            configuration.physical_address_bits = 0;
            configuration.ips_encoding = 0;
        }

        [[nodiscard]] bool add_without_overflow(uint64_t left, uint64_t right, uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(uint64_t left, uint64_t right,
                                                     uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left) return false;
            result = left * right;
            return true;
        }

        [[nodiscard]] bool ranges_overlap(uint64_t left_begin, uint64_t left_end,
                                          uint64_t right_begin, uint64_t right_end) noexcept
        {
            return left_begin < right_end && right_begin < left_end;
        }

        [[nodiscard]] page_table_error_t discover_features(
            uint64_t id_aa64mmfr0_el1,
            feature_configuration_t& features) noexcept
        {
            const uint32_t tgran4{ static_cast<uint32_t>((id_aa64mmfr0_el1 >> 28) & 0xf) };
            if (tgran4 != 0) return page_table_error_t::unsupported_granule;

            const uint32_t parange{ static_cast<uint32_t>(id_aa64mmfr0_el1 & 0xf) };
            constexpr uint32_t physical_bits[]{ 32, 36, 40, 42, 44, 48 };
            if (parange > 6) return page_table_error_t::unsupported_physical_range;

            features.ips_encoding = parange < 5 ? parange : 5;
            features.physical_address_bits = parange < 6 ? physical_bits[parange] : 48;
            features.physical_limit = UINT64_C(1) << features.physical_address_bits;
            features.tcr_el1 = k_tcr_el1_base |
                (static_cast<uint64_t>(features.ips_encoding) << 32);
            return page_table_error_t::success;
        }

        [[nodiscard]] bool is_canonical(uint64_t address) noexcept
        {
            return address < k_lower_canonical_end || address >= k_upper_canonical_start;
        }

        [[nodiscard]] bool is_upper(uint64_t address) noexcept
        {
            return address >= k_upper_canonical_start;
        }

        [[nodiscard]] uint64_t table_descriptor(uint64_t physical_address) noexcept
        {
            return physical_address | k_table_uxn | k_descriptor_valid | k_descriptor_table_or_page;
        }

        [[nodiscard]] uint64_t page_descriptor(const core::transition_mapping_t& mapping,
                                               uint64_t physical_address) noexcept
        {
            uint64_t descriptor{
                physical_address | k_access_flag | k_unprivileged_execute_never |
                k_descriptor_valid | k_descriptor_table_or_page
            };
            if (mapping.memory_type == core::transition_memory_type_t::device)
                descriptor |= UINT64_C(1) << 2 | k_shareability_outer;
            else
                descriptor |= k_shareability_inner;
            if ((mapping.permissions & core::k_transition_permission_write) == 0)
                descriptor |= k_page_read_only;
            if ((mapping.permissions & core::k_transition_permission_execute) == 0)
                descriptor |= k_privileged_execute_never;
            return descriptor;
        }

        [[nodiscard]] bool mapping_matches(
            const core::transition_mapping_t& mapping,
            uint64_t physical_start,
            uint64_t virtual_start,
            uint64_t page_count,
            core::transition_memory_type_t memory_type,
            uint32_t permissions,
            uint32_t flags) noexcept
        {
            return mapping.physical_start.value == physical_start &&
                mapping.virtual_start.value == virtual_start &&
                mapping.page_count.value == page_count &&
                mapping.memory_type == memory_type &&
                mapping.permissions == permissions && mapping.flags == flags &&
                mapping.reserved == 0;
        }

        [[nodiscard]] bool plan_has_mapping(
            const core::transition_plan_t& plan,
            uint64_t physical_start,
            uint64_t virtual_start,
            uint64_t page_count,
            core::transition_memory_type_t memory_type,
            uint32_t permissions,
            uint32_t flags) noexcept
        {
            for (uint32_t index{ 0 }; index < plan.mapping_count; ++index)
            {
                if (mapping_matches(plan.mappings[index], physical_start, virtual_start,
                                    page_count, memory_type, permissions, flags))
                    return true;
            }
            return false;
        }

        [[nodiscard]] page_table_error_t validate_storage(
            const core::transition_plan_t& plan,
            const page_table_storage_t& storage) noexcept
        {
            if (storage.empty_root_writable == nullptr || storage.table_pages_writable == nullptr ||
                (reinterpret_cast<uintptr_t>(storage.empty_root_writable) & k_page_mask) != 0 ||
                (reinterpret_cast<uintptr_t>(storage.table_pages_writable) & k_page_mask) != 0 ||
                storage.empty_root_physical_address.value != plan.empty_root_physical_address.value ||
                storage.table_pages_physical_address.value != plan.page_table_physical_start.value ||
                storage.table_page_capacity.value != plan.page_table_page_count.value ||
                storage.table_page_capacity.value == 0 ||
                storage.table_page_capacity.value > core::k_transition_table_page_count)
                return page_table_error_t::invalid_storage;

            uint64_t table_bytes{ 0 };
            uint64_t table_end{ 0 };
            uint64_t arena_bytes{ 0 };
            uint64_t arena_end{ 0 };
            if ((storage.empty_root_physical_address.value & k_page_mask) != 0 ||
                (storage.table_pages_physical_address.value & k_page_mask) != 0 ||
                !multiply_without_overflow(storage.table_page_capacity.value,
                                           core::k_transition_page_size, table_bytes) ||
                !add_without_overflow(storage.table_pages_physical_address.value, table_bytes, table_end) ||
                !multiply_without_overflow(plan.arena_page_count.value,
                                           core::k_transition_page_size, arena_bytes) ||
                !add_without_overflow(plan.arena_physical_start.value, arena_bytes, arena_end) ||
                storage.empty_root_physical_address.value != plan.arena_physical_start.value ||
                storage.table_pages_physical_address.value < plan.arena_physical_start.value ||
                table_end > arena_end)
                return page_table_error_t::invalid_storage;

            return page_table_error_t::success;
        }

        [[nodiscard]] page_table_error_t validate_plan(
            const core::transition_plan_t& plan,
            uint64_t physical_limit) noexcept
        {
            uint64_t expected_stack_physical{ 0 };
            uint64_t expected_table_physical{ 0 };
            uint64_t expected_stack_top{ 0 };
            if (plan.mapping_count == 0 || plan.mapping_count > core::k_transition_mapping_capacity ||
                plan.reserved != 0 ||
                plan.arena_page_count.value != core::k_transition_arena_page_count ||
                !add_without_overflow(plan.arena_physical_start.value,
                                      core::k_transition_page_size,
                                      expected_stack_physical) ||
                !add_without_overflow(plan.arena_physical_start.value,
                                      17 * core::k_transition_page_size,
                                      expected_table_physical) ||
                !add_without_overflow(plan.early_stack_virtual_start.value,
                                      core::k_transition_stack_page_count *
                                          core::k_transition_page_size,
                                      expected_stack_top) ||
                plan.empty_root_physical_address.value != plan.arena_physical_start.value ||
                plan.early_stack_physical_start.value != expected_stack_physical ||
                plan.early_stack_page_count.value != core::k_transition_stack_page_count ||
                plan.page_table_physical_start.value != expected_table_physical ||
                plan.page_table_page_count.value != core::k_transition_table_page_count ||
                plan.boot_information_page_count.value == 0 ||
                plan.bootstrap_stack_page_count.value == 0 ||
                plan.console_physical_address.value != core::k_reference_pl011_physical_address ||
                (plan.boot_information_physical_start.value & k_page_mask) != 0 ||
                (plan.bootstrap_stack_physical_start.value & k_page_mask) != 0 ||
                plan.early_stack_virtual_start.value != core::k_early_stack_virtual_start ||
                plan.early_stack_virtual_top.value != core::k_early_stack_virtual_top ||
                plan.early_stack_virtual_top.value != expected_stack_top)
                return page_table_error_t::invalid_plan;

            const uint32_t read_write{
                core::k_transition_permission_read | core::k_transition_permission_write
            };
            if (!plan_has_mapping(plan, plan.arena_physical_start.value,
                                  plan.arena_physical_start.value,
                                  core::k_transition_arena_page_count,
                                  core::transition_memory_type_t::normal, read_write,
                                  core::k_transition_mapping_temporary_identity) ||
                !plan_has_mapping(plan, plan.arena_physical_start.value,
                                  core::k_direct_map_virtual_bias + plan.arena_physical_start.value,
                                  core::k_transition_arena_page_count,
                                  core::transition_memory_type_t::normal, read_write, 0) ||
                !plan_has_mapping(plan, plan.early_stack_physical_start.value,
                                  core::k_early_stack_virtual_start,
                                  core::k_transition_stack_page_count,
                                  core::transition_memory_type_t::normal, read_write, 0) ||
                !plan_has_mapping(plan, plan.boot_information_physical_start.value,
                                  plan.boot_information_physical_start.value,
                                  plan.boot_information_page_count.value,
                                  core::transition_memory_type_t::normal,
                                  core::k_transition_permission_read,
                                  core::k_transition_mapping_temporary_identity) ||
                !plan_has_mapping(plan, plan.boot_information_physical_start.value,
                                  core::k_direct_map_virtual_bias +
                                      plan.boot_information_physical_start.value,
                                  plan.boot_information_page_count.value,
                                  core::transition_memory_type_t::normal,
                                  core::k_transition_permission_read, 0) ||
                !plan_has_mapping(plan, plan.bootstrap_stack_physical_start.value,
                                  plan.bootstrap_stack_physical_start.value,
                                  plan.bootstrap_stack_page_count.value,
                                  core::transition_memory_type_t::normal, read_write,
                                  core::k_transition_mapping_temporary_identity) ||
                !plan_has_mapping(plan, plan.console_physical_address.value,
                                  plan.console_physical_address.value, 1,
                                  core::transition_memory_type_t::device, read_write,
                                  core::k_transition_mapping_temporary_identity) ||
                !plan_has_mapping(plan, plan.console_physical_address.value,
                                  core::k_reference_pl011_virtual_address, 1,
                                  core::transition_memory_type_t::device, read_write, 0) ||
                !plan_has_mapping(plan,
                                  core::k_reference_gic_distributor_physical_address,
                                  core::k_reference_gic_distributor_virtual_address,
                                  core::k_reference_gic_distributor_page_count,
                                  core::transition_memory_type_t::device, read_write, 0) ||
                !plan_has_mapping(plan,
                                  core::k_reference_gic_redistributor_physical_address,
                                  core::k_reference_gic_redistributor_virtual_address,
                                  core::k_reference_gic_redistributor_page_count,
                                  core::transition_memory_type_t::device, read_write, 0))
                return page_table_error_t::invalid_plan;

            bool executable_identity_present{ false };
            bool executable_stable_present{ false };
            for (uint32_t index{ 0 }; index < plan.mapping_count; ++index)
            {
                const core::transition_mapping_t& mapping{ plan.mappings[index] };
                if (mapping.reserved != 0 || mapping.page_count.value == 0 ||
                    (mapping.physical_start.value & k_page_mask) != 0 ||
                    (mapping.virtual_start.value & k_page_mask) != 0 ||
                    (mapping.permissions & core::k_transition_permission_read) == 0 ||
                    (mapping.permissions & ~k_known_permissions) != 0 ||
                    (mapping.permissions & (core::k_transition_permission_write |
                                            core::k_transition_permission_execute)) ==
                        (core::k_transition_permission_write |
                         core::k_transition_permission_execute) ||
                    (mapping.flags & ~core::k_transition_mapping_temporary_identity) != 0)
                    return page_table_error_t::invalid_mapping_policy;
                if (mapping.memory_type != core::transition_memory_type_t::normal &&
                    mapping.memory_type != core::transition_memory_type_t::device)
                    return page_table_error_t::invalid_mapping_policy;

                uint64_t byte_count{ 0 };
                uint64_t physical_end{ 0 };
                uint64_t virtual_end{ 0 };
                if (!multiply_without_overflow(mapping.page_count.value,
                                               core::k_transition_page_size, byte_count) ||
                    !add_without_overflow(mapping.physical_start.value, byte_count, physical_end))
                    return page_table_error_t::physical_range_overflow;
                if (!add_without_overflow(mapping.virtual_start.value, byte_count, virtual_end))
                    return page_table_error_t::virtual_range_overflow;
                if (physical_end > physical_limit)
                    return page_table_error_t::unsupported_physical_range;
                if (!is_canonical(mapping.virtual_start.value) ||
                    !is_canonical(virtual_end - 1) ||
                    is_upper(mapping.virtual_start.value) != is_upper(virtual_end - 1))
                    return page_table_error_t::noncanonical_virtual_address;

                const bool temporary{
                    (mapping.flags & core::k_transition_mapping_temporary_identity) != 0
                };
                if (temporary && mapping.virtual_start.value != mapping.physical_start.value)
                    return page_table_error_t::invalid_mapping_policy;
                if (mapping.memory_type == core::transition_memory_type_t::device)
                {
                    const bool reference_pl011{
                        mapping.physical_start.value ==
                            core::k_reference_pl011_physical_address &&
                        mapping.page_count.value == 1 &&
                        ((temporary && mapping.virtual_start.value ==
                                           core::k_reference_pl011_physical_address) ||
                         (!temporary && mapping.virtual_start.value ==
                                            core::k_reference_pl011_virtual_address))
                    };
                    const bool reference_gic_distributor{
                        !temporary && mapping.physical_start.value ==
                            core::k_reference_gic_distributor_physical_address &&
                        mapping.virtual_start.value ==
                            core::k_reference_gic_distributor_virtual_address &&
                        mapping.page_count.value ==
                            core::k_reference_gic_distributor_page_count
                    };
                    const bool reference_gic_redistributor{
                        !temporary && mapping.physical_start.value ==
                            core::k_reference_gic_redistributor_physical_address &&
                        mapping.virtual_start.value ==
                            core::k_reference_gic_redistributor_virtual_address &&
                        mapping.page_count.value ==
                            core::k_reference_gic_redistributor_page_count
                    };
                    if ((!reference_pl011 && !reference_gic_distributor &&
                         !reference_gic_redistributor) ||
                        mapping.permissions != read_write ||
                        (!temporary && (mapping.virtual_start.value < core::k_mmio_virtual_start ||
                                        virtual_end > k_mmio_virtual_end)))
                        return page_table_error_t::invalid_mapping_policy;
                }
                else if (!temporary &&
                         mapping.virtual_start.value >= core::k_direct_map_virtual_bias &&
                         mapping.virtual_start.value < core::k_mmio_virtual_start)
                {
                    if (mapping.virtual_start.value - core::k_direct_map_virtual_bias !=
                            mapping.physical_start.value ||
                        (mapping.permissions & core::k_transition_permission_execute) != 0)
                        return page_table_error_t::invalid_mapping_policy;
                }

                if ((mapping.permissions & core::k_transition_permission_execute) != 0)
                {
                    executable_identity_present |= temporary;
                    executable_stable_present |= !temporary &&
                        mapping.virtual_start.value >= core::k_kernel_virtual_bias;
                }

                for (uint32_t previous{ 0 }; previous < index; ++previous)
                {
                    const core::transition_mapping_t& other{ plan.mappings[previous] };
                    uint64_t other_bytes{ 0 };
                    uint64_t other_physical_end{ 0 };
                    uint64_t other_virtual_end{ 0 };
                    if (!multiply_without_overflow(other.page_count.value,
                                                   core::k_transition_page_size, other_bytes) ||
                        !add_without_overflow(other.physical_start.value, other_bytes,
                                              other_physical_end) ||
                        !add_without_overflow(other.virtual_start.value, other_bytes,
                                              other_virtual_end))
                        return page_table_error_t::invalid_plan;
                    if (ranges_overlap(mapping.virtual_start.value, virtual_end,
                                       other.virtual_start.value, other_virtual_end))
                        return page_table_error_t::mapping_conflict;
                    if (!ranges_overlap(mapping.physical_start.value, physical_end,
                                        other.physical_start.value, other_physical_end))
                        continue;
                    if (mapping.memory_type != other.memory_type)
                        return page_table_error_t::invalid_mapping_policy;
                    const bool writable_executable{
                        ((mapping.permissions & core::k_transition_permission_write) != 0 &&
                         (other.permissions & core::k_transition_permission_execute) != 0) ||
                        ((other.permissions & core::k_transition_permission_write) != 0 &&
                         (mapping.permissions & core::k_transition_permission_execute) != 0)
                    };
                    if (writable_executable)
                        return page_table_error_t::writable_executable_alias;
                }
            }

            return executable_identity_present && executable_stable_present ?
                page_table_error_t::success : page_table_error_t::invalid_plan;
        }

        [[nodiscard]] bool storage_is_zero(const page_table_storage_t& storage) noexcept
        {
            for (uint64_t index{ 0 }; index < k_page_table_entry_count; ++index)
            {
                if (storage.empty_root_writable[index] != 0) return false;
            }
            const uint64_t entry_count{
                storage.table_page_capacity.value * k_page_table_entry_count
            };
            for (uint64_t index{ 0 }; index < entry_count; ++index)
            {
                if (storage.table_pages_writable[index] != 0) return false;
            }
            return true;
        }

        [[nodiscard]] page_table_error_t allocate_table(
            build_context_t& context,
            uint64_t*& writable,
            uint64_t& physical_address) noexcept
        {
            if (context.next_page >= context.storage.table_page_capacity.value)
                return page_table_error_t::table_capacity_exceeded;
            uint64_t offset{ 0 };
            if (!multiply_without_overflow(context.next_page, core::k_transition_page_size, offset) ||
                !add_without_overflow(context.storage.table_pages_physical_address.value,
                                      offset, physical_address) ||
                physical_address >= context.physical_limit)
                return page_table_error_t::physical_range_overflow;
            writable = context.storage.table_pages_writable +
                context.next_page * k_page_table_entry_count;
            ++context.next_page;
            return page_table_error_t::success;
        }

        [[nodiscard]] page_table_error_t table_pointer_for_build(
            build_context_t& context,
            uint64_t descriptor,
            uint64_t*& writable) noexcept
        {
            if ((descriptor & ~k_table_allowed_mask) != 0 ||
                (descriptor & (k_descriptor_valid | k_descriptor_table_or_page)) !=
                    (k_descriptor_valid | k_descriptor_table_or_page) ||
                (descriptor & k_table_uxn) == 0)
                return page_table_error_t::invalid_table_descriptor;
            const uint64_t physical_address{ descriptor & k_address_mask };
            if (physical_address < context.storage.table_pages_physical_address.value)
                return page_table_error_t::invalid_table_descriptor;
            const uint64_t offset{
                physical_address - context.storage.table_pages_physical_address.value
            };
            if ((offset & k_page_mask) != 0 ||
                offset / core::k_transition_page_size >= context.next_page)
                return page_table_error_t::invalid_table_descriptor;
            writable = context.storage.table_pages_writable +
                (offset / core::k_transition_page_size) * k_page_table_entry_count;
            return page_table_error_t::success;
        }

        [[nodiscard]] page_table_error_t map_page(
            build_context_t& context,
            uint64_t* root,
            const core::transition_mapping_t& mapping,
            uint64_t virtual_address,
            uint64_t physical_address) noexcept
        {
            uint64_t* table{ root };
            constexpr uint32_t shifts[]{ 39, 30, 21 };
            for (uint32_t level{ 0 }; level < 3; ++level)
            {
                const uint64_t index{ (virtual_address >> shifts[level]) & 0x1ff };
                uint64_t& descriptor{ table[index] };
                if (descriptor == 0)
                {
                    uint64_t* child{ nullptr };
                    uint64_t child_physical{ 0 };
                    const page_table_error_t allocation_result{
                        allocate_table(context, child, child_physical)
                    };
                    if (allocation_result != page_table_error_t::success)
                        return allocation_result;
                    descriptor = table_descriptor(child_physical);
                    table = child;
                }
                else
                {
                    const page_table_error_t pointer_result{
                        table_pointer_for_build(context, descriptor, table)
                    };
                    if (pointer_result != page_table_error_t::success)
                        return pointer_result;
                }
            }

            const uint64_t leaf_index{ (virtual_address >> 12) & 0x1ff };
            if (table[leaf_index] != 0) return page_table_error_t::mapping_conflict;
            table[leaf_index] = page_descriptor(mapping, physical_address);
            return page_table_error_t::success;
        }

        [[nodiscard]] page_table_error_t physical_to_page_index(
            const page_table_storage_t& storage,
            uint64_t physical_address,
            uint64_t page_count,
            uint64_t& index) noexcept
        {
            if (physical_address < storage.table_pages_physical_address.value)
                return page_table_error_t::invalid_table_descriptor;
            const uint64_t offset{ physical_address - storage.table_pages_physical_address.value };
            if ((offset & k_page_mask) != 0)
                return page_table_error_t::invalid_table_descriptor;
            index = offset / core::k_transition_page_size;
            if (index >= page_count)
                return page_table_error_t::invalid_table_descriptor;
            return page_table_error_t::success;
        }

        [[nodiscard]] page_table_error_t walk_table(
            walk_context_t& context,
            uint64_t page_index,
            uint32_t level) noexcept
        {
            const uint64_t* table{
                context.storage.table_pages_writable + page_index * k_page_table_entry_count
            };
            for (uint64_t index{ 0 }; index < k_page_table_entry_count; ++index)
            {
                const uint64_t descriptor{ table[index] };
                if (descriptor == 0) continue;
                if (level == 3)
                {
                    if ((descriptor & ~k_page_allowed_mask) != 0 ||
                        (descriptor & (k_descriptor_valid | k_descriptor_table_or_page)) !=
                            (k_descriptor_valid | k_descriptor_table_or_page) ||
                        (descriptor & k_access_flag) == 0 ||
                        (descriptor & k_unprivileged_execute_never) == 0)
                        return page_table_error_t::invalid_page_descriptor;
                    const uint64_t attribute_index{ (descriptor >> 2) & 7 };
                    const uint64_t shareability{ (descriptor >> 8) & 3 };
                    const uint64_t access_permission{ (descriptor >> 6) & 3 };
                    if ((attribute_index == 0 && shareability != 3) ||
                        (attribute_index == 1 && shareability != 2) ||
                        attribute_index > 1 ||
                        (access_permission != 0 && access_permission != 2))
                        return page_table_error_t::invalid_page_descriptor;
                    const uint64_t physical_limit{
                        UINT64_C(1) << context.configuration.physical_address_bits
                    };
                    if ((descriptor & k_address_mask) >= physical_limit)
                        return page_table_error_t::invalid_page_descriptor;
                    ++context.leaf_count;
                    continue;
                }

                if ((descriptor & ~k_table_allowed_mask) != 0 ||
                    (descriptor & (k_descriptor_valid | k_descriptor_table_or_page)) !=
                        (k_descriptor_valid | k_descriptor_table_or_page) ||
                    (descriptor & k_table_uxn) == 0)
                    return page_table_error_t::invalid_table_descriptor;
                uint64_t child_index{ 0 };
                const page_table_error_t index_result{
                    physical_to_page_index(context.storage, descriptor & k_address_mask,
                                           context.configuration.table_page_count.value,
                                           child_index)
                };
                if (index_result != page_table_error_t::success) return index_result;
                if (context.visited[child_index] != 0)
                    return page_table_error_t::invalid_table_descriptor;
                context.visited[child_index] = 1;
                const page_table_error_t child_result{
                    walk_table(context, child_index, level + 1)
                };
                if (child_result != page_table_error_t::success) return child_result;
            }
            return page_table_error_t::success;
        }

        [[nodiscard]] lookup_result_t lookup(
            const page_table_storage_t& storage,
            const translation_configuration_t& configuration,
            uint64_t virtual_address) noexcept
        {
            const uint64_t root_physical_address{
                is_upper(virtual_address) ? configuration.ttbr1_root_physical_address.value :
                                            configuration.ttbr0_root_physical_address.value
            };
            uint64_t page_index{ 0 };
            page_table_error_t result{
                physical_to_page_index(storage, root_physical_address,
                                       configuration.table_page_count.value, page_index)
            };
            if (result != page_table_error_t::success) return { result, false, 0 };

            constexpr uint32_t shifts[]{ 39, 30, 21 };
            for (uint32_t level{ 0 }; level < 3; ++level)
            {
                const uint64_t* table{
                    storage.table_pages_writable + page_index * k_page_table_entry_count
                };
                const uint64_t descriptor{ table[(virtual_address >> shifts[level]) & 0x1ff] };
                if (descriptor == 0) return { page_table_error_t::success, false, 0 };
                if ((descriptor & ~k_table_allowed_mask) != 0 ||
                    (descriptor & (k_descriptor_valid | k_descriptor_table_or_page)) !=
                        (k_descriptor_valid | k_descriptor_table_or_page) ||
                    (descriptor & k_table_uxn) == 0)
                    return { page_table_error_t::invalid_table_descriptor, false, 0 };
                result = physical_to_page_index(storage, descriptor & k_address_mask,
                                                configuration.table_page_count.value, page_index);
                if (result != page_table_error_t::success) return { result, false, 0 };
            }

            const uint64_t* table{
                storage.table_pages_writable + page_index * k_page_table_entry_count
            };
            const uint64_t descriptor{ table[(virtual_address >> 12) & 0x1ff] };
            if (descriptor == 0) return { page_table_error_t::success, false, 0 };
            if ((descriptor & ~k_page_allowed_mask) != 0 ||
                (descriptor & (k_descriptor_valid | k_descriptor_table_or_page)) !=
                    (k_descriptor_valid | k_descriptor_table_or_page))
                return { page_table_error_t::invalid_page_descriptor, false, 0 };
            return { page_table_error_t::success, true, descriptor };
        }

        [[nodiscard]] mapping_check_t check_leaf(
            const page_table_storage_t& storage,
            const translation_configuration_t& configuration,
            uint64_t virtual_address,
            uint64_t physical_address,
            core::transition_memory_type_t memory_type,
            uint32_t permissions) noexcept
        {
            if ((virtual_address & k_page_mask) != (physical_address & k_page_mask))
                return mapping_check_t::missing;

            const lookup_result_t found{ lookup(storage, configuration, virtual_address) };
            if (found.error != page_table_error_t::success)
                return mapping_check_t::invalid;
            if (!found.present) return mapping_check_t::missing;

            core::transition_mapping_t expected{};
            expected.memory_type = memory_type;
            expected.permissions = permissions;
            const uint64_t physical_page{ physical_address & ~k_page_mask };
            return found.descriptor == page_descriptor(expected, physical_page) ?
                mapping_check_t::success : mapping_check_t::missing;
        }

        [[nodiscard]] activation_preflight_error_t require_leaf(
            const page_table_storage_t& storage,
            const translation_configuration_t& configuration,
            uint64_t virtual_address,
            uint64_t physical_address,
            core::transition_memory_type_t memory_type,
            uint32_t permissions,
            activation_preflight_error_t missing_error) noexcept
        {
            const mapping_check_t result{
                check_leaf(storage, configuration, virtual_address, physical_address,
                           memory_type, permissions)
            };
            if (result == mapping_check_t::invalid)
                return activation_preflight_error_t::invalid_tables;
            return result == mapping_check_t::success ?
                activation_preflight_error_t::success : missing_error;
        }
    } // anonymous namespace

    page_table_error_t build_page_tables(
        uint64_t id_aa64mmfr0_el1,
        const core::transition_plan_t& plan,
        const page_table_storage_t& storage,
        translation_configuration_t& configuration) noexcept
    {
        clear_configuration(configuration);
        feature_configuration_t features{};
        page_table_error_t result{ discover_features(id_aa64mmfr0_el1, features) };
        if (result != page_table_error_t::success) return result;
        result = validate_storage(plan, storage);
        if (result != page_table_error_t::success) return result;
        result = validate_plan(plan, features.physical_limit);
        if (result != page_table_error_t::success) return result;
        if (!storage_is_zero(storage)) return page_table_error_t::storage_not_zero;

        build_context_t context{ storage, features.physical_limit, 0 };
        uint64_t* ttbr0_root{ nullptr };
        uint64_t* ttbr1_root{ nullptr };
        uint64_t ttbr0_physical{ 0 };
        uint64_t ttbr1_physical{ 0 };
        result = allocate_table(context, ttbr0_root, ttbr0_physical);
        if (result != page_table_error_t::success) return result;
        result = allocate_table(context, ttbr1_root, ttbr1_physical);
        if (result != page_table_error_t::success) return result;

        for (uint32_t mapping_index{ 0 }; mapping_index < plan.mapping_count; ++mapping_index)
        {
            const core::transition_mapping_t& mapping{ plan.mappings[mapping_index] };
            for (uint64_t page{ 0 }; page < mapping.page_count.value; ++page)
            {
                const uint64_t offset{ page * core::k_transition_page_size };
                result = map_page(context,
                                  is_upper(mapping.virtual_start.value) ? ttbr1_root : ttbr0_root,
                                  mapping,
                                  mapping.virtual_start.value + offset,
                                  mapping.physical_start.value + offset);
                if (result != page_table_error_t::success) return result;
            }
        }

        translation_configuration_t candidate{};
        candidate.ttbr0_root_physical_address = { ttbr0_physical };
        candidate.ttbr1_root_physical_address = { ttbr1_physical };
        candidate.table_page_count = { context.next_page };
        candidate.mair_el1 = k_mair_el1;
        candidate.tcr_el1 = features.tcr_el1;
        candidate.sctlr_el1 = k_sctlr_el1_owned;
        candidate.physical_address_bits = features.physical_address_bits;
        candidate.ips_encoding = features.ips_encoding;
        configuration = candidate;
        return page_table_error_t::success;
    }

    page_table_error_t audit_page_tables(
        uint64_t id_aa64mmfr0_el1,
        const core::transition_plan_t& plan,
        const page_table_storage_t& storage,
        const translation_configuration_t& configuration) noexcept
    {
        feature_configuration_t features{};
        page_table_error_t result{ discover_features(id_aa64mmfr0_el1, features) };
        if (result != page_table_error_t::success) return result;
        result = validate_storage(plan, storage);
        if (result != page_table_error_t::success) return result;
        result = validate_plan(plan, features.physical_limit);
        if (result != page_table_error_t::success) return result;
        if (configuration.table_page_count.value < 2 ||
            configuration.table_page_count.value > storage.table_page_capacity.value ||
            configuration.ttbr0_root_physical_address.value !=
                storage.table_pages_physical_address.value ||
            configuration.ttbr1_root_physical_address.value !=
                storage.table_pages_physical_address.value + core::k_transition_page_size ||
            configuration.mair_el1 != k_mair_el1 ||
            configuration.tcr_el1 != features.tcr_el1 ||
            configuration.sctlr_el1 != k_sctlr_el1_owned ||
            configuration.physical_address_bits != features.physical_address_bits ||
            configuration.ips_encoding != features.ips_encoding)
            return page_table_error_t::invalid_plan;

        uint64_t ttbr0_index{ 0 };
        uint64_t ttbr1_index{ 0 };
        result = physical_to_page_index(storage, configuration.ttbr0_root_physical_address.value,
                                        configuration.table_page_count.value, ttbr0_index);
        if (result != page_table_error_t::success) return result;
        result = physical_to_page_index(storage, configuration.ttbr1_root_physical_address.value,
                                        configuration.table_page_count.value, ttbr1_index);
        if (result != page_table_error_t::success || ttbr0_index == ttbr1_index) return result ==
            page_table_error_t::success ? page_table_error_t::invalid_plan : result;

        for (uint64_t index{ 0 }; index < k_page_table_entry_count; ++index)
        {
            if (storage.empty_root_writable[index] != 0)
                return page_table_error_t::invalid_table_descriptor;
        }
        const uint64_t used_entries{
            configuration.table_page_count.value * k_page_table_entry_count
        };
        const uint64_t capacity_entries{
            storage.table_page_capacity.value * k_page_table_entry_count
        };
        for (uint64_t index{ used_entries }; index < capacity_entries; ++index)
        {
            if (storage.table_pages_writable[index] != 0)
                return page_table_error_t::unreachable_table;
        }

        uint64_t visited[core::k_transition_table_page_count];
        for (uint64_t index{ 0 }; index < core::k_transition_table_page_count; ++index)
            visited[index] = 0;
        visited[ttbr0_index] = 1;
        visited[ttbr1_index] = 1;
        walk_context_t context{ storage, configuration, visited, 0 };
        result = walk_table(context, ttbr0_index, 0);
        if (result != page_table_error_t::success) return result;
        result = walk_table(context, ttbr1_index, 0);
        if (result != page_table_error_t::success) return result;
        for (uint64_t index{ 0 }; index < configuration.table_page_count.value; ++index)
        {
            if (visited[index] == 0) return page_table_error_t::unreachable_table;
        }

        uint64_t expected_leaf_count{ 0 };
        for (uint32_t mapping_index{ 0 }; mapping_index < plan.mapping_count; ++mapping_index)
        {
            const core::transition_mapping_t& mapping{ plan.mappings[mapping_index] };
            if (!add_without_overflow(expected_leaf_count, mapping.page_count.value,
                                      expected_leaf_count))
                return page_table_error_t::invalid_plan;
            for (uint64_t page{ 0 }; page < mapping.page_count.value; ++page)
            {
                const uint64_t offset{ page * core::k_transition_page_size };
                const lookup_result_t found{
                    lookup(storage, configuration, mapping.virtual_start.value + offset)
                };
                if (found.error != page_table_error_t::success) return found.error;
                if (!found.present || found.descriptor !=
                        page_descriptor(mapping, mapping.physical_start.value + offset))
                    return page_table_error_t::missing_mapping;
            }
        }
        const uint64_t guards[]{
            core::k_early_stack_guard_start,
            core::k_early_stack_virtual_top,
        };
        for (const uint64_t guard : guards)
        {
            const lookup_result_t found{ lookup(storage, configuration, guard) };
            if (found.error != page_table_error_t::success) return found.error;
            if (found.present) return page_table_error_t::mapped_stack_guard;
        }

        if (context.leaf_count != expected_leaf_count)
            return page_table_error_t::unexpected_leaf;

        return page_table_error_t::success;
    }

    activation_preflight_error_t preflight_activation(
        uint64_t id_aa64mmfr0_el1,
        const core::transition_plan_t& plan,
        const page_table_storage_t& storage,
        const translation_configuration_t& configuration,
        const activation_preflight_t& preflight) noexcept
    {
        if (audit_page_tables(id_aa64mmfr0_el1, plan, storage, configuration) !=
            page_table_error_t::success)
            return activation_preflight_error_t::invalid_tables;

        uint64_t expected_boot_virtual{ 0 };
        uint64_t expected_arena_virtual{ 0 };
        if (preflight.current_program_counter.value == 0 ||
            preflight.current_stack_pointer.value == 0 ||
            (preflight.current_stack_pointer.value & 0xf) != 0 ||
            preflight.boot_information_physical_address.value == 0 ||
            preflight.target_program_counter_physical_address.value == 0 ||
            preflight.stable_vectors_physical_address.value == 0 ||
            (preflight.stable_vectors_physical_address.value & 0x7ff) != 0 ||
            (preflight.stable_vectors_virtual_address.value & 0x7ff) != 0 ||
            preflight.target_program_counter_virtual_address.value <
                core::k_kernel_virtual_bias ||
            preflight.stable_vectors_virtual_address.value <
                core::k_kernel_virtual_bias ||
            preflight.owned_stack_pointer.value != plan.early_stack_virtual_top.value ||
            !add_without_overflow(core::k_direct_map_virtual_bias,
                                  preflight.boot_information_physical_address.value,
                                  expected_boot_virtual) ||
            !add_without_overflow(core::k_direct_map_virtual_bias,
                                  plan.arena_physical_start.value,
                                  expected_arena_virtual) ||
            preflight.boot_information_virtual_address.value != expected_boot_virtual ||
            preflight.arena_virtual_address.value != expected_arena_virtual ||
            preflight.console_virtual_address.value !=
                core::k_reference_pl011_virtual_address ||
            preflight.gic_distributor_virtual_address.value !=
                core::k_reference_gic_distributor_virtual_address ||
            preflight.gic_redistributor_virtual_address.value !=
                core::k_reference_gic_redistributor_virtual_address)
            return activation_preflight_error_t::invalid_runtime_state;

        constexpr uint32_t read{ core::k_transition_permission_read };
        constexpr uint32_t read_write{
            read | core::k_transition_permission_write
        };
        constexpr uint32_t read_execute{
            read | core::k_transition_permission_execute
        };
        activation_preflight_error_t result{ require_leaf(
            storage, configuration,
            preflight.current_program_counter.value,
            preflight.current_program_counter.value,
            core::transition_memory_type_t::normal,
            read_execute,
            activation_preflight_error_t::missing_current_program_counter)
        };
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.gic_distributor_virtual_address.value,
            core::k_reference_gic_distributor_physical_address,
            core::transition_memory_type_t::device,
            read_write,
            activation_preflight_error_t::missing_gic_distributor_alias);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.gic_redistributor_virtual_address.value,
            core::k_reference_gic_redistributor_physical_address,
            core::transition_memory_type_t::device,
            read_write,
            activation_preflight_error_t::missing_gic_redistributor_alias);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.current_stack_pointer.value - 1,
            preflight.current_stack_pointer.value - 1,
            core::transition_memory_type_t::normal,
            read_write,
            activation_preflight_error_t::missing_current_stack);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.boot_information_physical_address.value,
            preflight.boot_information_physical_address.value,
            core::transition_memory_type_t::normal,
            read,
            activation_preflight_error_t::missing_boot_information_identity);
        if (result != activation_preflight_error_t::success) return result;

        const uint64_t table_identity_addresses[]{
            plan.empty_root_physical_address.value,
            configuration.ttbr0_root_physical_address.value,
            configuration.ttbr1_root_physical_address.value,
        };
        for (const uint64_t address : table_identity_addresses)
        {
            result = require_leaf(
                storage, configuration, address, address,
                core::transition_memory_type_t::normal,
                read_write,
                activation_preflight_error_t::missing_table_identity);
            if (result != activation_preflight_error_t::success) return result;
        }

        result = require_leaf(
            storage, configuration,
            plan.console_physical_address.value,
            plan.console_physical_address.value,
            core::transition_memory_type_t::device,
            read_write,
            activation_preflight_error_t::missing_console_identity);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.target_program_counter_virtual_address.value,
            preflight.target_program_counter_physical_address.value,
            core::transition_memory_type_t::normal,
            read_execute,
            activation_preflight_error_t::missing_target_program_counter);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.stable_vectors_virtual_address.value,
            preflight.stable_vectors_physical_address.value,
            core::transition_memory_type_t::normal,
            read_execute,
            activation_preflight_error_t::missing_stable_vectors);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.owned_stack_pointer.value - 1,
            plan.early_stack_physical_start.value +
                plan.early_stack_page_count.value * core::k_transition_page_size - 1,
            core::transition_memory_type_t::normal,
            read_write,
            activation_preflight_error_t::missing_owned_stack);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.boot_information_virtual_address.value,
            preflight.boot_information_physical_address.value,
            core::transition_memory_type_t::normal,
            read,
            activation_preflight_error_t::missing_boot_information_alias);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.arena_virtual_address.value,
            plan.arena_physical_start.value,
            core::transition_memory_type_t::normal,
            read_write,
            activation_preflight_error_t::missing_arena_alias);
        if (result != activation_preflight_error_t::success) return result;

        result = require_leaf(
            storage, configuration,
            preflight.console_virtual_address.value,
            plan.console_physical_address.value,
            core::transition_memory_type_t::device,
            read_write,
            activation_preflight_error_t::missing_console_alias);
        if (result != activation_preflight_error_t::success) return result;

        const uint64_t guards[]{
            core::k_early_stack_guard_start,
            preflight.owned_stack_pointer.value,
        };
        for (const uint64_t guard : guards)
        {
            const lookup_result_t found{ lookup(storage, configuration, guard) };
            if (found.error != page_table_error_t::success)
                return activation_preflight_error_t::invalid_tables;
            if (found.present)
                return activation_preflight_error_t::mapped_stack_guard;
        }

        return activation_preflight_error_t::success;
    }
} // namespace burrow::arch::aarch64
