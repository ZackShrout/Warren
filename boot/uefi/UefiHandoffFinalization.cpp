//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/UefiHandoffFinalization.h>
#include <warren/boot/UefiHandoffStorage.h>

#include <stddef.h>
#include <stdint.h>

static_assert(EFI_MEMORY_DESCRIPTOR_VERSION == warren::boot::k_uefi_memory_descriptor_version);
static_assert(sizeof(EFI_MEMORY_DESCRIPTOR) == 40);
static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, Type) == 0x00);
static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, PhysicalStart) == 0x08);
static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, NumberOfPages) == 0x18);
static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, Attribute) == 0x20);

namespace warren::boot::uefi {
    namespace {
        struct finalization_context_t
        {
            EFI_HANDLE image_handle;
            EFI_SYSTEM_TABLE* system_table;
            bool early_console_present;
        };

        boot_handoff_firmware_result_t get_memory_map(
            void* opaque_context,
            uint8_t* memory_map,
            uint64_t memory_map_capacity,
            boot_handoff_memory_map_snapshot_t& snapshot,
            uint64_t& platform_status) noexcept
        {
            auto& context{ *static_cast<finalization_context_t*>(opaque_context) };
            constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

            if (context.system_table == nullptr || context.system_table->BootServices == nullptr ||
                context.system_table->BootServices->GetMemoryMap == nullptr ||
                memory_map == nullptr || memory_map_capacity > maximum_uintn)
            {
                platform_status = EFI_INVALID_PARAMETER;
                return boot_handoff_firmware_result_t::failure;
            }

            UINTN map_size{ static_cast<UINTN>(memory_map_capacity) };
            UINTN map_key{ 0 };
            UINTN descriptor_size{ 0 };
            UINT32 descriptor_version{ 0 };
            const EFI_STATUS status{
                context.system_table->BootServices->GetMemoryMap(
                    &map_size,
                    reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(memory_map),
                    &map_key,
                    &descriptor_size,
                    &descriptor_version)
            };
            platform_status = status;
            snapshot = { map_size, map_key, descriptor_size, descriptor_version };

            if (status == EFI_BUFFER_TOO_SMALL)
                return boot_handoff_firmware_result_t::buffer_too_small;

            return EFI_ERROR(status)
                       ? boot_handoff_firmware_result_t::failure
                       : boot_handoff_firmware_result_t::success;
        }

        boot_handoff_firmware_result_t exit_boot_services(void* opaque_context, uint64_t map_key,
                                                          uint64_t& platform_status) noexcept
        {
            auto& context{ *static_cast<finalization_context_t*>(opaque_context) };
            constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

            if (context.system_table == nullptr || context.system_table->BootServices == nullptr ||
                context.system_table->BootServices->ExitBootServices == nullptr ||
                map_key > maximum_uintn)
            {
                platform_status = EFI_INVALID_PARAMETER;
                return boot_handoff_firmware_result_t::failure;
            }

            const EFI_STATUS status{
                context.system_table->BootServices->ExitBootServices(
                    context.image_handle, static_cast<UINTN>(map_key))
            };
            platform_status = status;

            if (status == EFI_INVALID_PARAMETER)
                return boot_handoff_firmware_result_t::stale_map_key;

            return EFI_ERROR(status)
                       ? boot_handoff_firmware_result_t::failure
                       : boot_handoff_firmware_result_t::success;
        }

        boot_handoff_firmware_result_t resize_storage(
            void* opaque_context,
            uint64_t required_memory_map_size,
            uint64_t memory_descriptor_size,
            bool exit_attempted,
            boot_handoff_storage_t& storage,
            uint64_t& platform_status) noexcept
        {
            auto& context{ *static_cast<finalization_context_t*>(opaque_context) };

            if (context.system_table == nullptr)
            {
                platform_status = EFI_INVALID_PARAMETER;
                return boot_handoff_firmware_result_t::failure;
            }

            boot_handoff_storage_plan_t replacement_plan{ };
            const boot_handoff_storage_error_t plan_result{
                plan_boot_handoff_storage(
                    required_memory_map_size,
                    memory_descriptor_size,
                    context.early_console_present,
                    replacement_plan)
            };

            if (plan_result != boot_handoff_storage_error_t::success)
            {
                platform_status = EFI_INVALID_PARAMETER;
                return boot_handoff_firmware_result_t::failure;
            }

            EFI_STATUS status{ EFI_SUCCESS };
            const boot_handoff_storage_error_t resize_result{
                resize_handoff_storage(
                    *context.system_table,
                    replacement_plan,
                    exit_attempted,
                    storage,
                    status)
            };

            if (resize_result != boot_handoff_storage_error_t::success)
            {
                platform_status = status;
                return boot_handoff_firmware_result_t::failure;
            }
            platform_status = EFI_SUCCESS;
            return boot_handoff_firmware_result_t::success;
        }
    } // anonymous namespace

    boot_handoff_finalization_error_t finalize_handoff(
        EFI_HANDLE image_handle,
        EFI_SYSTEM_TABLE& system_table,
        const burrow_loaded_image_t& loaded_image,
        const warren_boot_early_console_t& early_console,
        boot_handoff_storage_t& storage,
        boot_handoff_finalization_result_t& result) noexcept
    {
        finalization_context_t context{
            image_handle,
            &system_table,
            early_console.kind != 0,
        };
        const boot_handoff_firmware_operations_t firmware{
            &context,
            get_memory_map,
            exit_boot_services,
            resize_storage,
        };
        return finalize_boot_handoff(loaded_image, early_console, storage, firmware, result);
    }
} // namespace warren::boot::uefi
