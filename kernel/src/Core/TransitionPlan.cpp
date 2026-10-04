//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/TransitionPlan.h>

namespace burrow::core {
    namespace {
        struct range_t
        {
            uint64_t begin;
            uint64_t end;
        };

        struct source_identity_t
        {
            uint32_t source_kind;
            uint32_t source_type;
            uint64_t source_attributes;
        };

        constexpr uint32_t k_maximum_exclusion_count{ 8 };

        [[nodiscard]] bool add_without_overflow(uint64_t left, uint64_t right, uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(uint64_t left, uint64_t right, uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left) return false;

            result = left * right;
            return true;
        }

        [[nodiscard]] bool ranges_overlap(const range_t& left, const range_t& right) noexcept
        {
            return left.begin < right.end && right.begin < left.end;
        }

        [[nodiscard]] bool source_identity_matches(const source_identity_t& left,
                                                   const source_identity_t& right) noexcept
        {
            return left.source_kind == right.source_kind && left.source_type == right.source_type &&
                left.source_attributes == right.source_attributes;
        }

        void clear_view(validated_boot_information_t& view) noexcept
        {
            view.object = nullptr;
            view.physical_address = { 0 };
            view.byte_count = { 0 };
            view.memory_entries = nullptr;
            view.memory_entry_count = 0;
            view.reserved = 0;
            view.console_physical_address = { 0 };
        }

        void clear_mapping(transition_mapping_t& mapping) noexcept
        {
            mapping.physical_start = { 0 };
            mapping.virtual_start = { 0 };
            mapping.page_count = { 0 };
            mapping.memory_type = transition_memory_type_t::normal;
            mapping.permissions = 0;
            mapping.flags = 0;
            mapping.reserved = 0;
        }

        void clear_plan(transition_plan_t& plan) noexcept
        {
            for (uint32_t index{ 0 }; index < k_transition_mapping_capacity; ++index)
                clear_mapping(plan.mappings[index]);

            plan.mapping_count = 0;
            plan.reserved = 0;
            plan.arena_physical_start = { 0 };
            plan.arena_page_count = { 0 };
            plan.empty_root_physical_address = { 0 };
            plan.early_stack_physical_start = { 0 };
            plan.early_stack_page_count = { 0 };
            plan.page_table_physical_start = { 0 };
            plan.page_table_page_count = { 0 };
            plan.boot_information_physical_start = { 0 };
            plan.boot_information_page_count = { 0 };
            plan.bootstrap_stack_physical_start = { 0 };
            plan.bootstrap_stack_page_count = { 0 };
            plan.console_physical_address = { 0 };
            plan.early_stack_virtual_start = { 0 };
            plan.early_stack_virtual_top = { 0 };
        }

        void copy_plan(transition_plan_t& destination, const transition_plan_t& source) noexcept
        {
            for (uint32_t index{ 0 }; index < k_transition_mapping_capacity; ++index)
                destination.mappings[index] = source.mappings[index];

            destination.mapping_count = source.mapping_count;
            destination.reserved = source.reserved;
            destination.arena_physical_start = source.arena_physical_start;
            destination.arena_page_count = source.arena_page_count;
            destination.empty_root_physical_address = source.empty_root_physical_address;
            destination.early_stack_physical_start = source.early_stack_physical_start;
            destination.early_stack_page_count = source.early_stack_page_count;
            destination.page_table_physical_start = source.page_table_physical_start;
            destination.page_table_page_count = source.page_table_page_count;
            destination.boot_information_physical_start = source.boot_information_physical_start;
            destination.boot_information_page_count = source.boot_information_page_count;
            destination.bootstrap_stack_physical_start = source.bootstrap_stack_physical_start;
            destination.bootstrap_stack_page_count = source.bootstrap_stack_page_count;
            destination.console_physical_address = source.console_physical_address;
            destination.early_stack_virtual_start = source.early_stack_virtual_start;
            destination.early_stack_virtual_top = source.early_stack_virtual_top;
        }

