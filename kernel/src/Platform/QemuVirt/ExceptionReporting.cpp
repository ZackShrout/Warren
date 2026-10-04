//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/ExceptionReporting.h>
#include <burrow/Drivers/Pl011.h>
#include <burrow/Platform/QemuVirt/Console.h>

#include <stdint.h>

extern "C" [[gnu::visibility("hidden")]] uint64_t burrow_aarch64_report_exception(
    const burrow::arch::aarch64::exception_frame_t* frame) noexcept
{
    const uintptr_t address{ reinterpret_cast<uintptr_t>(frame) };
    if (frame == nullptr || (address & (UINT64_C(1) << 63)) == 0 ||
        (address & 0xf) != 0 || address > UINT64_MAX - sizeof(*frame) ||
        ((address + sizeof(*frame) - 1) & (UINT64_C(1) << 63)) == 0)
        return 0;

    burrow::drivers::pl011_device_t device{};
    if (burrow::drivers::initialize_pl011(
            reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
                burrow::platform::qemu_virt::k_pl011_virtual_address)),
            burrow::drivers::k_pl011_default_poll_limit,
            device) != burrow::drivers::pl011_error_t::success)
        return 0;

    const burrow::drivers::console_writer_t console{
        burrow::drivers::make_pl011_writer(device)
    };
    if (burrow::arch::aarch64::report_exception(*frame, console) !=
        burrow::arch::aarch64::exception_report_error_t::success)
        return 0;
    return burrow::platform::qemu_virt::k_pl011_virtual_address;
}
