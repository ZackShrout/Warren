//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/GenericTimer.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct capture_t
    {
        char bytes[256]{};
        uint32_t byte_count{};
        uint32_t fail_at{ UINT32_MAX };
    };

    [[nodiscard]] bool capture_byte(void* context, uint8_t byte) noexcept
    {
        if (context == nullptr) return false;
        auto& capture{ *static_cast<capture_t*>(context) };
        if (capture.byte_count == capture.fail_at ||
            capture.byte_count >= sizeof(capture.bytes) - 1)
            return false;
        capture.bytes[capture.byte_count++] = static_cast<char>(byte);
        return true;
    }

    [[nodiscard]] bool expect_u64(const char* name,
                                  uint64_t actual,
                                  uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected %llu, received %llu\n",
                     name,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        return false;
    }
}

int main()
{
    using burrow::arch::aarch64::generic_timer_error_t;

    uint32_t interval{ UINT32_MAX };
    bool passed{ expect_u64(
        "reference interval result",
        static_cast<uint32_t>(burrow::arch::aarch64::calculate_timer_interval(
            UINT64_C(62500000), 100, interval)),
        static_cast<uint32_t>(generic_timer_error_t::success)) };
    passed &= expect_u64("reference interval", interval, 625000);

    passed &= expect_u64(
        "zero frequency",
        static_cast<uint32_t>(burrow::arch::aarch64::calculate_timer_interval(
            0, 100, interval)),
        static_cast<uint32_t>(generic_timer_error_t::invalid_frequency));
    passed &= expect_u64("failed interval cleared", interval, 0);
    passed &= expect_u64(
        "zero rate",
        static_cast<uint32_t>(burrow::arch::aarch64::calculate_timer_interval(
            62500000, 0, interval)),
        static_cast<uint32_t>(generic_timer_error_t::invalid_rate));
    passed &= expect_u64(
        "sub-tick interval",
        static_cast<uint32_t>(burrow::arch::aarch64::calculate_timer_interval(
            99, 100, interval)),
        static_cast<uint32_t>(generic_timer_error_t::interval_out_of_range));
    passed &= expect_u64(
        "signed TVAL overflow",
        static_cast<uint32_t>(burrow::arch::aarch64::calculate_timer_interval(
            (UINT64_C(0x7fffffff) + 1) * 100, 100, interval)),
        static_cast<uint32_t>(generic_timer_error_t::interval_out_of_range));

    capture_t capture{};
    const burrow::drivers::console_writer_t writer{ &capture, capture_byte };
    passed &= expect_u64(
        "timer report",
        static_cast<uint32_t>(burrow::arch::aarch64::report_timer_tick(
            writer, UINT64_C(62500000), 1)),
        static_cast<uint32_t>(generic_timer_error_t::success));
    passed &= expect_u64(
        "timer report text",
        std::strcmp(capture.bytes,
                    "BURROW_TIMER:source=cntp:interrupt=30:ticks=1:"
                    "frequency=62500000\r\n"),
        0);

    passed &= expect_u64(
        "zero tick count",
        static_cast<uint32_t>(burrow::arch::aarch64::report_timer_tick(
            writer, UINT64_C(62500000), 0)),
        static_cast<uint32_t>(generic_timer_error_t::invalid_tick_count));
    capture = {};
    capture.fail_at = 12;
    passed &= expect_u64(
        "bounded report failure",
        static_cast<uint32_t>(burrow::arch::aarch64::report_timer_tick(
            writer, UINT64_C(62500000), 1)),
        static_cast<uint32_t>(generic_timer_error_t::output_failure));
    passed &= expect_u64("bounded report prefix", capture.byte_count, 12);

    if (!passed) return 1;
    std::puts("Warren AArch64 generic timer tests passed.");
    return 0;
}
