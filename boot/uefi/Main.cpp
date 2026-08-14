//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BurrowLoader.h>
#include <warren/boot/Uefi.h>
#include <warren/boot/UefiFile.h>

namespace {
    CHAR16 banner[]{
        'B', 'u', 'n', 'n', 'y', 'S', 'o', 'f', 't', ' ', 'W', 'a', 'r', 'r',
        'e', 'n', '\r', '\n', 0
    };

    CHAR16 begin_marker[]{
        'W', 'A', 'R', 'R', 'E', 'N', '_', 'T', 'E', 'S', 'T', ':', '1', ':',
        'B', 'E', 'G', 'I', 'N', ':', 'b', 'u', 'r', 'r', 'o', 'w', '-', 'l',
        'o', 'a', 'd', 'e', 'r', '\r', '\n', 0
    };

    CHAR16 success_marker[]{
        'W', 'A', 'R', 'R', 'E', 'N', '_', 'T', 'E', 'S', 'T', ':', '1', ':',
        'P', 'A', 'S', 'S', ':', 'b', 'u', 'r', 'r', 'o', 'w', '-', 'l', 'o',
        'a', 'd', 'e', 'r', '\r', '\n', 0
    };

    CHAR16 failure_marker[]{
        'W', 'A', 'R', 'R', 'E', 'N', '_', 'T', 'E', 'S', 'T', ':', '1', ':',
        'F', 'A', 'I', 'L', ':', 'b', 'u', 'r', 'r', 'o', 'w', '-', 'l', 'o',
        'a', 'd', 'e', 'r', ':', '6', '4', '\r', '\n', 0
    };

    [[nodiscard]] EFI_STATUS write_ascii(EFI_SYSTEM_TABLE& system_table, const char* text) noexcept
    {
        if (text == nullptr) return EFI_INVALID_PARAMETER;

        CHAR16 buffer[128]{ };
        uint32_t length{ 0 };

        while (text[length] != 0 && length < 125)
        {
            const unsigned char value{ static_cast<unsigned char>(text[length]) };
            buffer[length] = value <= 0x7f ? value : '?';
            ++length;
        }

        buffer[length++] = '\r';
        buffer[length++] = '\n';
        buffer[length] = 0;

        return warren::boot::uefi::write(system_table, buffer);
    }

    [[nodiscard]] EFI_STATUS write_hex_line(EFI_SYSTEM_TABLE& system_table, uint64_t image_start, uint64_t image_size,
                                            uint64_t load_bias, uint64_t entry) noexcept
    {
        constexpr char digits[]{ "0123456789ABCDEF" };
        constexpr char prefix[]{ "BURROW_LOADED start=0x" };
        constexpr char size_label[]{ " size=0x" };
        constexpr char bias_label[]{ " bias=0x" };
        constexpr char entry_label[]{ " entry=0x" };
        CHAR16 buffer[128]{ };
        uint32_t output{ 0 };

        const auto append_text = [&buffer, &output](const char* text) noexcept {
            for (uint32_t index{ 0 }; text[index] != 0; ++index)
                buffer[output++] = static_cast<CHAR16>(text[index]);
        };

        const auto append_hex = [&buffer, &output](uint64_t value) noexcept {
            for (uint32_t index{ 0 }; index < 16; ++index)
            {
                const uint32_t shift{ 60U - index * 4U };
                buffer[output++] = digits[(value >> shift) & 0xfU];
            }
        };

        append_text(prefix);
        append_hex(image_start);
        append_text(size_label);
        append_hex(image_size);
        append_text(bias_label);
        append_hex(load_bias);
        append_text(entry_label);
        append_hex(entry);
        buffer[output++] = '\r';
        buffer[output++] = '\n';
        buffer[output] = 0;

        return warren::boot::uefi::write(system_table, buffer);
    }

