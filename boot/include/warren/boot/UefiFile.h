//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <warren/boot/BurrowLoader.h>
#include <warren/boot/Uefi.h>

namespace warren::boot::uefi {
    enum class file_source_error_t : uint32_t
    {
        success = 0,
        invalid_services,
        loaded_image_protocol,
        invalid_loaded_image,
        filesystem_protocol,
        invalid_filesystem,
        open_volume,
        open_file,
        invalid_file_protocol,
        get_file_info,
        invalid_file_info,
        read_out_of_bounds,
        seek_failed,
        read_failed,
        short_read,
        zero_progress,
        close_resource
    };

    struct file_source_t
    {
        EFI_BOOT_SERVICES* boot_services;
        EFI_HANDLE image_handle;
        EFI_HANDLE device_handle;
        EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem;
        EFI_FILE_PROTOCOL* root;
        EFI_FILE_PROTOCOL* file;
        uint64_t byte_count;
        file_source_error_t error;
        EFI_STATUS status;
        bool loaded_image_open;
        bool filesystem_open;
    };

    [[nodiscard]] file_source_error_t open_burrow_file(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE& system_table,
                                                       file_source_t& source) noexcept;

    [[nodiscard]] file_source_error_t close_burrow_file(file_source_t& source) noexcept;
    [[nodiscard]] byte_source_t make_byte_source(file_source_t& source) noexcept;
    [[nodiscard]] const char* file_source_error_name(file_source_error_t error) noexcept;
} // namespace warren::boot::uefi
