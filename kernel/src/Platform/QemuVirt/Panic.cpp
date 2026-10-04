//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Platform/QemuVirt/Panic.h>

#include <burrow/Drivers/Pl011.h>
#include <burrow/Platform/QemuVirt/Console.h>

#include <stdint.h>

extern "C" void burrow_aarch64_mask_panic_interrupts() noexcept;
extern "C" [[noreturn]] void burrow_aarch64_terminate_panic(
    uint32_t result_code,
    uint64_t console_address) noexcept;

namespace
{
    uint32_t panic_latch{};
    constexpr char k_recursive_record[]{
        "BURROW_PANIC_V1:kind=panic:id=panic.recursion:file=unknown:line=1:"
        "message=recursive panic\r\n"
    };
    constexpr char k_report_failure[]{
        "BURROW_PANIC_V1:kind=panic:id=panic.report-failure:file=unknown:line=1:"
        "message=panic record rejected\r\n"
    };

    [[nodiscard]] burrow::drivers::console_writer_t make_console(
        burrow::drivers::pl011_device_t& device) noexcept
    {
        if (burrow::drivers::initialize_pl011(
                reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
                    burrow::platform::qemu_virt::k_pl011_virtual_address)),
                burrow::drivers::k_pl011_default_poll_limit,
                device) != burrow::drivers::pl011_error_t::success)
            return {};
        return burrow::drivers::make_pl011_writer(device);
    }

    void write_fallback(
        const burrow::drivers::console_writer_t& writer,
        const char* text,
        uint32_t byte_count) noexcept
    {
        static_cast<void>(burrow::drivers::write_console(writer, text, byte_count));
    }
}

extern "C" [[noreturn]] void burrow_qemu_virt_panic(
    const burrow::core::panic_record_t* record) noexcept
{
    burrow_aarch64_mask_panic_interrupts();

    burrow::drivers::pl011_device_t device{};
    const burrow::drivers::console_writer_t writer{ make_console(device) };
    if (burrow::core::claim_panic_path(panic_latch) ==
        burrow::core::panic_latch_result_t::recursive_entry)
    {
        write_fallback(writer, k_recursive_record, sizeof(k_recursive_record) - 1);
        burrow_aarch64_terminate_panic(
            static_cast<uint32_t>(burrow::core::panic_kind_t::kernel),
            burrow::platform::qemu_virt::k_pl011_virtual_address);
    }

    uint32_t result_code{
        static_cast<uint32_t>(burrow::core::panic_kind_t::kernel)
    };
    if (record != nullptr && record->kind == burrow::core::panic_kind_t::assertion)
        result_code = static_cast<uint32_t>(burrow::core::panic_kind_t::assertion);

    if (record == nullptr ||
        burrow::core::report_panic(*record, writer) !=
            burrow::core::panic_report_error_t::success)
        write_fallback(writer, k_report_failure, sizeof(k_report_failure) - 1);

    burrow_aarch64_terminate_panic(
        result_code,
        burrow::platform::qemu_virt::k_pl011_virtual_address);
}

#if defined(WARREN_AARCH64_FAULT_INJECT_ASSERTION)
extern "C" [[noreturn]] void burrow_qemu_virt_trigger_assertion_fixture() noexcept
{
    constexpr char identifier[]{ "phase1.assertion" };
    constexpr char file[]{ "kernel/src/Platform/QemuVirt/Panic.cpp" };
    constexpr char message[]{ "fixture assertion" };
    const burrow::core::panic_record_t record{
        burrow::core::panic_kind_t::assertion,
        identifier,
        sizeof(identifier) - 1,
        file,
        sizeof(file) - 1,
        __LINE__,
        message,
        sizeof(message) - 1,
    };
    burrow_qemu_virt_panic(&record);
}
#endif

#if defined(WARREN_AARCH64_FAULT_INJECT_PANIC)
extern "C" [[noreturn]] void burrow_qemu_virt_trigger_panic_fixture() noexcept
{
    constexpr char identifier[]{ "phase1.panic" };
    constexpr char file[]{ "kernel/src/Platform/QemuVirt/Panic.cpp" };
    constexpr char message[]{ "fixture panic" };
    const burrow::core::panic_record_t record{
        burrow::core::panic_kind_t::kernel,
        identifier,
        sizeof(identifier) - 1,
        file,
        sizeof(file) - 1,
        __LINE__,
        message,
        sizeof(message) - 1,
    };
    burrow_qemu_virt_panic(&record);
}
#endif