    [[nodiscard]] EFI_STATUS write_status_line(EFI_SYSTEM_TABLE& system_table, uint64_t value) noexcept
    {
        constexpr char prefix[]{ "BURROW_STATUS value=0x" };
        constexpr char digits[]{ "0123456789ABCDEF" };
        CHAR16 buffer[48]{ };
        uint32_t output{ 0 };

        for (uint32_t index{ 0 }; prefix[index] != 0; ++index)
            buffer[output++] = static_cast<CHAR16>(prefix[index]);

        for (uint32_t index{ 0 }; index < 16; ++index)
        {
            const uint32_t shift{ 60U - index * 4U };
            buffer[output++] = digits[(value >> shift) & 0xfU];
        }

        buffer[output++] = '\r';
        buffer[output++] = '\n';
        buffer[output] = 0;

        return warren::boot::uefi::write(system_table, buffer);
    }

    [[noreturn]] void reset_and_wait(EFI_SYSTEM_TABLE& system_table, EFI_STATUS status) noexcept
    {
        if (system_table.RuntimeServices != nullptr && system_table.RuntimeServices->ResetSystem != nullptr)
            system_table.RuntimeServices->ResetSystem(EfiResetShutdown, status, 0, nullptr);

        for (;;)
            asm volatile("wfe");
    }

    [[noreturn]] void fail_and_shutdown(EFI_SYSTEM_TABLE& system_table, const char* stage, const char* detail,
                                        uint64_t status_value) noexcept
    {
        static_cast<void>(write_ascii(system_table, stage));
        static_cast<void>(write_ascii(system_table, detail));
        static_cast<void>(write_status_line(system_table, status_value));
        static_cast<void>(warren::boot::uefi::write(system_table, failure_marker));
        reset_and_wait(system_table, EFI_ABORTED);
    }

    void clear_pages(uint8_t* pages, uint64_t byte_count) noexcept
    {
        if (pages == nullptr) return;

        for (uint64_t index{ 0 }; index < byte_count; ++index)
            pages[index] = 0;
    }
} // anonymous namespace

extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table) noexcept
{
    using warren::boot::burrow_load_error_name;
    using warren::boot::burrow_load_error_t;
    using warren::boot::burrow_load_plan_t;
    using warren::boot::burrow_loaded_image_t;
    using warren::boot::byte_source_t;
    using warren::boot::materialize_burrow_image;
    using warren::boot::plan_burrow_image;
    using warren::boot::uefi::close_burrow_file;
    using warren::boot::uefi::file_source_error_name;
    using warren::boot::uefi::file_source_error_t;
    using warren::boot::uefi::file_source_t;
    using warren::boot::uefi::make_byte_source;
    using warren::boot::uefi::open_burrow_file;

    if (system_table == nullptr) return EFI_INVALID_PARAMETER;

    EFI_STATUS status{ warren::boot::uefi::write(*system_table, banner) };

    if (EFI_ERROR(status)) return status;

    status = warren::boot::uefi::write(*system_table, begin_marker);

    if (EFI_ERROR(status)) return status;

    file_source_t file_source{ };
    const file_source_error_t open_result{ open_burrow_file(image_handle, *system_table, file_source) };

    if (open_result != file_source_error_t::success)
        fail_and_shutdown(*system_table, "BURROW_FILE_OPEN", file_source_error_name(open_result), file_source.status);

    const byte_source_t source{ make_byte_source(file_source) };
    burrow_load_plan_t plan{ };
    const burrow_load_error_t plan_result{ plan_burrow_image(source, plan) };

    if (plan_result != burrow_load_error_t::success)
    {
        const file_source_error_t read_result{ file_source.error };
        const EFI_STATUS read_status{ file_source.status };
        static_cast<void>(close_burrow_file(file_source));

        fail_and_shutdown(*system_table, "BURROW_LOAD_PLAN",
                          plan_result == burrow_load_error_t::source_read_failed
                              ? file_source_error_name(read_result)
                              : burrow_load_error_name(plan_result),
                          plan_result == burrow_load_error_t::source_read_failed
                              ? read_status
                              : static_cast<uint32_t>(plan_result));
    }

    if (system_table->BootServices == nullptr || system_table->BootServices->AllocatePages == nullptr ||
        system_table->BootServices->FreePages == nullptr || plan.allocation_size == 0 ||
        plan.allocation_size % warren::boot::k_burrow_page_size != 0)
    {
        static_cast<void>(close_burrow_file(file_source));
        fail_and_shutdown(*system_table, "BURROW_ALLOCATE", "invalid-boot-services", EFI_UNSUPPORTED);
    }

    const uint64_t page_count{ plan.allocation_size / warren::boot::k_burrow_page_size };
    constexpr uint64_t maximum_uintn{ static_cast<uint64_t>(~static_cast<UINTN>(0)) };

    if (page_count > maximum_uintn)
    {
        static_cast<void>(close_burrow_file(file_source));
        fail_and_shutdown(*system_table, "BURROW_ALLOCATE", "page-count-overflow",
                          static_cast<uint32_t>(burrow_load_error_t::allocation_span_overflow));
    }

    EFI_PHYSICAL_ADDRESS physical_start{ 0 };
    status = system_table->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, static_cast<UINTN>(page_count),
                                                       &physical_start);

    if (EFI_ERROR(status))
    {
        static_cast<void>(close_burrow_file(file_source));
        fail_and_shutdown(*system_table, "BURROW_ALLOCATE", "firmware-allocation-failed", status);
    }

    if (physical_start > UINTPTR_MAX)
    {
        static_cast<void>(system_table->BootServices->FreePages(physical_start, static_cast<UINTN>(page_count)));
        static_cast<void>(close_burrow_file(file_source));
        fail_and_shutdown(*system_table, "BURROW_ALLOCATE", "physical-address-overflow", physical_start);
    }

    auto* destination{ reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(physical_start)) };
    burrow_loaded_image_t loaded_image{ };

    const burrow_load_error_t materialize_result{
        materialize_burrow_image(source, plan, physical_start, destination, plan.allocation_size, loaded_image)
    };

    if (materialize_result != burrow_load_error_t::success)
    {
        const file_source_error_t read_result{ file_source.error };
        const EFI_STATUS read_status{ file_source.status };
        clear_pages(destination, plan.allocation_size);
        static_cast<void>(system_table->BootServices->FreePages(physical_start, static_cast<UINTN>(page_count)));
        static_cast<void>(close_burrow_file(file_source));

        fail_and_shutdown(*system_table, "BURROW_MATERIALIZE",
                          materialize_result == burrow_load_error_t::source_read_failed
                              ? file_source_error_name(read_result)
                              : burrow_load_error_name(materialize_result),
                          materialize_result == burrow_load_error_t::source_read_failed
                              ? read_status
                              : static_cast<uint32_t>(materialize_result));
    }

    const file_source_error_t close_result{ close_burrow_file(file_source) };

    if (close_result != file_source_error_t::success)
    {
        clear_pages(destination, plan.allocation_size);
        static_cast<void>(system_table->BootServices->FreePages(physical_start, static_cast<UINTN>(page_count)));
        fail_and_shutdown(*system_table, "BURROW_FILE_CLOSE", file_source_error_name(close_result), file_source.status);
    }

    status = write_hex_line(*system_table, loaded_image.physical_start, loaded_image.physical_size,
                            loaded_image.load_bias, loaded_image.entry_physical_address);

    if (EFI_ERROR(status))
        fail_and_shutdown(*system_table, "BURROW_DIAGNOSTIC", "console-write-failed", status);

    status = warren::boot::uefi::write(*system_table, success_marker);

    if (EFI_ERROR(status)) return status;

    reset_and_wait(*system_table, EFI_SUCCESS);
}
