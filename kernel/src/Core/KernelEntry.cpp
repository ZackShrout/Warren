//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/KernelEntry.h>

#include <warren/boot/BootInformation.h>
#include <warren/boot/BootInformationValidation.h>

extern "C" [[gnu::visibility("hidden")]] [[gnu::used]] uint64_t
    burrow_kernel_entry_retained_witness{};

namespace burrow::core {
    namespace {
        constexpr uint64_t k_page_size{ 4096 };
        constexpr uint64_t k_upper_address_bit{ UINT64_C(1) << 63 };

        [[nodiscard]] bool is_upper_address(uint64_t address) noexcept
        {
            return (address & k_upper_address_bit) != 0;
        }

        [[nodiscard]] bool valid_context(const KernelEntryContext& context) noexcept
        {
            if (context.abi_major != k_kernel_entry_abi_major ||
                context.structure_size != k_kernel_entry_context_size ||
                context.flags != k_kernel_entry_flags ||
                !is_upper_address(context.boot_information_address) ||
                context.boot_information_physical_address == 0 ||
                is_upper_address(context.boot_information_physical_address) ||
                context.boot_information_byte_count < WARREN_BOOT_INFORMATION_HEADER_SIZE ||
                (context.initial_exception_level != 1 &&
                 context.initial_exception_level != 2) ||
                context.transition_arena_physical_address == 0 ||
                is_upper_address(context.transition_arena_physical_address) ||
                (context.transition_arena_physical_address & (k_page_size - 1)) != 0 ||
                context.transition_arena_page_count != k_kernel_entry_arena_page_count ||
                !is_upper_address(context.retained_witness_address) ||
                (context.retained_witness_address & (alignof(uint64_t) - 1)) != 0 ||
                context.retained_witness_address == context.boot_information_address)
                return false;

            if (context.boot_information_address >
                UINT64_MAX - context.boot_information_byte_count ||
                !is_upper_address(context.boot_information_address +
                                   context.boot_information_byte_count - 1))
                return false;

            const uint64_t arena_bytes{
                context.transition_arena_page_count * k_page_size
            };
            return context.transition_arena_physical_address <=
                UINT64_MAX - arena_bytes &&
                !is_upper_address(context.transition_arena_physical_address + arena_bytes - 1);
        }
    } // anonymous namespace

    uint32_t validate_kernel_entry(
        const KernelEntryContext& context,
        const void* readable_boot_information,
        volatile uint64_t* writable_witness) noexcept
    {
        if (!valid_context(context) || readable_boot_information == nullptr ||
            writable_witness == nullptr ||
            (reinterpret_cast<uintptr_t>(writable_witness) & (alignof(uint64_t) - 1)) != 0)
            return 0;

        const auto validation_result{ warren::boot::validate_boot_information(
            readable_boot_information,
            context.boot_information_byte_count,
            context.boot_information_physical_address)
        };
        if (validation_result != warren::boot::boot_information_error_t::success)
            return 0;

        const auto* boot_information{
            static_cast<const warren_boot_information_t*>(readable_boot_information)
        };
        if (boot_information->total_size != context.boot_information_byte_count)
            return 0;

        *writable_witness = k_kernel_entry_witness;
        return k_kernel_entry_success;
    }
} // namespace burrow::core

extern "C" [[gnu::visibility("hidden")]] uint32_t burrow_kernel_entry(
    const burrow::core::KernelEntryContext* context) noexcept
{
    const uintptr_t context_address{ reinterpret_cast<uintptr_t>(context) };
    if (context == nullptr || (context_address & (UINT64_C(1) << 63)) == 0 ||
        (context_address & (alignof(burrow::core::KernelEntryContext) - 1)) != 0 ||
        context_address > UINT64_MAX - sizeof(*context) ||
        ((context_address + sizeof(*context) - 1) & (UINT64_C(1) << 63)) == 0)
        return 0;

    return burrow::core::validate_kernel_entry(
        *context,
        reinterpret_cast<const void*>(
            static_cast<uintptr_t>(context->boot_information_address)),
        reinterpret_cast<volatile uint64_t*>(
            static_cast<uintptr_t>(context->retained_witness_address)));
}
