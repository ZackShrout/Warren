//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/AArch64Handoff.h>

#include <stdint.h>

extern "C" void warren_aarch64_synchronize_instruction_range(
    uint64_t physical_begin,
    uint64_t physical_end) noexcept;

extern "C" [[noreturn]] void warren_aarch64_handoff(
    uint64_t boot_information_physical_address,
    uint64_t entry_physical_address,
    uint64_t bootstrap_stack_top,
    uint64_t pl011_physical_address) noexcept;

extern "C" [[noreturn]] void warren_aarch64_post_exit_failure(
    uint64_t pl011_physical_address,
    uint64_t test_result_handler) noexcept;

#if defined(WARREN_ENABLE_QEMU_POST_EXIT_FAILURE_RESULT)
extern "C" [[noreturn]] void warren_qemu_post_exit_failure_result(
    uint64_t pl011_physical_address) noexcept;
#endif

namespace warren::boot {
    bool synchronize_aarch64_instruction_cache(
        const aarch64_executable_range_t& executable_range) noexcept
    {
        if (executable_range.physical_begin >= executable_range.physical_end)
            return false;

        warren_aarch64_synchronize_instruction_range(
            executable_range.physical_begin,
            executable_range.physical_end);
        return true;
    }

    [[noreturn]] void transfer_to_burrow(
        const aarch64_handoff_arguments_t& arguments) noexcept
    {
        warren_aarch64_handoff(
            arguments.boot_information_physical_address,
            arguments.entry_physical_address,
            arguments.bootstrap_stack_top,
            arguments.pl011_physical_address);
    }

    [[noreturn]] void wait_after_aarch64_handoff_failure(
        uint64_t pl011_physical_address) noexcept
    {
#if defined(WARREN_ENABLE_QEMU_POST_EXIT_FAILURE_RESULT)
        warren_aarch64_post_exit_failure(
            pl011_physical_address,
            reinterpret_cast<uintptr_t>(&warren_qemu_post_exit_failure_result));
#else
        warren_aarch64_post_exit_failure(pl011_physical_address, 0);
#endif
    }
} // namespace warren::boot