        [[nodiscard]] bool make_page_cover(uint64_t physical_start, uint64_t byte_count, range_t& range) noexcept
        {
            if (byte_count == 0) return false;

            uint64_t byte_end{ 0 };
            if (!add_without_overflow(physical_start, byte_count, byte_end) ||
                byte_end > UINT64_MAX - (k_transition_page_size - 1))
                return false;

            range.begin = physical_start & ~(k_transition_page_size - 1);
            range.end = (byte_end + k_transition_page_size - 1) & ~(k_transition_page_size - 1);
            return range.end > range.begin;
        }

        [[nodiscard]] bool append_exclusion(range_t* exclusions, uint32_t& count,
                                            uint64_t physical_start, uint64_t byte_count) noexcept
        {
            if (count >= k_maximum_exclusion_count) return false;

            range_t range{};
            if (!make_page_cover(physical_start, byte_count, range)) return false;

            exclusions[count++] = range;
            return true;
        }

        [[nodiscard]] transition_plan_error_t build_exclusions(
            const validated_boot_information_t& view,
            range_t* exclusions,
            uint32_t& exclusion_count) noexcept
        {
            exclusion_count = 0;
            const warren_boot_information_t& object{ *view.object };

            if (!append_exclusion(exclusions, exclusion_count, object.self_physical_address, object.total_size) ||
                !append_exclusion(exclusions, exclusion_count, object.kernel_physical_start,
                                  object.kernel_physical_size) ||
                !append_exclusion(exclusions, exclusion_count, object.bootstrap_stack_physical_start,
                                  object.bootstrap_stack_size))
                return transition_plan_error_t::physical_range_overflow;

            if ((object.present_features & WARREN_BOOT_FEATURE_INITIAL_IMAGE) != 0 &&
                !append_exclusion(exclusions, exclusion_count, object.initial_image_physical_start,
                                  object.initial_image_size))
                return transition_plan_error_t::physical_range_overflow;

            if ((object.present_features & WARREN_BOOT_FEATURE_ACPI_RSDP) != 0 &&
                !append_exclusion(exclusions, exclusion_count, object.acpi_rsdp_physical_address, 1))
                return transition_plan_error_t::physical_range_overflow;

            if ((object.present_features & WARREN_BOOT_FEATURE_DEVICE_TREE) != 0 &&
                !append_exclusion(exclusions, exclusion_count, object.device_tree_physical_address,
                                  object.device_tree_size))
                return transition_plan_error_t::physical_range_overflow;

            if ((object.present_features & WARREN_BOOT_FEATURE_FRAMEBUFFER) != 0)
            {
                const auto* bytes{ reinterpret_cast<const uint8_t*>(view.object) };
                const auto* framebuffer{ reinterpret_cast<const warren_boot_framebuffer_t*>(
                    bytes + object.framebuffer.offset) };
                if (!append_exclusion(exclusions, exclusion_count, framebuffer->physical_address,
                                      framebuffer->size))
                    return transition_plan_error_t::physical_range_overflow;
            }

            if (!append_exclusion(exclusions, exclusion_count, view.console_physical_address.value,
                                  k_transition_page_size))
                return transition_plan_error_t::physical_range_overflow;

            return transition_plan_error_t::success;
        }

