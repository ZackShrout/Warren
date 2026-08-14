//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/UefiFile.h>

namespace warren::boot::uefi {
    namespace {
        constexpr uint64_t k_file_information_buffer_size{ 512 };

        CHAR16 burrow_path[]{
            '\\', 'E', 'F', 'I', '\\', 'W', 'A', 'R', 'R', 'E', 'N', '\\',
            'B', 'U', 'R', 'R', 'O', 'W', '.', 'E', 'L', 'F', 0
        };

        file_source_error_t fail(file_source_t& source, file_source_error_t error, EFI_STATUS status) noexcept
        {
            source.error = error;
            source.status = status;

            return error;
        }

        [[nodiscard]] bool read_file_bytes(void* opaque_source, uint64_t offset, uint8_t* destination,
                                           uint64_t byte_count) noexcept
        {
            auto& source{ *static_cast<file_source_t*>(opaque_source) };

            if (source.file == nullptr || source.file->SetPosition == nullptr || source.file->Read == nullptr)
            {
                fail(source, file_source_error_t::invalid_file_protocol, EFI_UNSUPPORTED);
                return false;
            }

            if (offset > source.byte_count || byte_count > source.byte_count - offset)
            {
                fail(source, file_source_error_t::read_out_of_bounds, EFI_BAD_BUFFER_SIZE);
                return false;
            }

            EFI_STATUS status{ source.file->SetPosition(source.file, offset) };
            if (EFI_ERROR(status))
            {
                fail(source, file_source_error_t::seek_failed, status);
                return false;
            }

            uint64_t completed{ 0 };
            constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

            while (completed < byte_count)
            {
                const uint64_t remaining{ byte_count - completed };
                const UINTN requested{ static_cast<UINTN>(remaining < maximum_uintn ? remaining : maximum_uintn) };
                UINTN actual{ requested };
                status = source.file->Read(source.file, &actual, destination + completed);

                if (EFI_ERROR(status))
                {
                    fail(source, file_source_error_t::read_failed, status);
                    return false;
                }

                if (actual == 0)
                {
                    fail(source, file_source_error_t::zero_progress, EFI_END_OF_FILE);
                    return false;
                }

                if (actual > requested)
                {
                    fail(source, file_source_error_t::short_read, EFI_DEVICE_ERROR);
                    return false;
                }

                completed += actual;
            }

            source.error = file_source_error_t::success;
            source.status = EFI_SUCCESS;

            return true;
        }
    } // anonymous namespace

