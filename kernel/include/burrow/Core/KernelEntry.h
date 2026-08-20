//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#define BURROW_KERNEL_ENTRY_ABI_MAJOR UINT32_C(1)
#define BURROW_KERNEL_ENTRY_CONTEXT_SIZE UINT32_C(64)
#define BURROW_KERNEL_ENTRY_FLAG_OWNED_TABLES (UINT64_C(1) << 0)
#define BURROW_KERNEL_ENTRY_FLAG_IDENTITY_REMOVED (UINT64_C(1) << 1)
#define BURROW_KERNEL_ENTRY_FLAGS \
    (BURROW_KERNEL_ENTRY_FLAG_OWNED_TABLES | BURROW_KERNEL_ENTRY_FLAG_IDENTITY_REMOVED)
#define BURROW_KERNEL_ENTRY_ARENA_PAGE_COUNT UINT64_C(128)
#define BURROW_KERNEL_ENTRY_SUCCESS UINT32_C(0x57415231)
#define BURROW_KERNEL_ENTRY_WITNESS UINT64_C(0x57415252454e3031)

typedef struct burrow_kernel_entry_context
{
    uint32_t abi_major;
    uint32_t structure_size;
    uint64_t flags;
    uint64_t boot_information_address;
    uint64_t boot_information_physical_address;
    uint32_t boot_information_byte_count;
    uint32_t initial_exception_level;
    uint64_t transition_arena_physical_address;
    uint64_t transition_arena_page_count;
    uint64_t retained_witness_address;
} burrow_kernel_entry_context_t;

#if defined(__cplusplus)
namespace burrow::core {
    constexpr uint32_t k_kernel_entry_abi_major{ BURROW_KERNEL_ENTRY_ABI_MAJOR };
    constexpr uint32_t k_kernel_entry_context_size{ BURROW_KERNEL_ENTRY_CONTEXT_SIZE };
    constexpr uint64_t k_kernel_entry_flag_owned_tables{
        BURROW_KERNEL_ENTRY_FLAG_OWNED_TABLES
    };
    constexpr uint64_t k_kernel_entry_flag_identity_removed{
        BURROW_KERNEL_ENTRY_FLAG_IDENTITY_REMOVED
    };
    constexpr uint64_t k_kernel_entry_flags{ BURROW_KERNEL_ENTRY_FLAGS };
    constexpr uint64_t k_kernel_entry_arena_page_count{
        BURROW_KERNEL_ENTRY_ARENA_PAGE_COUNT
    };
    constexpr uint32_t k_kernel_entry_success{ BURROW_KERNEL_ENTRY_SUCCESS };
    constexpr uint64_t k_kernel_entry_witness{ BURROW_KERNEL_ENTRY_WITNESS };

    using KernelEntryContext = ::burrow_kernel_entry_context_t;

    [[nodiscard]] uint32_t validate_kernel_entry(
        const KernelEntryContext& context,
        const void* readable_boot_information,
        volatile uint64_t* writable_witness) noexcept;
} // namespace burrow::core

extern "C" uint32_t burrow_kernel_entry(
    const burrow::core::KernelEntryContext* context) noexcept;

#define BURROW_KERNEL_ENTRY_STATIC_ASSERT(condition) static_assert(condition)
#define BURROW_KERNEL_ENTRY_ALIGNOF(type) alignof(type)
#else
uint32_t burrow_kernel_entry(const burrow_kernel_entry_context_t* context);

#define BURROW_KERNEL_ENTRY_STATIC_ASSERT(condition) _Static_assert(condition, #condition)
#define BURROW_KERNEL_ENTRY_ALIGNOF(type) _Alignof(type)
#endif

BURROW_KERNEL_ENTRY_STATIC_ASSERT(sizeof(burrow_kernel_entry_context_t) == 64);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(BURROW_KERNEL_ENTRY_ALIGNOF(
    burrow_kernel_entry_context_t) == 8);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t, abi_major) == 0x00);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t, structure_size) == 0x04);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t, flags) == 0x08);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           boot_information_address) == 0x10);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           boot_information_physical_address) == 0x18);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           boot_information_byte_count) == 0x20);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           initial_exception_level) == 0x24);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           transition_arena_physical_address) == 0x28);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           transition_arena_page_count) == 0x30);
BURROW_KERNEL_ENTRY_STATIC_ASSERT(offsetof(burrow_kernel_entry_context_t,
                                           retained_witness_address) == 0x38);

#undef BURROW_KERNEL_ENTRY_STATIC_ASSERT
#undef BURROW_KERNEL_ENTRY_ALIGNOF