        [[nodiscard]] bool select_from_run(const range_t& run,
                                           const range_t* exclusions,
                                           uint32_t exclusion_count,
                                           uint64_t arena_byte_count,
                                           uint64_t& selected_start) noexcept
        {
            uint64_t candidate{ run.begin };

            while (candidate <= run.end)
            {
                uint64_t candidate_end{ 0 };
                if (!add_without_overflow(candidate, arena_byte_count, candidate_end) || candidate_end > run.end)
                    return false;

                bool moved{ false };
                const range_t candidate_range{ candidate, candidate_end };
                for (uint32_t index{ 0 }; index < exclusion_count; ++index)
                {
                    if (!ranges_overlap(candidate_range, exclusions[index])) continue;

                    candidate = exclusions[index].end;
                    moved = true;
                    break;
                }

                if (!moved)
                {
                    selected_start = candidate;
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] transition_plan_error_t select_transition_arena(
            const validated_boot_information_t& view,
            const range_t* exclusions,
            uint32_t exclusion_count,
            uint64_t& arena_start) noexcept
        {
            uint64_t arena_byte_count{ 0 };
            if (!multiply_without_overflow(k_transition_arena_page_count, k_transition_page_size,
                                           arena_byte_count))
                return transition_plan_error_t::physical_range_overflow;

            bool run_present{ false };
            range_t run{};
            source_identity_t run_source{};
            uint64_t previous_end{ 0 };

            for (uint32_t index{ 0 }; index < view.memory_entry_count; ++index)
            {
                const warren_boot_memory_entry_t& entry{ view.memory_entries[index] };
                uint64_t entry_byte_count{ 0 };
                uint64_t entry_end{ 0 };
                if (entry.page_count == 0 || (entry.physical_start % k_transition_page_size) != 0 ||
                    !multiply_without_overflow(entry.page_count, k_transition_page_size, entry_byte_count) ||
                    !add_without_overflow(entry.physical_start, entry_byte_count, entry_end) ||
                    (index != 0 && entry.physical_start < previous_end))
                    return transition_plan_error_t::invalid_memory_map;

                previous_end = entry_end;
                if (entry.memory_kind != WARREN_BOOT_MEMORY_USABLE)
                {
                    if (run_present && select_from_run(run, exclusions, exclusion_count, arena_byte_count,
                                                      arena_start))
                        return transition_plan_error_t::success;

                    run_present = false;
                    continue;
                }

                const source_identity_t source{
                    entry.source_kind,
                    entry.source_type,
                    entry.source_attributes,
                };
                if (run_present && entry.physical_start == run.end && source_identity_matches(source, run_source))
                {
                    run.end = entry_end;
                    continue;
                }

                if (run_present && select_from_run(run, exclusions, exclusion_count, arena_byte_count, arena_start))
                    return transition_plan_error_t::success;

                run = { entry.physical_start, entry_end };
                run_source = source;
                run_present = true;
            }

            if (run_present && select_from_run(run, exclusions, exclusion_count, arena_byte_count, arena_start))
                return transition_plan_error_t::success;

            return transition_plan_error_t::transition_arena_exhausted;
        }

        [[nodiscard]] transition_plan_error_t append_mapping(
            transition_plan_t& plan,
            uint64_t physical_start,
            uint64_t virtual_start,
            uint64_t page_count,
            transition_memory_type_t memory_type,
            uint32_t permissions,
            uint32_t flags) noexcept
        {
            if (plan.mapping_count >= k_transition_mapping_capacity)
                return transition_plan_error_t::mapping_capacity_exceeded;

            constexpr uint32_t known_permissions{
                k_transition_permission_read | k_transition_permission_write | k_transition_permission_execute
            };
            if (page_count == 0 || (physical_start % k_transition_page_size) != 0 ||
                (virtual_start % k_transition_page_size) != 0 ||
                (permissions & k_transition_permission_read) == 0 ||
                (permissions & ~known_permissions) != 0 ||
                (permissions & (k_transition_permission_write | k_transition_permission_execute)) ==
                    (k_transition_permission_write | k_transition_permission_execute) ||
                (flags & ~k_transition_mapping_temporary_identity) != 0)
                return transition_plan_error_t::invalid_image_layout;

            uint64_t byte_count{ 0 };
            uint64_t physical_end{ 0 };
            uint64_t virtual_end{ 0 };
            if (!multiply_without_overflow(page_count, k_transition_page_size, byte_count) ||
                !add_without_overflow(physical_start, byte_count, physical_end))
                return transition_plan_error_t::physical_range_overflow;
            if (!add_without_overflow(virtual_start, byte_count, virtual_end))
                return transition_plan_error_t::virtual_range_overflow;

            const range_t requested{ virtual_start, virtual_end };
            for (uint32_t index{ 0 }; index < plan.mapping_count; ++index)
            {
                const transition_mapping_t& existing{ plan.mappings[index] };
                uint64_t existing_bytes{ 0 };
                uint64_t existing_end{ 0 };
                if (!multiply_without_overflow(existing.page_count.value, k_transition_page_size, existing_bytes) ||
                    !add_without_overflow(existing.virtual_start.value, existing_bytes, existing_end))
                    return transition_plan_error_t::virtual_range_overflow;
                if (ranges_overlap(requested, { existing.virtual_start.value, existing_end }))
                    return transition_plan_error_t::mapping_overlap;
            }

            transition_mapping_t& mapping{ plan.mappings[plan.mapping_count++] };
            mapping.physical_start = { physical_start };
            mapping.virtual_start = { virtual_start };
            mapping.page_count = { page_count };
            mapping.memory_type = memory_type;
            mapping.permissions = permissions;
            mapping.flags = flags;
            mapping.reserved = 0;
            return transition_plan_error_t::success;
        }

        [[nodiscard]] transition_plan_error_t validate_image_layout(
            const validated_boot_information_t& view,
            const transition_image_layout_t& image_layout) noexcept
        {
            if (image_layout.segment_count == 0 ||
                image_layout.segment_count > k_transition_image_segment_capacity || image_layout.reserved != 0)
                return transition_plan_error_t::invalid_image_layout;

            const warren_boot_information_t& object{ *view.object };
            uint64_t kernel_end{ 0 };
            if (!add_without_overflow(object.kernel_physical_start, object.kernel_physical_size, kernel_end))
                return transition_plan_error_t::physical_range_overflow;

            uint64_t previous_physical_end{ 0 };
            uint64_t previous_relative_end{ 0 };
            for (uint32_t index{ 0 }; index < image_layout.segment_count; ++index)
            {
                const transition_image_segment_t& segment{ image_layout.segments[index] };
                uint64_t byte_count{ 0 };
                uint64_t physical_end{ 0 };
                uint64_t relative_end{ 0 };
                uint64_t expected_physical{ 0 };
                uint64_t stable_start{ 0 };
                uint64_t stable_end{ 0 };
                if (segment.reserved != 0 || segment.page_count.value == 0 ||
                    (segment.physical_start.value % k_transition_page_size) != 0 ||
                    (segment.image_relative_start.value % k_transition_page_size) != 0 ||
                    !multiply_without_overflow(segment.page_count.value, k_transition_page_size, byte_count) ||
                    !add_without_overflow(segment.physical_start.value, byte_count, physical_end) ||
                    !add_without_overflow(segment.image_relative_start.value, byte_count, relative_end) ||
                    relative_end > k_kernel_image_relative_limit ||
                    !add_without_overflow(object.kernel_load_bias, segment.image_relative_start.value,
                                          expected_physical) ||
                    expected_physical != segment.physical_start.value ||
                    segment.physical_start.value < object.kernel_physical_start || physical_end > kernel_end ||
                    !add_without_overflow(k_kernel_virtual_bias, segment.image_relative_start.value, stable_start) ||
                    !add_without_overflow(stable_start, byte_count, stable_end) ||
                    (index != 0 && (segment.physical_start.value < previous_physical_end ||
                                    segment.image_relative_start.value < previous_relative_end)))
                    return transition_plan_error_t::invalid_image_layout;

                constexpr uint32_t known_permissions{
                    k_transition_permission_read | k_transition_permission_write | k_transition_permission_execute
                };
                if ((segment.permissions & k_transition_permission_read) == 0 ||
                    (segment.permissions & ~known_permissions) != 0 ||
                    (segment.permissions & (k_transition_permission_write | k_transition_permission_execute)) ==
                        (k_transition_permission_write | k_transition_permission_execute))
                    return transition_plan_error_t::invalid_image_layout;

                previous_physical_end = physical_end;
                previous_relative_end = relative_end;
            }

            return transition_plan_error_t::success;
        }
    } // anonymous namespace

    warren::boot::boot_information_error_t consume_boot_information(
        const void* object,
        uint32_t readable_size,
        uint64_t physical_address,
        validated_boot_information_t& view) noexcept
    {
        clear_view(view);
        const warren::boot::boot_information_error_t result{
            warren::boot::validate_boot_information(object, readable_size, physical_address)
        };
        if (result != warren::boot::boot_information_error_t::success)
            return result;

        const auto* header{ static_cast<const warren_boot_information_t*>(object) };
        const auto* bytes{ static_cast<const uint8_t*>(object) };
        physical_address_t console_physical_address{ 0 };
        if ((header->present_features & WARREN_BOOT_FEATURE_EARLY_CONSOLE) != 0)
        {
            const auto* console{ reinterpret_cast<const warren_boot_early_console_t*>(
                bytes + header->early_console.offset) };
            console_physical_address = { console->physical_address };
        }

        view.object = header;
        view.physical_address = { physical_address };
        view.byte_count = { header->total_size };
        view.memory_entries = reinterpret_cast<const warren_boot_memory_entry_t*>(
            bytes + header->memory_map.offset);
        view.memory_entry_count = header->memory_map.count;
        view.reserved = 0;
        view.console_physical_address = console_physical_address;
        return warren::boot::boot_information_error_t::success;
    }

    transition_plan_error_t plan_aarch64_transition(
        const validated_boot_information_t& view,
        const transition_image_layout_t& image_layout,
        transition_plan_t& plan) noexcept
    {
        clear_plan(plan);
        if (view.object == nullptr || view.memory_entries == nullptr || view.memory_entry_count == 0 ||
            view.reserved != 0 || view.byte_count.value != view.object->total_size ||
            view.physical_address.value != view.object->self_physical_address)
            return transition_plan_error_t::invalid_validated_view;
        if (view.console_physical_address.value != k_reference_pl011_physical_address)
            return transition_plan_error_t::invalid_console_aperture;

        const transition_plan_error_t image_result{ validate_image_layout(view, image_layout) };
        if (image_result != transition_plan_error_t::success) return image_result;

        range_t exclusions[k_maximum_exclusion_count]{};
        uint32_t exclusion_count{ 0 };
        const transition_plan_error_t exclusion_result{
            build_exclusions(view, exclusions, exclusion_count)
        };
        if (exclusion_result != transition_plan_error_t::success) return exclusion_result;

        uint64_t arena_start{ 0 };
        const transition_plan_error_t arena_result{
            select_transition_arena(view, exclusions, exclusion_count, arena_start)
        };
        if (arena_result != transition_plan_error_t::success) return arena_result;
        if (arena_start >= k_direct_map_physical_limit ||
            k_transition_arena_page_count >
                (k_direct_map_physical_limit - arena_start) / k_transition_page_size)
            return transition_plan_error_t::physical_range_overflow;

        transition_plan_t candidate;
        clear_plan(candidate);
        candidate.arena_physical_start = { arena_start };
        candidate.arena_page_count = { k_transition_arena_page_count };
        candidate.empty_root_physical_address = { arena_start };
        if (!add_without_overflow(arena_start, k_transition_page_size,
                                  candidate.early_stack_physical_start.value) ||
            !add_without_overflow(arena_start, 17 * k_transition_page_size,
                                  candidate.page_table_physical_start.value))
            return transition_plan_error_t::physical_range_overflow;
        candidate.early_stack_page_count = { k_transition_stack_page_count };
        candidate.page_table_page_count = { k_transition_table_page_count };
        candidate.early_stack_virtual_start = { k_early_stack_virtual_start };
        candidate.early_stack_virtual_top = { k_early_stack_virtual_top };

        transition_plan_error_t result{ transition_plan_error_t::success };
        for (uint32_t index{ 0 }; index < image_layout.segment_count; ++index)
        {
            const transition_image_segment_t& segment{ image_layout.segments[index] };
            result = append_mapping(candidate, segment.physical_start.value, segment.physical_start.value,
                                    segment.page_count.value, transition_memory_type_t::normal,
                                    segment.permissions, k_transition_mapping_temporary_identity);
            if (result != transition_plan_error_t::success) return result;
        }
        for (uint32_t index{ 0 }; index < image_layout.segment_count; ++index)
        {
            const transition_image_segment_t& segment{ image_layout.segments[index] };
            uint64_t stable_start{ 0 };
            if (!add_without_overflow(k_kernel_virtual_bias, segment.image_relative_start.value, stable_start))
                return transition_plan_error_t::virtual_range_overflow;
            result = append_mapping(candidate, segment.physical_start.value, stable_start,
                                    segment.page_count.value, transition_memory_type_t::normal,
                                    segment.permissions, 0);
            if (result != transition_plan_error_t::success) return result;
        }

        range_t boot_information_pages{};
        if (!make_page_cover(view.physical_address.value, view.byte_count.value, boot_information_pages))
            return transition_plan_error_t::physical_range_overflow;
        const uint64_t boot_information_page_count{
            (boot_information_pages.end - boot_information_pages.begin) / k_transition_page_size
        };
        if (boot_information_pages.end > k_direct_map_physical_limit)
            return transition_plan_error_t::physical_range_overflow;
        const warren_boot_information_t& object{ *view.object };
        candidate.boot_information_physical_start = { boot_information_pages.begin };
        candidate.boot_information_page_count = { boot_information_page_count };
        candidate.bootstrap_stack_physical_start = { object.bootstrap_stack_physical_start };
        candidate.bootstrap_stack_page_count = {
            object.bootstrap_stack_size / k_transition_page_size
        };
        candidate.console_physical_address = view.console_physical_address;

        result = append_mapping(candidate, boot_information_pages.begin, boot_information_pages.begin,
                                boot_information_page_count, transition_memory_type_t::normal,
                                k_transition_permission_read, k_transition_mapping_temporary_identity);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, boot_information_pages.begin,
                                k_direct_map_virtual_bias + boot_information_pages.begin,
                                boot_information_page_count, transition_memory_type_t::normal,
                                k_transition_permission_read, 0);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, object.bootstrap_stack_physical_start,
                                object.bootstrap_stack_physical_start,
                                object.bootstrap_stack_size / k_transition_page_size,
                                transition_memory_type_t::normal,
                                k_transition_permission_read | k_transition_permission_write,
                                k_transition_mapping_temporary_identity);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, arena_start, arena_start, k_transition_arena_page_count,
                                transition_memory_type_t::normal,
                                k_transition_permission_read | k_transition_permission_write,
                                k_transition_mapping_temporary_identity);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, arena_start, k_direct_map_virtual_bias + arena_start,
                                k_transition_arena_page_count, transition_memory_type_t::normal,
                                k_transition_permission_read | k_transition_permission_write, 0);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, k_reference_pl011_physical_address,
                                k_reference_pl011_physical_address, 1,
                                transition_memory_type_t::device,
                                k_transition_permission_read | k_transition_permission_write,
                                k_transition_mapping_temporary_identity);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, k_reference_pl011_physical_address,
                                k_reference_pl011_virtual_address, 1,
                                transition_memory_type_t::device,
                                k_transition_permission_read | k_transition_permission_write, 0);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, k_reference_gic_distributor_physical_address,
                                k_reference_gic_distributor_virtual_address,
                                k_reference_gic_distributor_page_count,
                                transition_memory_type_t::device,
                                k_transition_permission_read | k_transition_permission_write, 0);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, k_reference_gic_redistributor_physical_address,
                                k_reference_gic_redistributor_virtual_address,
                                k_reference_gic_redistributor_page_count,
                                transition_memory_type_t::device,
                                k_transition_permission_read | k_transition_permission_write, 0);
        if (result != transition_plan_error_t::success) return result;
        result = append_mapping(candidate, candidate.early_stack_physical_start.value,
                                k_early_stack_virtual_start, k_transition_stack_page_count,
                                transition_memory_type_t::normal,
                                k_transition_permission_read | k_transition_permission_write, 0);
        if (result != transition_plan_error_t::success) return result;

        copy_plan(plan, candidate);
        return transition_plan_error_t::success;
    }
} // namespace burrow::core
