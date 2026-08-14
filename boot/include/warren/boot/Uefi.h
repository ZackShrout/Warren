//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

// This is the one deliberate containment boundary for the imported EDK2
// interfaces used by Warren's bootloader. Warren source outside boot/ must not
// include EDK2 headers directly.
#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

namespace warren::boot::uefi {
    [[nodiscard]] inline EFI_STATUS write(EFI_SYSTEM_TABLE& system_table, CHAR16* text) noexcept
    {
        if (system_table.ConOut == nullptr || system_table.ConOut->OutputString == nullptr)
            return EFI_UNSUPPORTED;

        return system_table.ConOut->OutputString(system_table.ConOut, text);
    }
} // namespace warren::boot::uefi
