//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootHandoffStorage.h>

#include <warren/boot/BootInformation.h>
#include <warren/boot/BootInformationProducer.h>

#include <stddef.h>
#include <stdint.h>

namespace warren::boot {
    namespace {
        constexpr uint64_t k_page_size{ WARREN_BOOT_INFORMATION_PAGE_SIZE };
        constexpr uint64_t k_minimum_uefi_descriptor_size{ 40 };

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

        [[nodiscard]] bool bytes_to_pages(uint64_t byte_count, uint64_t& page_count,
                                          uint64_t& allocation_size) noexcept
        {
            uint64_t rounded_size{ 0 };

            if (byte_count == 0 || !add_without_overflow(byte_count, k_page_size - 1, rounded_size))
                return false;

            page_count = rounded_size / k_page_size;
            return multiply_without_overflow(page_count, k_page_size, allocation_size);
        }

        [[nodiscard]] bool allocation_is_empty(const boot_handoff_allocation_t& allocation) noexcept
        {
            return allocation.physical_start == 0 && allocation.page_count == 0 &&
                   allocation.writable_start == nullptr;
        }

        [[nodiscard]] bool storage_is_empty(const boot_handoff_storage_t& storage) noexcept
        {
            return allocation_is_empty(storage.bootstrap_stack) && allocation_is_empty(storage.memory_map) &&
                   allocation_is_empty(storage.source_descriptors) && allocation_is_empty(storage.work_entries) &&
                   allocation_is_empty(storage.object);
        }

        [[nodiscard]] bool allocation_is_valid(const boot_handoff_allocation_t& allocation) noexcept
        {
            if (allocation.physical_start == 0 || allocation.page_count == 0 ||
                allocation.writable_start == nullptr || (allocation.physical_start % k_page_size) != 0 ||
                (reinterpret_cast<uintptr_t>(allocation.writable_start) % k_page_size) != 0)
                return false;

            uint64_t byte_count{ 0 };
            uint64_t end{ 0 };
            return multiply_without_overflow(allocation.page_count, k_page_size, byte_count) &&
                   add_without_overflow(allocation.physical_start, byte_count, end);
        }

        [[nodiscard]] bool allocations_overlap(const boot_handoff_allocation_t& left,
                                               const boot_handoff_allocation_t& right) noexcept
        {
            uint64_t left_size{ 0 };
            uint64_t right_size{ 0 };
            uint64_t left_end{ 0 };
            uint64_t right_end{ 0 };
            static_cast<void>(multiply_without_overflow(left.page_count, k_page_size, left_size));
            static_cast<void>(multiply_without_overflow(right.page_count, k_page_size, right_size));
            static_cast<void>(add_without_overflow(left.physical_start, left_size, left_end));
            static_cast<void>(add_without_overflow(right.physical_start, right_size, right_end));
            return left.physical_start < right_end && right.physical_start < left_end;
        }

        void clear_allocation(boot_handoff_allocation_t& allocation) noexcept
        {
            uint64_t byte_count{ 0 };
            static_cast<void>(multiply_without_overflow(allocation.page_count, k_page_size, byte_count));

            for (uint64_t index{ 0 }; index < byte_count; ++index)
                allocation.writable_start[index] = 0;
        }

        [[nodiscard]] bool plan_is_valid(const boot_handoff_storage_plan_t& plan) noexcept
        {
            uint64_t expected_size{ 0 };
            uint64_t required_work_size{ 0 };
            uint64_t minimum_object_size{ WARREN_BOOT_INFORMATION_HEADER_SIZE };

            if (plan.memory_map_page_count == 0 || plan.source_page_count == 0 || plan.work_page_count == 0 ||
                plan.object_page_count == 0 ||
                plan.memory_descriptor_size < k_minimum_uefi_descriptor_size ||
                (plan.memory_descriptor_size & 7U) != 0 ||
                plan.source_descriptor_capacity == 0 || plan.work_entry_capacity == 0 ||
                plan.object_required_capacity < WARREN_BOOT_INFORMATION_HEADER_SIZE)
                return false;

            if (!multiply_without_overflow(plan.memory_map_page_count, k_page_size, expected_size) ||
                expected_size != plan.memory_map_capacity ||
                plan.source_descriptor_capacity != plan.memory_map_capacity / plan.memory_descriptor_size ||
                !multiply_without_overflow(plan.source_page_count, k_page_size, expected_size) ||
                expected_size != plan.source_capacity ||
                !multiply_without_overflow(plan.work_page_count, k_page_size, expected_size) ||
                expected_size != plan.work_capacity ||
                !multiply_without_overflow(plan.object_page_count, k_page_size, expected_size) ||
                expected_size != plan.object_allocation_size ||
                plan.object_required_capacity > plan.object_allocation_size ||
                !multiply_without_overflow(plan.work_entry_capacity, sizeof(warren_boot_memory_entry_t),
                                           required_work_size) ||
                required_work_size > plan.work_capacity ||
                !multiply_without_overflow(plan.source_descriptor_capacity,
                                           sizeof(boot_information_source_descriptor_t), expected_size) ||
                expected_size > plan.source_capacity ||
                !add_without_overflow(minimum_object_size, required_work_size, minimum_object_size) ||
                (plan.object_required_capacity != minimum_object_size &&
                 plan.object_required_capacity != minimum_object_size + sizeof(warren_boot_early_console_t)))
                return false;

            return true;
        }

