//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/PhysicalMemory.h>

namespace burrow::core {
    namespace {
        constexpr uint32_t k_report_capacity{ 256 };

        struct range_t
        {
            uint64_t begin;
            uint64_t end;
        };

        struct report_builder_t
        {
            char bytes[k_report_capacity];
            uint32_t byte_count;
            bool valid;
        };

        [[nodiscard]] bool add_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            result = left + right;
            return result >= left;
        }

        [[nodiscard]] bool multiply_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left) return false;
            result = left * right;
            return true;
        }

        [[nodiscard]] bool ranges_overlap(
            const range_t& left,
            const range_t& right) noexcept
        {
            return left.begin < right.end && right.begin < left.end;
        }

        void clear_extent(physical_memory_extent_t& extent) noexcept
        {
            extent.physical_start = { 0 };
            extent.page_count = { 0 };
            extent.consumed_page_count = { 0 };
            extent.source_kind = 0;
            extent.source_type = 0;
            extent.source_attributes = 0;
        }

        void clear_state(physical_memory_state_t& state) noexcept
        {
            for (uint32_t index{ 0 }; index < k_physical_memory_extent_capacity;
                 ++index)
                clear_extent(state.extents[index]);
            state.extent_count = 0;
            state.reserved = 0;
            state.total_page_count = { 0 };
            state.consumed_page_count = { 0 };
            state.allocated_page_count = { 0 };
            state.excluded_page_count = { 0 };
            state.unmanaged_page_count = { 0 };
            state.transition_arena_physical_start = { 0 };
            state.transition_arena_page_count = { 0 };
            state.magic = 0;
        }

        void clear_allocation(boot_allocation_t& allocation) noexcept
        {
            allocation.physical_start = { 0 };
            allocation.page_count = { 0 };
            allocation.alignment_page_count = { 0 };
            allocation.consumed_page_count = { 0 };
        }

        void copy_state(
            physical_memory_state_t& destination,
            const physical_memory_state_t& source) noexcept
        {
            for (uint32_t index{ 0 }; index < k_physical_memory_extent_capacity;
                 ++index)
                destination.extents[index] = source.extents[index];
            destination.extent_count = source.extent_count;
            destination.reserved = source.reserved;
            destination.total_page_count = source.total_page_count;
            destination.consumed_page_count = source.consumed_page_count;
            destination.allocated_page_count = source.allocated_page_count;
            destination.excluded_page_count = source.excluded_page_count;
            destination.unmanaged_page_count = source.unmanaged_page_count;
            destination.transition_arena_physical_start =
                source.transition_arena_physical_start;
            destination.transition_arena_page_count =
                source.transition_arena_page_count;
            destination.magic = source.magic;
        }

        [[nodiscard]] bool append_extent(
            physical_memory_state_t& state,
            uint64_t begin,
            uint64_t end,
            const warren_boot_memory_entry_t& source) noexcept
        {
            if (begin == end) return true;
            if (begin > end || (begin % k_transition_page_size) != 0 ||
                (end % k_transition_page_size) != 0 ||
                state.extent_count >= k_physical_memory_extent_capacity)
                return false;

            const uint64_t page_count{ (end - begin) / k_transition_page_size };
            if (page_count == 0 ||
                state.total_page_count.value > UINT64_MAX - page_count)
                return false;

            physical_memory_extent_t& extent{
                state.extents[state.extent_count++]
            };
            extent.physical_start = { begin };
            extent.page_count = { page_count };
            extent.consumed_page_count = { 0 };
            extent.source_kind = source.source_kind;
            extent.source_type = source.source_type;
            extent.source_attributes = source.source_attributes;
            state.total_page_count.value += page_count;
            return true;
        }

        [[nodiscard]] bool state_is_valid(
            const physical_memory_state_t& state) noexcept
        {
            if (state.magic != k_physical_memory_state_magic ||
                state.extent_count == 0 ||
                state.extent_count > k_physical_memory_extent_capacity ||
                state.reserved != 0 ||
                state.transition_arena_page_count.value == 0 ||
                (state.transition_arena_physical_start.value %
                 k_transition_page_size) != 0 ||
                state.allocated_page_count.value >
                    state.consumed_page_count.value ||
                state.consumed_page_count.value > state.total_page_count.value)
                return false;

            uint64_t arena_bytes{ 0 };
            uint64_t arena_end{ 0 };
            if (!multiply_without_overflow(
                    state.transition_arena_page_count.value,
                    k_transition_page_size,
                    arena_bytes) ||
                !add_without_overflow(
                    state.transition_arena_physical_start.value,
                    arena_bytes,
                    arena_end))
                return false;
            const range_t arena{
                state.transition_arena_physical_start.value,
                arena_end,
            };

            uint64_t total_pages{ 0 };
            uint64_t consumed_pages{ 0 };
            uint64_t previous_end{ 0 };
            for (uint32_t index{ 0 }; index < state.extent_count; ++index)
            {
                const physical_memory_extent_t& extent{ state.extents[index] };
                uint64_t extent_bytes{ 0 };
                uint64_t extent_end{ 0 };
                if (extent.page_count.value == 0 ||
                    extent.consumed_page_count.value > extent.page_count.value ||
                    extent.physical_start.value < k_transition_page_size ||
                    (extent.physical_start.value % k_transition_page_size) != 0 ||
                    !multiply_without_overflow(
                        extent.page_count.value,
                        k_transition_page_size,
                        extent_bytes) ||
                    !add_without_overflow(
                        extent.physical_start.value,
                        extent_bytes,
                        extent_end) ||
                    extent_end > k_direct_map_physical_limit ||
                    (index != 0 && extent.physical_start.value < previous_end) ||
                    ranges_overlap(
                        { extent.physical_start.value, extent_end }, arena) ||
                    total_pages > UINT64_MAX - extent.page_count.value ||
                    consumed_pages >
                        UINT64_MAX - extent.consumed_page_count.value)
                    return false;

                total_pages += extent.page_count.value;
                consumed_pages += extent.consumed_page_count.value;
                previous_end = extent_end;
            }

            return total_pages == state.total_page_count.value &&
                consumed_pages == state.consumed_page_count.value;
        }

        [[nodiscard]] bool allocation_is_valid(
            const physical_memory_state_t& state,
            const boot_allocation_t& allocation) noexcept
        {
            if (allocation.physical_start.value == 0 ||
                allocation.page_count.value == 0 ||
                allocation.alignment_page_count.value == 0 ||
                allocation.consumed_page_count.value <
                    allocation.page_count.value ||
                (allocation.alignment_page_count.value &
                 (allocation.alignment_page_count.value - 1)) != 0 ||
                state.allocated_page_count.value < allocation.page_count.value)
                return false;

            uint64_t alignment_bytes{ 0 };
            uint64_t allocation_bytes{ 0 };
            uint64_t allocation_end{ 0 };
            if (!multiply_without_overflow(
                    allocation.alignment_page_count.value,
                    k_transition_page_size,
                    alignment_bytes) ||
                !multiply_without_overflow(
                    allocation.page_count.value,
                    k_transition_page_size,
                    allocation_bytes) ||
                (allocation.physical_start.value % alignment_bytes) != 0 ||
                !add_without_overflow(
                    allocation.physical_start.value,
                    allocation_bytes,
                    allocation_end))
                return false;

            for (uint32_t index{ 0 }; index < state.extent_count; ++index)
            {
                const physical_memory_extent_t& extent{ state.extents[index] };
                const uint64_t extent_end{
                    extent.physical_start.value +
                    extent.page_count.value * k_transition_page_size
                };
                if (allocation.physical_start.value >=
                        extent.physical_start.value &&
                    allocation_end <= extent_end)
                    return true;
            }
            return false;
        }

        void append_character(
            report_builder_t& builder,
            char character) noexcept
        {
            if (!builder.valid || builder.byte_count >= k_report_capacity)
            {
                builder.valid = false;
                return;
            }
            builder.bytes[builder.byte_count++] = character;
        }

        void append_text(report_builder_t& builder, const char* text) noexcept
        {
            while (*text != '\0') append_character(builder, *text++);
        }

        void append_decimal(report_builder_t& builder, uint64_t value) noexcept
        {
            char digits[20];
            uint32_t count{ 0 };
            do
            {
                digits[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0);
            while (count != 0) append_character(builder, digits[--count]);
        }

        void append_hexadecimal(report_builder_t& builder, uint64_t value) noexcept
        {
            constexpr char digits[]{ "0123456789ABCDEF" };
            append_text(builder, "0x");
            for (uint32_t shift{ 64 }; shift != 0; shift -= 4)
                append_character(
                    builder,
                    digits[(value >> (shift - 4)) & 0xf]);
        }
    } // anonymous namespace

    physical_memory_error_t initialize_physical_memory(
        const validated_boot_information_t& view,
        physical_address_t transition_arena_physical_start,
        page_count_t transition_arena_page_count,
        physical_memory_state_t& state) noexcept
    {
        clear_state(state);
        if (view.object == nullptr || view.memory_entries == nullptr ||
            view.memory_entry_count == 0 || view.reserved != 0 ||
            view.byte_count.value != view.object->total_size ||
            view.physical_address.value != view.object->self_physical_address)
            return physical_memory_error_t::invalid_validated_view;

        uint64_t arena_bytes{ 0 };
        uint64_t arena_end{ 0 };
        if (transition_arena_physical_start.value == 0 ||
            transition_arena_page_count.value == 0 ||
            (transition_arena_physical_start.value %
             k_transition_page_size) != 0 ||
            !multiply_without_overflow(
                transition_arena_page_count.value,
                k_transition_page_size,
                arena_bytes) ||
            !add_without_overflow(
                transition_arena_physical_start.value,
                arena_bytes,
                arena_end) ||
            arena_end > k_direct_map_physical_limit)
            return physical_memory_error_t::invalid_transition_arena;

        physical_memory_state_t candidate;
        clear_state(candidate);
        candidate.transition_arena_physical_start =
            transition_arena_physical_start;
        candidate.transition_arena_page_count = transition_arena_page_count;
        const range_t arena{
            transition_arena_physical_start.value,
            arena_end,
        };
        uint64_t arena_covered_pages{ 0 };
        uint64_t previous_end{ 0 };

        for (uint32_t index{ 0 }; index < view.memory_entry_count; ++index)
        {
            const warren_boot_memory_entry_t& entry{ view.memory_entries[index] };
            uint64_t entry_bytes{ 0 };
            uint64_t entry_end{ 0 };
            if (entry.page_count == 0 ||
                (entry.physical_start % k_transition_page_size) != 0 ||
                entry.reserved != 0 ||
                !multiply_without_overflow(
                    entry.page_count,
                    k_transition_page_size,
                    entry_bytes) ||
                !add_without_overflow(
                    entry.physical_start,
                    entry_bytes,
                    entry_end) ||
                (index != 0 && entry.physical_start < previous_end))
                return physical_memory_error_t::invalid_memory_map;
            previous_end = entry_end;

            if (entry.memory_kind != WARREN_BOOT_MEMORY_USABLE) continue;

            uint64_t managed_begin{ entry.physical_start };
            uint64_t managed_end{ entry_end };
            if (managed_begin == 0)
            {
                managed_begin = k_transition_page_size;
                candidate.excluded_page_count.value++;
            }
            if (managed_end > k_direct_map_physical_limit)
            {
                const uint64_t high_begin{
                    managed_begin < k_direct_map_physical_limit ?
                        k_direct_map_physical_limit : managed_begin
                };
                const uint64_t high_pages{
                    (managed_end - high_begin) / k_transition_page_size
                };
                if (candidate.unmanaged_page_count.value >
                    UINT64_MAX - high_pages)
                    return physical_memory_error_t::invalid_memory_map;
                candidate.unmanaged_page_count.value += high_pages;
                if (managed_begin >= k_direct_map_physical_limit) continue;
                managed_end = k_direct_map_physical_limit;
            }
            if (managed_begin >= managed_end) continue;

            const range_t managed{ managed_begin, managed_end };
            if (!ranges_overlap(managed, arena))
            {
                if (!append_extent(
                        candidate,
                        managed.begin,
                        managed.end,
                        entry))
                    return physical_memory_error_t::extent_capacity_exceeded;
                continue;
            }

            const uint64_t overlap_begin{
                managed.begin > arena.begin ? managed.begin : arena.begin
            };
            const uint64_t overlap_end{
                managed.end < arena.end ? managed.end : arena.end
            };
            const uint64_t overlap_pages{
                (overlap_end - overlap_begin) / k_transition_page_size
            };
            if (arena_covered_pages > UINT64_MAX - overlap_pages ||
                candidate.excluded_page_count.value >
                    UINT64_MAX - overlap_pages)
                return physical_memory_error_t::invalid_memory_map;
            arena_covered_pages += overlap_pages;
            candidate.excluded_page_count.value += overlap_pages;

            if (!append_extent(
                    candidate,
                    managed.begin,
                    overlap_begin,
                    entry) ||
                !append_extent(
                    candidate,
                    overlap_end,
                    managed.end,
                    entry))
                return physical_memory_error_t::extent_capacity_exceeded;
        }

        if (arena_covered_pages != transition_arena_page_count.value)
            return physical_memory_error_t::invalid_transition_arena;
        if (candidate.extent_count == 0 ||
            candidate.total_page_count.value == 0)
            return physical_memory_error_t::no_usable_memory;

        candidate.magic = k_physical_memory_state_magic;
        copy_state(state, candidate);
        return physical_memory_error_t::success;
    }

    physical_memory_error_t allocate_boot_pages(
        physical_memory_state_t& state,
        page_count_t page_count,
        page_count_t alignment_page_count,
        boot_allocation_t& allocation) noexcept
    {
        clear_allocation(allocation);
        if (!state_is_valid(state))
            return physical_memory_error_t::invalid_state;
        if (page_count.value == 0 || alignment_page_count.value == 0 ||
            (alignment_page_count.value &
             (alignment_page_count.value - 1)) != 0)
            return physical_memory_error_t::invalid_request;

        const uint64_t alignment_mask{ alignment_page_count.value - 1 };
        for (uint32_t index{ 0 }; index < state.extent_count; ++index)
        {
            physical_memory_extent_t& extent{ state.extents[index] };
            const uint64_t start_page{
                extent.physical_start.value / k_transition_page_size
            };
            const uint64_t current_page{
                start_page + extent.consumed_page_count.value
            };
            if (current_page > UINT64_MAX - alignment_mask)
                return physical_memory_error_t::invalid_state;
            const uint64_t aligned_page{
                (current_page + alignment_mask) & ~alignment_mask
            };
            const uint64_t padding{ aligned_page - current_page };
            if (padding > extent.page_count.value -
                    extent.consumed_page_count.value ||
                page_count.value > extent.page_count.value -
                    extent.consumed_page_count.value - padding)
                continue;

            const uint64_t consumed{ padding + page_count.value };
            if (state.consumed_page_count.value >
                    UINT64_MAX - consumed ||
                state.allocated_page_count.value >
                    UINT64_MAX - page_count.value)
                return physical_memory_error_t::invalid_state;

            extent.consumed_page_count.value += consumed;
            state.consumed_page_count.value += consumed;
            state.allocated_page_count.value += page_count.value;
            allocation.physical_start = {
                aligned_page * k_transition_page_size
            };
            allocation.page_count = page_count;
            allocation.alignment_page_count = alignment_page_count;
            allocation.consumed_page_count = { consumed };
            return physical_memory_error_t::success;
        }
        return physical_memory_error_t::exhausted;
    }

    physical_memory_error_t report_physical_memory(
        const physical_memory_state_t& state,
        const boot_allocation_t& allocation,
        const drivers::console_writer_t& console) noexcept
    {
        if (!state_is_valid(state) ||
            !allocation_is_valid(state, allocation))
            return physical_memory_error_t::invalid_state;

        report_builder_t report;
        report.byte_count = 0;
        report.valid = true;
        append_text(report, "BURROW_MEMORY_V1:extents=");
        append_decimal(report, state.extent_count);
        append_text(report, ":pages=");
        append_decimal(report, state.total_page_count.value);
        append_text(report, ":arena=");
        append_hexadecimal(
            report,
            state.transition_arena_physical_start.value);
        append_text(report, ":arena_pages=");
        append_decimal(report, state.transition_arena_page_count.value);
        append_text(report, ":boot_alloc=");
        append_hexadecimal(report, allocation.physical_start.value);
        append_text(report, ":boot_pages=");
        append_decimal(report, allocation.page_count.value);
        append_text(report, ":remaining=");
        append_decimal(
            report,
            state.total_page_count.value -
                state.consumed_page_count.value);
        append_text(report, "\r\n");

        if (!report.valid ||
            drivers::write_console(
                console,
                report.bytes,
                report.byte_count) !=
                drivers::console_write_error_t::success)
            return physical_memory_error_t::output_failure;
        return physical_memory_error_t::success;
    }
} // namespace burrow::core
