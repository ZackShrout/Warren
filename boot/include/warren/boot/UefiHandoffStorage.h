//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootHandoffStorage.h>
#include <warren/boot/Uefi.h>

namespace warren::boot::uefi {
    [[nodiscard]] boot_handoff_storage_error_t allocate_handoff_storage(
        EFI_SYSTEM_TABLE& system_table,
        const boot_handoff_storage_plan_t& plan,
        boot_handoff_storage_t& storage,
        EFI_STATUS& status) noexcept;

    // Cleanup is valid only before the first ExitBootServices() attempt.
    [[nodiscard]] boot_handoff_storage_error_t release_handoff_storage(
        EFI_SYSTEM_TABLE& system_table,
        boot_handoff_storage_t& storage,
        EFI_STATUS& status) noexcept;
} // namespace warren::boot::uefi