        [[nodiscard]] bool storage_is_valid(const boot_handoff_storage_t& storage) noexcept
        {
            const boot_handoff_allocation_t* allocations[]{
                &storage.bootstrap_stack,
                &storage.memory_map,
                &storage.source_descriptors,
                &storage.work_entries,
                &storage.object,
            };

            if (!plan_is_valid(storage.plan) ||
                storage.bootstrap_stack.page_count != k_bootstrap_stack_page_count ||
                storage.memory_map.page_count != storage.plan.memory_map_page_count ||
                storage.source_descriptors.page_count != storage.plan.source_page_count ||
                storage.work_entries.page_count != storage.plan.work_page_count ||
                storage.object.page_count != storage.plan.object_page_count)
                return false;

            for (uint32_t index{ 0 }; index < 5; ++index)
            {
                if (!allocation_is_valid(*allocations[index])) return false;

                for (uint32_t previous{ 0 }; previous < index; ++previous)
                {
                    if (allocations_overlap(*allocations[previous], *allocations[index])) return false;
                }
            }

            return true;
        }

        [[nodiscard]] boot_handoff_storage_error_t allocate_one(
            const boot_handoff_page_allocator_t& allocator,
            uint64_t page_count,
            boot_handoff_allocation_t& allocation,
            uint64_t& platform_status) noexcept
        {
            boot_handoff_page_allocation_t page_allocation{ };
            platform_status = allocator.allocate_pages(allocator.context, page_count, page_allocation);

            if (platform_status != 0)
                return boot_handoff_storage_error_t::allocation_failed;

            allocation = { page_allocation.physical_start, page_count, page_allocation.writable_start };

            if (!allocation_is_valid(allocation))
                return boot_handoff_storage_error_t::invalid_allocation;

            clear_allocation(allocation);
            return boot_handoff_storage_error_t::success;
        }

        [[nodiscard]] boot_handoff_storage_error_t free_one(
            const boot_handoff_page_allocator_t& allocator,
            boot_handoff_allocation_t& allocation,
            uint64_t& platform_status) noexcept
        {
            if (allocation_is_empty(allocation))
                return boot_handoff_storage_error_t::success;

            const uint64_t status{
                allocator.free_pages(allocator.context, allocation.physical_start, allocation.page_count)
            };

            if (status == 0)
            {
                allocation = { };
                return boot_handoff_storage_error_t::success;
            }

            if (platform_status == 0) platform_status = status;
            return boot_handoff_storage_error_t::cleanup_failed;
        }

        [[nodiscard]] boot_handoff_storage_error_t release_resizable_allocations(
            const boot_handoff_page_allocator_t& allocator,
            boot_handoff_storage_t& storage,
            uint64_t& platform_status) noexcept
        {
            boot_handoff_storage_error_t result{ boot_handoff_storage_error_t::success };
            boot_handoff_allocation_t* allocations[]{
                &storage.object,
                &storage.work_entries,
                &storage.source_descriptors,
                &storage.memory_map,
            };

            for (boot_handoff_allocation_t* allocation: allocations)
            {
                if (free_one(allocator, *allocation, platform_status) != boot_handoff_storage_error_t::success)
                    result = boot_handoff_storage_error_t::cleanup_failed;
            }

            return result;
        }
    } // anonymous namespace

