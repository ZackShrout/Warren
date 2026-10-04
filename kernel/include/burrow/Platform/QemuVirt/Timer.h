//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Arch/AArch64/ExceptionFrame.h>

#include <stdint.h>

namespace burrow::platform::qemu_virt {
    constexpr uint32_t k_gic_poll_limit{ 1000000 };

    enum class timer_start_error_t : uint32_t
    {
        success = 0,
        invalid_counter_frequency = 1,
        invalid_interval = 2,
        distributor_timeout = 3,
        redistributor_timeout = 4,
        cpu_interface_unavailable = 5,
    };
} // namespace burrow::platform::qemu_virt

extern "C" uint32_t burrow_qemu_virt_start_timer() noexcept;
extern "C" uint32_t burrow_aarch64_dispatch_irq(
    const burrow::arch::aarch64::exception_frame_t* frame) noexcept;
extern "C" uint32_t burrow_qemu_virt_publish_timer_tick() noexcept;
extern "C" [[gnu::visibility("hidden")]] uint64_t burrow_qemu_virt_timer_ticks;
