//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/Uefi.h>

namespace
{
    // EDK2 deliberately defines CHAR16 independently of the host compiler's
    // wchar_t. Keeping firmware strings explicit prevents an accidental ABI
    // dependency on -fshort-wchar or the macOS host representation.
    CHAR16 banner[]{
        'B', 'u', 'n', 'n', 'y', 'S', 'o', 'f', 't', ' ', 'W', 'a', 'r', 'r',
        'e', 'n', '\r', '\n', 0
    };

    CHAR16 begin_marker[]{
        'W', 'A', 'R', 'R', 'E', 'N', '_', 'T', 'E', 'S', 'T', ':', '1', ':',
        'B', 'E', 'G', 'I', 'N', ':', 'u', 'e', 'f', 'i', '-', 'f', 'i', 'r',
        's', 't', '-', 'l', 'i', 'g', 'h', 't', '\r', '\n', 0
    };

    CHAR16 success_marker[]{
        'W', 'A', 'R', 'R', 'E', 'N', '_', 'T', 'E', 'S', 'T', ':', '1', ':',
        'P', 'A', 'S', 'S', ':', 'u', 'e', 'f', 'i', '-', 'f', 'i', 'r', 's',
        't', '-', 'l', 'i', 'g', 'h', 't', '\r', '\n', 0
    };
}

extern "C" EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table) noexcept
{
    static_cast<void>(image_handle);

    if (system_table == nullptr)
        return EFI_INVALID_PARAMETER;

    EFI_STATUS status{ warren::boot::uefi::write(*system_table, banner) };
    if (EFI_ERROR(status))
        return status;

    status = warren::boot::uefi::write(*system_table, begin_marker);
    if (EFI_ERROR(status))
        return status;

    status = warren::boot::uefi::write(*system_table, success_marker);
    if (EFI_ERROR(status))
        return status;

    if (system_table->RuntimeServices == nullptr ||
        system_table->RuntimeServices->ResetSystem == nullptr)
        return EFI_SUCCESS;

    system_table->RuntimeServices->ResetSystem(
        EfiResetShutdown,
        EFI_SUCCESS,
        0,
        nullptr);

    // ResetSystem should not return after a successful shutdown. If a firmware
    // implementation does return, remain quiescent rather than returning into
    // an unknown boot-manager path.
    for (;;)
        asm volatile("wfe");
}
