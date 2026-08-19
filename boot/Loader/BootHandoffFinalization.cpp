//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootHandoffFinalization.h>

#include <stddef.h>
#include <stdint.h>

namespace warren::boot {
    namespace {
        constexpr uint64_t k_minimum_uefi_descriptor_size{ 40 };

        [[nodiscard]] uint32_t read_u32(const uint8_t* bytes) noexcept
        {
            uint32_t value{ 0 };

            for (uint32_t index{ 0 }; index < 4; ++index)
                value |= static_cast<uint32_t>(bytes[index]) << (index * 8U);

            return value;
        }

        [[nodiscard]] uint64_t read_u64(const uint8_t* bytes) noexcept
        {
            uint64_t value{ 0 };

            for (uint32_t index{ 0 }; index < 8; ++index)
                value |= static_cast<uint64_t>(bytes[index]) << (index * 8U);

            return value;
        }

        [[nodiscard]] boot_handoff_finalization_error_t decode_memory_map(
            const boot_handoff_memory_map_snapshot_t& snapshot,
            const boot_handoff_storage_t& storage,
            uint32_t& descriptor_count) noexcept
        {
            descriptor_count = 0;

            if (snapshot.memory_map_size == 0 || snapshot.memory_map_size > storage.plan.memory_map_capacity ||
                snapshot.descriptor_size != storage.plan.memory_descriptor_size ||
                snapshot.descriptor_size < k_minimum_uefi_descriptor_size ||
                (snapshot.descriptor_size & 7U) != 0 ||
                snapshot.memory_map_size % snapshot.descriptor_size != 0 ||
                snapshot.descriptor_version != k_uefi_memory_descriptor_version)
                return boot_handoff_finalization_error_t::invalid_memory_map_snapshot;

            const uint64_t count{ snapshot.memory_map_size / snapshot.descriptor_size };

            if (count == 0 || count > storage.plan.source_descriptor_capacity || count > UINT32_MAX)
                return boot_handoff_finalization_error_t::source_capacity_exceeded;

            auto* descriptors{ reinterpret_cast<boot_information_source_descriptor_t*>(
                storage.source_descriptors.writable_start) };

            for (uint64_t index{ 0 }; index < count; ++index)
            {
                const uint8_t* raw{ storage.memory_map.writable_start + index * snapshot.descriptor_size };
                boot_information_source_descriptor_t& descriptor{ descriptors[index] };
                descriptor.source_type = read_u32(raw + 0x00);
                descriptor.physical_start = read_u64(raw + 0x08);
                descriptor.page_count = read_u64(raw + 0x18);
                descriptor.source_attributes = read_u64(raw + 0x20);

                if (descriptor.page_count == 0 ||
                    (descriptor.physical_start % WARREN_BOOT_INFORMATION_PAGE_SIZE) != 0)
                    return boot_handoff_finalization_error_t::invalid_memory_descriptor;
            }

            descriptor_count = static_cast<uint32_t>(count);
            return boot_handoff_finalization_error_t::success;
        }

        void set_error(boot_handoff_finalization_result_t& result,
                       boot_handoff_finalization_error_t error) noexcept
        {
            result.error = error;
        }

        void invalidate_object(boot_handoff_storage_t& storage,
                               boot_handoff_finalization_result_t& result) noexcept
        {
            for (uint32_t index{ 0 }; index < WARREN_BOOT_INFORMATION_MAGIC_SIZE; ++index)
                storage.object.writable_start[index] = 0;

            result.object_size = 0;
            result.source_descriptor_count = 0;
        }
    } // anonymous namespace

