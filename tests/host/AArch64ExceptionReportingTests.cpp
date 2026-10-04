//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/ExceptionReporting.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct capture_t
    {
        char bytes[2048]{};
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

    [[nodiscard]] bool expect(bool condition, const char* name) noexcept
    {
        if (condition) return true;
        std::fprintf(stderr, "FAIL: %s\n", name);
        return false;
    }

    [[nodiscard]] burrow::arch::aarch64::exception_frame_t make_frame() noexcept
    {
        burrow::arch::aarch64::exception_frame_t frame{};
        frame.abi_major = burrow::arch::aarch64::k_exception_frame_abi_major;
        frame.structure_size = burrow::arch::aarch64::k_exception_frame_size;
        frame.vector = 4;
        for (uint32_t index{};
             index < burrow::arch::aarch64::k_exception_register_count;
             ++index)
            frame.registers[index] = UINT64_C(0x1111000000000000) + index;
        frame.stack_pointer = UINT64_C(0xffffd00000010ec0);
        frame.transition_stage = 9;
        frame.current_exception_level = 1;
        frame.esr = UINT64_C(0x00000000f200077a);
        frame.elr = UINT64_C(0xffff800012345678);
        frame.far = UINT64_C(0);
        frame.spsr = UINT64_C(0x3c5);
        return frame;
    }
}

int main()
{
    using burrow::arch::aarch64::exception_report_error_t;

    capture_t capture{};
    const burrow::drivers::console_writer_t writer{ &capture, capture_byte };
    auto frame{ make_frame() };
    bool passed{ expect(
        burrow::arch::aarch64::report_exception(frame, writer) ==
            exception_report_error_t::success,
        "complete report succeeds") };
    passed &= expect(
        std::strstr(capture.bytes,
                    "BURROW_EXCEPTION_V1:stage=9:vector=4:el=1:"
                    "esr=0x00000000F200077A:") == capture.bytes,
        "stable prefix and architectural state");
    passed &= expect(
        std::strstr(capture.bytes, ":sp=0xFFFFD00000010EC0:") != nullptr,
        "stack pointer");

    for (uint32_t index{};
         index < burrow::arch::aarch64::k_exception_register_count;
         ++index)
    {
        char expected[48]{};
        const int length{ std::snprintf(
            expected,
            sizeof(expected),
            ":x%u=0x%016llX",
            index,
            static_cast<unsigned long long>(frame.registers[index])) };
        passed &= expect(length > 0 && std::strstr(capture.bytes, expected) != nullptr,
                         "general register");
    }
    passed &= expect(capture.byte_count >= 2 &&
                         capture.bytes[capture.byte_count - 2] == '\r' &&
                         capture.bytes[capture.byte_count - 1] == '\n',
                     "report terminator");

    frame.abi_major = 2;
    passed &= expect(
        burrow::arch::aarch64::report_exception(frame, writer) ==
            exception_report_error_t::invalid_frame,
        "invalid ABI");
    frame = make_frame();
    frame.vector = 16;
    passed &= expect(
        burrow::arch::aarch64::report_exception(frame, writer) ==
            exception_report_error_t::invalid_frame,
        "invalid vector");
    frame = make_frame();
    frame.current_exception_level = 2;
    passed &= expect(
        burrow::arch::aarch64::report_exception(frame, writer) ==
            exception_report_error_t::invalid_frame,
        "invalid exception level");

    capture = {};
    capture.fail_at = 19;
    frame = make_frame();
    passed &= expect(
        burrow::arch::aarch64::report_exception(frame, writer) ==
            exception_report_error_t::output_failure,
        "mid-report failure");
    passed &= expect(capture.byte_count == 19, "bounded failure prefix");

    if (!passed) return 1;
    std::puts("Warren AArch64 exception reporting tests passed.");
    return 0;
}