    boot_handoff_storage_error_t plan_boot_handoff_storage(
        uint64_t estimated_memory_map_size,
        uint64_t memory_descriptor_size,
        bool early_console_present,
        boot_handoff_storage_plan_t& plan) noexcept
    {
        plan = { };

        if (memory_descriptor_size < k_minimum_uefi_descriptor_size || (memory_descriptor_size & 7U) != 0)
            return boot_handoff_storage_error_t::invalid_descriptor_size;

        if (estimated_memory_map_size == 0 || estimated_memory_map_size % memory_descriptor_size != 0)
            return boot_handoff_storage_error_t::invalid_memory_map_size;

        uint64_t growth_size{ 0 };
        uint64_t requested_map_capacity{ 0 };

        if (!multiply_without_overflow(k_memory_map_growth_descriptor_count, memory_descriptor_size, growth_size) ||
            !add_without_overflow(estimated_memory_map_size, growth_size, requested_map_capacity))
            return boot_handoff_storage_error_t::arithmetic_overflow;

        if (!bytes_to_pages(requested_map_capacity, plan.memory_map_page_count, plan.memory_map_capacity))
            return boot_handoff_storage_error_t::arithmetic_overflow;

        const uint64_t descriptor_capacity{ plan.memory_map_capacity / memory_descriptor_size };
        uint64_t work_entry_capacity{ 0 };

        if (!add_without_overflow(descriptor_capacity, k_handoff_live_resource_count * 2, work_entry_capacity) ||
            descriptor_capacity > UINT32_MAX || work_entry_capacity > UINT32_MAX)
            return boot_handoff_storage_error_t::capacity_overflow;

        plan.source_descriptor_capacity = static_cast<uint32_t>(descriptor_capacity);
        plan.memory_descriptor_size = memory_descriptor_size;
        plan.work_entry_capacity = static_cast<uint32_t>(work_entry_capacity);

        uint64_t source_byte_count{ 0 };

        if (!multiply_without_overflow(descriptor_capacity, sizeof(boot_information_source_descriptor_t),
                                       source_byte_count) ||
            !bytes_to_pages(source_byte_count, plan.source_page_count, plan.source_capacity))
            return boot_handoff_storage_error_t::arithmetic_overflow;

        uint64_t work_byte_count{ 0 };

        if (!multiply_without_overflow(work_entry_capacity, sizeof(warren_boot_memory_entry_t), work_byte_count) ||
            !bytes_to_pages(work_byte_count, plan.work_page_count, plan.work_capacity))
            return boot_handoff_storage_error_t::arithmetic_overflow;

        uint64_t object_size{ WARREN_BOOT_INFORMATION_HEADER_SIZE };
        uint64_t object_section_size{ 0 };

        if (!multiply_without_overflow(work_entry_capacity, sizeof(warren_boot_memory_entry_t),
                                       object_section_size) ||
            !add_without_overflow(object_size, object_section_size, object_size) ||
            (early_console_present &&
             !add_without_overflow(object_size, sizeof(warren_boot_early_console_t), object_size)) ||
            object_size > UINT32_MAX)
            return boot_handoff_storage_error_t::capacity_overflow;

        plan.object_required_capacity = static_cast<uint32_t>(object_size);

        if (!bytes_to_pages(object_size, plan.object_page_count, plan.object_allocation_size))
            return boot_handoff_storage_error_t::arithmetic_overflow;

        return boot_handoff_storage_error_t::success;
    }