    file_source_error_t open_burrow_file(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE& system_table,
                                         file_source_t& source) noexcept
    {
        source = { };
        source.image_handle = image_handle;
        source.boot_services = system_table.BootServices;

        if (image_handle == nullptr || source.boot_services == nullptr ||
            source.boot_services->OpenProtocol == nullptr || source.boot_services->CloseProtocol == nullptr)
            return fail(source, file_source_error_t::invalid_services, EFI_INVALID_PARAMETER);

        EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        void* interface{ nullptr };

        EFI_STATUS status{
            source.boot_services->OpenProtocol(image_handle, &loaded_image_guid, &interface, image_handle, nullptr,
                                               EFI_OPEN_PROTOCOL_GET_PROTOCOL)
        };

        if (EFI_ERROR(status))
            return fail(source, file_source_error_t::loaded_image_protocol, status);

        source.loaded_image_open = true;
        source.loaded_image = static_cast<EFI_LOADED_IMAGE_PROTOCOL*>(interface);

        if (source.loaded_image == nullptr || source.loaded_image->DeviceHandle == nullptr)
        {
            const file_source_error_t result{ fail(source, file_source_error_t::invalid_loaded_image, EFI_NOT_FOUND) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        source.device_handle = source.loaded_image->DeviceHandle;
        EFI_GUID filesystem_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
        interface = nullptr;

        status = source.boot_services->OpenProtocol(source.device_handle, &filesystem_guid, &interface, image_handle,
                                                    nullptr, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(status))
        {
            const file_source_error_t result{ fail(source, file_source_error_t::filesystem_protocol, status) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        source.filesystem_open = true;
        source.filesystem = static_cast<EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*>(interface);

        if (source.filesystem == nullptr || source.filesystem->OpenVolume == nullptr)
        {
            const file_source_error_t result{ fail(source, file_source_error_t::invalid_filesystem, EFI_UNSUPPORTED) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        status = source.filesystem->OpenVolume(source.filesystem, &source.root);

        if (EFI_ERROR(status))
        {
            const file_source_error_t result{ fail(source, file_source_error_t::open_volume, status) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }
        if (source.root == nullptr || source.root->Open == nullptr || source.root->Close == nullptr)
        {
            const file_source_error_t result{
                fail(source, file_source_error_t::invalid_file_protocol, EFI_UNSUPPORTED)
            };

            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        status = source.root->Open(source.root, &source.file, burrow_path, EFI_FILE_MODE_READ, 0);

        if (EFI_ERROR(status))
        {
            const file_source_error_t result{ fail(source, file_source_error_t::open_file, status) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        if (source.file == nullptr || source.file->Close == nullptr || source.file->GetInfo == nullptr || source.file->
            Read == nullptr || source.file->SetPosition == nullptr)
        {
            const file_source_error_t result{
                fail(source, file_source_error_t::invalid_file_protocol, EFI_UNSUPPORTED)
            };

            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        alignas(8) uint8_t information_buffer[k_file_information_buffer_size]{ };
        UINTN information_size{ sizeof(information_buffer) };
        EFI_GUID information_guid = EFI_FILE_INFO_ID;
        status = source.file->GetInfo(source.file, &information_guid, &information_size, information_buffer);

        if (EFI_ERROR(status))
        {
            const file_source_error_t result{ fail(source, file_source_error_t::get_file_info, status) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        const auto* information{ reinterpret_cast<const EFI_FILE_INFO*>(information_buffer) };

        if (information_size < SIZE_OF_EFI_FILE_INFO || information->Size < SIZE_OF_EFI_FILE_INFO ||
            information->Size > information_size || information->FileSize < 64 ||
            (information->Attribute & EFI_FILE_DIRECTORY) != 0)
        {
            const file_source_error_t result{ fail(source, file_source_error_t::invalid_file_info, EFI_LOAD_ERROR) };
            const EFI_STATUS original_status{ source.status };
            static_cast<void>(close_burrow_file(source));
            source.error = result;
            source.status = original_status;

            return result;
        }

        source.byte_count = information->FileSize;
        source.error = file_source_error_t::success;
        source.status = EFI_SUCCESS;

        return file_source_error_t::success;
    }

    file_source_error_t close_burrow_file(file_source_t& source) noexcept
    {
        file_source_error_t result{ file_source_error_t::success };
        EFI_STATUS first_status{ EFI_SUCCESS };

        if (source.file != nullptr)
        {
            const EFI_STATUS status{
                source.file->Close == nullptr ? EFI_UNSUPPORTED : source.file->Close(source.file)
            };

            if (EFI_ERROR(status) && result == file_source_error_t::success)
            {
                result = file_source_error_t::close_resource;
                first_status = status;
            }

            source.file = nullptr;
        }

        if (source.root != nullptr)
        {
            const EFI_STATUS status{
                source.root->Close == nullptr ? EFI_UNSUPPORTED : source.root->Close(source.root)
            };

            if (EFI_ERROR(status) && result == file_source_error_t::success)
            {
                result = file_source_error_t::close_resource;
                first_status = status;
            }

            source.root = nullptr;
        }

        if (source.filesystem_open && source.boot_services != nullptr)
        {
            EFI_GUID filesystem_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

            const EFI_STATUS status{
                source.boot_services->CloseProtocol(source.device_handle, &filesystem_guid, source.image_handle,
                                                    nullptr)
            };

            if (EFI_ERROR(status) && result == file_source_error_t::success)
            {
                result = file_source_error_t::close_resource;
                first_status = status;
            }

            source.filesystem_open = false;
            source.filesystem = nullptr;
        }

        if (source.loaded_image_open && source.boot_services != nullptr)
        {
            EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

            const EFI_STATUS status{
                source.boot_services->CloseProtocol(source.image_handle, &loaded_image_guid, source.image_handle,
                                                    nullptr)
            };

            if (EFI_ERROR(status) && result == file_source_error_t::success)
            {
                result = file_source_error_t::close_resource;
                first_status = status;
            }

            source.loaded_image_open = false;
            source.loaded_image = nullptr;
        }

        source.error = result;
        source.status = first_status;

        return result;
    }

    byte_source_t make_byte_source(file_source_t& source) noexcept
    {
        return { &source, source.byte_count, read_file_bytes };
    }

    const char* file_source_error_name(file_source_error_t error) noexcept
    {
        switch (error)
        {
            case file_source_error_t::success: return "success";
            case file_source_error_t::invalid_services: return "invalid-services";
            case file_source_error_t::loaded_image_protocol: return "loaded-image-protocol";
            case file_source_error_t::invalid_loaded_image: return "invalid-loaded-image";
            case file_source_error_t::filesystem_protocol: return "filesystem-protocol";
            case file_source_error_t::invalid_filesystem: return "invalid-filesystem";
            case file_source_error_t::open_volume: return "open-volume";
            case file_source_error_t::open_file: return "open-file";
            case file_source_error_t::invalid_file_protocol: return "invalid-file-protocol";
            case file_source_error_t::get_file_info: return "get-file-info";
            case file_source_error_t::invalid_file_info: return "invalid-file-info";
            case file_source_error_t::read_out_of_bounds: return "read-out-of-bounds";
            case file_source_error_t::seek_failed: return "seek-failed";
            case file_source_error_t::read_failed: return "read-failed";
            case file_source_error_t::short_read: return "short-read";
            case file_source_error_t::zero_progress: return "zero-progress";
            case file_source_error_t::close_resource: return "close-resource";
        }
        return "unknown";
    }
} // namespace warren::boot::uefi
