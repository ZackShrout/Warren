//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Platform/QemuVirt/Monitor.h>

#include <burrow/Core/DiagnosticMonitor.h>
#include <burrow/Core/TransitionPlan.h>
#include <burrow/Drivers/Pl011.h>
#include <burrow/Platform/QemuVirt/Timer.h>

#include <stdint.h>

extern "C" [[gnu::visibility("hidden")]] uint32_t
    burrow_qemu_virt_run_monitor() noexcept
{
    burrow::drivers::pl011_device_t device{};
    if (burrow::drivers::initialize_pl011(
            reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
                burrow::core::k_reference_pl011_virtual_address)),
            burrow::drivers::k_pl011_monitor_poll_limit,
            device) != burrow::drivers::pl011_error_t::success)
        return 1;

    const burrow::drivers::console_reader_t reader{
        burrow::drivers::make_pl011_reader(device)
    };
    const burrow::drivers::console_writer_t writer{
        burrow::drivers::make_pl011_writer(device)
    };
    return burrow::core::run_diagnostic_monitor(
               reader,
               writer,
               burrow_qemu_virt_timer_ticks) ==
            burrow::core::diagnostic_monitor_error_t::success ? 0U : 1U;
}