    boot_handoff_finalization_error_t finalize_boot_handoff(
        const burrow_loaded_image_t& loaded_image,
        const warren_boot_early_console_t& early_console,
        boot_handoff_storage_t& storage,
        const boot_handoff_firmware_operations_t& firmware,
        boot_handoff_finalization_result_t& result) noexcept
    {
        result = {
            boot_handoff_finalization_error_t::invalid_input,
            boot_information_producer_error_t::success,
            boot_information_error_t::success,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            false,
        };

        if (!boot_handoff_storage_is_valid(storage) || firmware.get_memory_map == nullptr ||
            firmware.exit_boot_services == nullptr || firmware.resize_storage == nullptr)
            return result.error;

        while (result.exit_attempt_count < k_boot_services_exit_attempt_limit)
        {
            invalidate_object(storage, result);
            boot_handoff_memory_map_snapshot_t snapshot{ };
            uint64_t platform_status{ 0 };
            const boot_handoff_firmware_result_t map_result{
                firmware.get_memory_map(
                    firmware.context,
                    storage.memory_map.writable_start,
                    storage.plan.memory_map_capacity,
                    snapshot,
                    platform_status)
            };
            ++result.memory_map_capture_count;
            result.platform_status = platform_status;

            if (map_result == boot_handoff_firmware_result_t::buffer_too_small)
            {
                result.required_memory_map_size = snapshot.memory_map_size;

                if (result.storage_resize_count == k_handoff_storage_resize_attempt_limit)
                {
                    set_error(result, boot_handoff_finalization_error_t::storage_resize_retry_exhausted);
                    return result.error;
                }

                ++result.storage_resize_count;
                platform_status = 0;
                const boot_handoff_firmware_result_t resize_result{
                    firmware.resize_storage(
                        firmware.context,
                        snapshot.memory_map_size,
                        snapshot.descriptor_size,
                        result.exit_attempted,
                        storage,
                        platform_status)
                };
                result.platform_status = platform_status;

                if (resize_result != boot_handoff_firmware_result_t::success)
                {
                    set_error(result, boot_handoff_finalization_error_t::storage_resize_failed);
                    return result.error;
                }

                if (!boot_handoff_storage_is_valid(storage))
                {
                    set_error(result, boot_handoff_finalization_error_t::storage_resize_failed);
                    return result.error;
                }

                continue;
            }

            if (map_result != boot_handoff_firmware_result_t::success)
            {
                set_error(result, boot_handoff_finalization_error_t::memory_map_failed);
                return result.error;
            }

            uint32_t source_descriptor_count{ 0 };
            const boot_handoff_finalization_error_t decode_result{
                decode_memory_map(snapshot, storage, source_descriptor_count)
            };

            if (decode_result != boot_handoff_finalization_error_t::success)
            {
                set_error(result, decode_result);
                return result.error;
            }

            result.source_descriptor_count = source_descriptor_count;
            const boot_information_producer_input_t producer_input{
                reinterpret_cast<const boot_information_source_descriptor_t*>(
                    storage.source_descriptors.writable_start),
                source_descriptor_count,
                storage.object.physical_start,
                storage.plan.object_allocation_size,
                loaded_image.physical_start,
                loaded_image.physical_size,
                loaded_image.load_bias,
                loaded_image.entry_physical_address,
                storage.bootstrap_stack.physical_start,
                storage.bootstrap_stack.page_count * WARREN_BOOT_INFORMATION_PAGE_SIZE,
                &early_console,
            };
            boot_information_producer_output_t producer_output{ };
            result.producer_error = produce_boot_information(
                producer_input,
                storage.object.writable_start,
                storage.plan.object_required_capacity,
                reinterpret_cast<warren_boot_memory_entry_t*>(storage.work_entries.writable_start),
                storage.plan.work_entry_capacity,
                producer_output);

            if (result.producer_error != boot_information_producer_error_t::success)
            {
                set_error(result, boot_handoff_finalization_error_t::producer_failed);
                return result.error;
            }

            result.object_size = producer_output.total_size;
            result.validation_error = validate_boot_information(
                storage.object.writable_start,
                producer_output.total_size,
                storage.object.physical_start);

            if (result.validation_error != boot_information_error_t::success)
            {
                invalidate_object(storage, result);
                set_error(result, boot_handoff_finalization_error_t::validation_failed);
                return result.error;
            }

            result.exit_attempted = true;
            ++result.exit_attempt_count;
            platform_status = 0;
            const boot_handoff_firmware_result_t exit_result{
                firmware.exit_boot_services(firmware.context, snapshot.map_key, platform_status)
            };
            result.platform_status = platform_status;

            if (exit_result == boot_handoff_firmware_result_t::success)
            {
                set_error(result, boot_handoff_finalization_error_t::success);
                return result.error;
            }

            invalidate_object(storage, result);

            if (exit_result != boot_handoff_firmware_result_t::stale_map_key)
            {
                set_error(result, boot_handoff_finalization_error_t::exit_failed);
                return result.error;
            }
        }

        set_error(result, boot_handoff_finalization_error_t::exit_retry_exhausted);
        return result.error;
    }
} // namespace warren::boot
