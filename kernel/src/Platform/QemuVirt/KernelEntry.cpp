//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/KernelEntry.h>
#include <burrow/Platform/QemuVirt/Console.h>

#include <stdint.h>

extern "C" [[gnu::visibility("hidden")]] [[gnu::used]] uint64_t
    burrow_kernel_entry_retained_witness{};

extern "C" [[gnu::visibility("hidden")]] uint32_t burrow_kernel_entry(
    const burrow::core::KernelEntryContext* context) noexcept
{
    const uintptr_t context_address{ reinterpret_cast<uintptr_t>(context) };
    if (context == nullptr || (context_address & (UINT64_C(1) << 63)) == 0 ||
        (context_address & (alignof(burrow::core::KernelEntryContext) - 1)) != 0 ||
        context_address > UINT64_MAX - sizeof(*context) ||
        ((context_address + sizeof(*context) - 1) & (UINT64_C(1) << 63)) == 0)
        return 0;

    const auto* boot_information{ reinterpret_cast<const warren_boot_information_t*>(
        static_cast<uintptr_t>(context->boot_information_address)) };
    if (burrow::core::validate_kernel_entry(*context, boot_information) !=
        burrow::core::kernel_entry_error_t::success)
        return 0;

    burrow::drivers::pl011_device_t device{};
    burrow::drivers::console_reader_t input{};
    burrow::drivers::console_writer_t console{};
    if (burrow::platform::qemu_virt::select_early_console(
            *boot_information, device, input, console) !=
        burrow::platform::qemu_virt::console_selection_error_t::success)
        return 0;

    return burrow::core::publish_kernel_entry(
        console,
        reinterpret_cast<volatile uint64_t*>(
            static_cast<uintptr_t>(context->retained_witness_address)));
}
