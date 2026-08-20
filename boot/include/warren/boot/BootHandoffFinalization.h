//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootHandoffStorage.h>
#include <warren/boot/BurrowLoader.h>

#include <warren/boot/BootInformationProducer.h>
#include <warren/boot/BootInformationValidation.h>

#include <stdint.h>

namespace warren::boot {
    constexpr uint32_t k_boot_services_exit_attempt_limit{ 8 };
    constexpr uint32_t k_handoff_storage_resize_attempt_limit{ 8 };
    constexpr uint32_t k_uefi_memory_descriptor_version{ 1 };

    enum class boot_handoff_firmware_result_t : uint32_t
    {
        success = 0,
        buffer_too_small = 1,
        stale_map_key = 2,
        failure = 3,
    };

    struct boot_handoff_memory_map_snapshot_t
    {
        uint64_t memory_map_size;
        uint64_t map_key;
        uint64_t descriptor_size;
        uint32_t descriptor_version;
    };

    using boot_handoff_get_memory_map_fn = boot_handoff_firmware_result_t (*)(
        void* context,
        uint8_t* memory_map,
        uint64_t memory_map_capacity,
        boot_handoff_memory_map_snapshot_t& snapshot,
        uint64_t& platform_status) noexcept;

    using boot_handoff_exit_boot_services_fn = boot_handoff_firmware_result_t (*)(
        void* context,
        uint64_t map_key,
        uint64_t& platform_status) noexcept;

    using boot_handoff_resize_storage_fn = boot_handoff_firmware_result_t (*)(
        void* context,
        uint64_t required_memory_map_size,
        uint64_t memory_descriptor_size,
        bool exit_attempted,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept;

    struct boot_handoff_firmware_operations_t
    {
        void* context;
        boot_handoff_get_memory_map_fn get_memory_map;
        boot_handoff_exit_boot_services_fn exit_boot_services;
        boot_handoff_resize_storage_fn resize_storage;
    };

    enum class boot_handoff_finalization_error_t : uint32_t
    {
        success = 0,
        invalid_input = 1,
        memory_map_failed = 2,
        invalid_memory_map_snapshot = 3,
        source_capacity_exceeded = 4,
        invalid_memory_descriptor = 5,
        producer_failed = 6,
        validation_failed = 7,
        exit_failed = 8,
        exit_retry_exhausted = 9,
        storage_resize_failed = 10,
        storage_resize_retry_exhausted = 11,
    };

    struct boot_handoff_finalization_result_t
    {
        boot_handoff_finalization_error_t error;
        boot_information_producer_error_t producer_error;
        boot_information_error_t validation_error;
        uint64_t platform_status;
        uint64_t required_memory_map_size;
        uint32_t memory_map_capture_count;
        uint32_t exit_attempt_count;
        uint32_t storage_resize_count;
        uint32_t source_descriptor_count;
        uint32_t object_size;
        bool exit_attempted;
    };

    [[nodiscard]] boot_handoff_finalization_error_t finalize_boot_handoff(
        const burrow_loaded_image_t& loaded_image,
        const warren_boot_early_console_t& early_console,
        boot_handoff_storage_t& storage,
        const boot_handoff_firmware_operations_t& firmware,
        boot_handoff_finalization_result_t& result) noexcept;
} // namespace warren::boot