    boot_handoff_storage_error_t allocate_boot_handoff_storage(
        const boot_handoff_storage_plan_t& plan,
        const boot_handoff_page_allocator_t& allocator,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept
    {
        platform_status = 0;

        if (!storage_is_empty(storage))
            return boot_handoff_storage_error_t::storage_not_empty;

        storage = { };

        if (!plan_is_valid(plan))
            return boot_handoff_storage_error_t::invalid_plan;

        if (allocator.allocate_pages == nullptr || allocator.free_pages == nullptr)
            return boot_handoff_storage_error_t::invalid_allocator;

        storage.plan = plan;
        const uint64_t page_counts[]{
            k_bootstrap_stack_page_count,
            plan.memory_map_page_count,
            plan.source_page_count,
            plan.work_page_count,
            plan.object_page_count,
        };
        boot_handoff_allocation_t* allocations[]{
            &storage.bootstrap_stack,
            &storage.memory_map,
            &storage.source_descriptors,
            &storage.work_entries,
            &storage.object,
        };

        for (uint32_t index{ 0 }; index < 5; ++index)
        {
            const boot_handoff_storage_error_t allocation_result{
                allocate_one(allocator, page_counts[index], *allocations[index], platform_status)
            };

            if (allocation_result != boot_handoff_storage_error_t::success)
            {
                uint64_t cleanup_status{ 0 };
                const boot_handoff_storage_error_t cleanup_result{
                    release_boot_handoff_storage(allocator, storage, cleanup_status)
                };

                if (cleanup_result != boot_handoff_storage_error_t::success)
                {
                    platform_status = cleanup_status;
                    return cleanup_result;
                }

                return allocation_result;
            }

            for (uint32_t previous{ 0 }; previous < index; ++previous)
            {
                if (allocations_overlap(*allocations[previous], *allocations[index]))
                {
                    uint64_t cleanup_status{ 0 };
                    const boot_handoff_storage_error_t cleanup_result{
                        release_boot_handoff_storage(allocator, storage, cleanup_status)
                    };

                    if (cleanup_result != boot_handoff_storage_error_t::success)
                    {
                        platform_status = cleanup_status;
                        return cleanup_result;
                    }

                    return boot_handoff_storage_error_t::overlapping_allocations;
                }
            }
        }

        return boot_handoff_storage_error_t::success;
    }

    bool boot_handoff_storage_is_valid(const boot_handoff_storage_t& storage) noexcept
    {
        return storage_is_valid(storage);
    }

    boot_handoff_storage_error_t release_boot_handoff_storage(
        const boot_handoff_page_allocator_t& allocator,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept
    {
        platform_status = 0;

        if (allocator.free_pages == nullptr)
            return boot_handoff_storage_error_t::invalid_allocator;

        boot_handoff_storage_error_t result{ boot_handoff_storage_error_t::success };
        boot_handoff_allocation_t* allocations[]{
            &storage.object,
            &storage.work_entries,
            &storage.source_descriptors,
            &storage.memory_map,
            &storage.bootstrap_stack,
        };

        for (boot_handoff_allocation_t* allocation: allocations)
        {
            if (free_one(allocator, *allocation, platform_status) != boot_handoff_storage_error_t::success)
                result = boot_handoff_storage_error_t::cleanup_failed;
        }

        if (result == boot_handoff_storage_error_t::success)
            storage = { };

        return result;
    }

    boot_handoff_storage_error_t resize_boot_handoff_storage(
        const boot_handoff_storage_plan_t& plan,
        const boot_handoff_page_allocator_t& allocator,
        bool retain_superseded_storage,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept
    {
        platform_status = 0;

        if (!plan_is_valid(plan))
            return boot_handoff_storage_error_t::invalid_plan;

        if (allocator.allocate_pages == nullptr || allocator.free_pages == nullptr)
            return boot_handoff_storage_error_t::invalid_allocator;

        if (!storage_is_valid(storage))
            return boot_handoff_storage_error_t::invalid_allocation;

        boot_handoff_storage_t replacement{ };
        replacement.plan = plan;
        replacement.bootstrap_stack = storage.bootstrap_stack;
        const uint64_t page_counts[]{
            plan.memory_map_page_count,
            plan.source_page_count,
            plan.work_page_count,
            plan.object_page_count,
        };
        boot_handoff_allocation_t* allocations[]{
            &replacement.memory_map,
            &replacement.source_descriptors,
            &replacement.work_entries,
            &replacement.object,
        };
        const boot_handoff_allocation_t* superseded_allocations[]{
            &storage.bootstrap_stack,
            &storage.memory_map,
            &storage.source_descriptors,
            &storage.work_entries,
            &storage.object,
        };

        for (uint32_t index{ 0 }; index < 4; ++index)
        {
            const boot_handoff_storage_error_t allocation_result{
                allocate_one(allocator, page_counts[index], *allocations[index], platform_status)
            };

            if (allocation_result != boot_handoff_storage_error_t::success)
            {
                uint64_t cleanup_status{ 0 };
                if (release_resizable_allocations(allocator, replacement, cleanup_status) !=
                    boot_handoff_storage_error_t::success)
                {
                    platform_status = cleanup_status;
                    return boot_handoff_storage_error_t::cleanup_failed;
                }

                return allocation_result;
            }

            bool overlaps_existing{ false };
            for (const boot_handoff_allocation_t* superseded: superseded_allocations)
                overlaps_existing = overlaps_existing || allocations_overlap(*superseded, *allocations[index]);

            if (overlaps_existing)
            {
                uint64_t cleanup_status{ 0 };
                static_cast<void>(release_resizable_allocations(allocator, replacement, cleanup_status));
                return boot_handoff_storage_error_t::overlapping_allocations;
            }

            for (uint32_t previous{ 0 }; previous < index; ++previous)
            {
                if (!allocations_overlap(*allocations[previous], *allocations[index])) continue;

                uint64_t cleanup_status{ 0 };
                static_cast<void>(release_resizable_allocations(allocator, replacement, cleanup_status));
                return boot_handoff_storage_error_t::overlapping_allocations;
            }
        }

        if (!retain_superseded_storage)
        {
            const boot_handoff_storage_error_t release_result{
                release_resizable_allocations(allocator, storage, platform_status)
            };

            if (release_result != boot_handoff_storage_error_t::success)
            {
                uint64_t cleanup_status{ 0 };
                static_cast<void>(release_resizable_allocations(allocator, replacement, cleanup_status));
                return release_result;
            }
        }

        storage = replacement;
        return boot_handoff_storage_error_t::success;
    }
} // namespace warren::boot
