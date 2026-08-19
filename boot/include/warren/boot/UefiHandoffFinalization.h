//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BootHandoffFinalization.h>
#include <warren/boot/Uefi.h>

namespace warren::boot::uefi {
    [[nodiscard]] boot_handoff_finalization_error_t finalize_handoff(
        EFI_HANDLE image_handle,
        EFI_SYSTEM_TABLE& system_table,
        const burrow_loaded_image_t& loaded_image,
        const warren_boot_early_console_t& early_console,
        boot_handoff_storage_t& storage,
        boot_handoff_finalization_result_t& result) noexcept;
} // namespace warren::boot::uefi
