//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/KernelEntry.h>
#include <burrow/Core/PhysicalMemory.h>
#include <burrow/Platform/QemuVirt/Console.h>

#include <stdint.h>

extern "C" [[gnu::visibility("hidden")]] [[gnu::used]] uint64_t
    burrow_kernel_entry_retained_witness{};
extern "C" [[gnu::visibility("hidden")]] [[gnu::used]]
    burrow::core::physical_memory_state_t burrow_physical_memory_state{};
extern "C" [[gnu::visibility("hidden")]] [[gnu::used]]
    burrow::core::boot_allocation_t burrow_boot_allocation{};

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

    burrow::core::validated_boot_information_t view{};
    if (burrow::core::consume_boot_information(
            boot_information,
            context->boot_information_byte_count,
            context->boot_information_physical_address,
            view) != warren::boot::boot_information_error_t::success ||
        burrow::core::initialize_physical_memory(
            view,
            { context->transition_arena_physical_address },
            { context->transition_arena_page_count },
            burrow_physical_memory_state) !=
            burrow::core::physical_memory_error_t::success ||
        burrow::core::allocate_boot_pages(
            burrow_physical_memory_state,
            { 4 },
            { 4 },
            burrow_boot_allocation) !=
            burrow::core::physical_memory_error_t::success)
        return 0;

#if defined(WARREN_AARCH64_FAULT_INJECT_PHYSICAL_MEMORY)
    asm volatile(
        "mov w15, #0xf11b\n"
        "mov w0, wzr"
        :
        :
        : "x0", "x15");
    return 0;
#endif

    if (!burrow::core::publish_kernel_entry_readiness(console) ||
        burrow::core::report_physical_memory(
            burrow_physical_memory_state,
            burrow_boot_allocation,
            console) != burrow::core::physical_memory_error_t::success)
        return 0;

    return burrow::core::retain_kernel_entry_witness(
        reinterpret_cast<volatile uint64_t*>(
            static_cast<uintptr_t>(context->retained_witness_address)));
}
