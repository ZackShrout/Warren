//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/UefiHandoffStorage.h>

#include <stdint.h>

namespace warren::boot::uefi {
    namespace {
        struct allocator_context_t
        {
            EFI_BOOT_SERVICES* boot_services;
        };

        [[nodiscard]] uint64_t allocate_pages(void* opaque_context, uint64_t page_count,
                                              boot_handoff_page_allocation_t& allocation) noexcept
        {
            auto& context{ *static_cast<allocator_context_t*>(opaque_context) };
            constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

            if (context.boot_services == nullptr || context.boot_services->AllocatePages == nullptr ||
                context.boot_services->FreePages == nullptr || page_count == 0 || page_count > maximum_uintn)
                return EFI_INVALID_PARAMETER;

            EFI_PHYSICAL_ADDRESS physical_start{ 0 };
            const EFI_STATUS status{
                context.boot_services->AllocatePages(
                    AllocateAnyPages,
                    EfiLoaderData,
                    static_cast<UINTN>(page_count),
                    &physical_start)
            };

            if (EFI_ERROR(status)) return status;

            if (physical_start > UINTPTR_MAX)
            {
                static_cast<void>(context.boot_services->FreePages(physical_start, static_cast<UINTN>(page_count)));
                return EFI_UNSUPPORTED;
            }

            allocation = {
                physical_start,
                reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(physical_start)),
            };
            return EFI_SUCCESS;
        }

        [[nodiscard]] uint64_t free_pages(void* opaque_context, uint64_t physical_start,
                                          uint64_t page_count) noexcept
        {
            auto& context{ *static_cast<allocator_context_t*>(opaque_context) };
            constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

            if (context.boot_services == nullptr || context.boot_services->FreePages == nullptr ||
                page_count == 0 || page_count > maximum_uintn)
                return EFI_INVALID_PARAMETER;

            return context.boot_services->FreePages(physical_start, static_cast<UINTN>(page_count));
        }
    } // anonymous namespace

    boot_handoff_storage_error_t allocate_handoff_storage(
        EFI_SYSTEM_TABLE& system_table,
        const boot_handoff_storage_plan_t& plan,
        boot_handoff_storage_t& storage,
        EFI_STATUS& status) noexcept
    {
        allocator_context_t context{ system_table.BootServices };
        const boot_handoff_page_allocator_t allocator{ &context, allocate_pages, free_pages };
        uint64_t platform_status{ 0 };
        const boot_handoff_storage_error_t result{
            allocate_boot_handoff_storage(plan, allocator, storage, platform_status)
        };
        status = result != boot_handoff_storage_error_t::success && platform_status == 0
                     ? EFI_INVALID_PARAMETER
                     : platform_status;
        return result;
    }

    boot_handoff_storage_error_t release_handoff_storage(
        EFI_SYSTEM_TABLE& system_table,
        boot_handoff_storage_t& storage,
        EFI_STATUS& status) noexcept
    {
        allocator_context_t context{ system_table.BootServices };
        const boot_handoff_page_allocator_t allocator{ &context, allocate_pages, free_pages };
        uint64_t platform_status{ 0 };
        const boot_handoff_storage_error_t result{
            release_boot_handoff_storage(allocator, storage, platform_status)
        };
        status = result != boot_handoff_storage_error_t::success && platform_status == 0
                     ? EFI_INVALID_PARAMETER
                     : platform_status;
        return result;
    }
} // namespace warren::boot::uefi
