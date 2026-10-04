//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Arch/AArch64/ExceptionFrame.h>
#include <burrow/Drivers/Console.h>

#include <stdint.h>

namespace burrow::arch::aarch64 {
    enum class exception_report_error_t : uint32_t
    {
        success = 0,
        invalid_frame = 1,
        output_failure = 2,
    };

    [[nodiscard]] exception_report_error_t report_exception(
        const exception_frame_t& frame,
        const drivers::console_writer_t& console) noexcept;
} // namespace burrow::arch::aarch64

extern "C" uint64_t burrow_aarch64_report_exception(
    const burrow::arch::aarch64::exception_frame_t* frame) noexcept;
