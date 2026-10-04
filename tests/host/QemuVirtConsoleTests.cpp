//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Platform/QemuVirt/Console.h>

#include <cstdint>
#include <cstdio>

namespace
{
    struct fixture_t
    {
        alignas(8) uint8_t bytes[WARREN_BOOT_INFORMATION_HEADER_SIZE +
                                 sizeof(warren_boot_early_console_t)]{};
    };

    [[nodiscard]] warren_boot_information_t& header(fixture_t& fixture) noexcept
    {
        return *reinterpret_cast<warren_boot_information_t*>(fixture.bytes);
    }

    [[nodiscard]] warren_boot_early_console_t& console(fixture_t& fixture) noexcept
    {
        return *reinterpret_cast<warren_boot_early_console_t*>(
            fixture.bytes + WARREN_BOOT_INFORMATION_HEADER_SIZE);
    }

    [[nodiscard]] fixture_t make_fixture() noexcept
    {
        fixture_t fixture{};
        warren_boot_information_t& object{ header(fixture) };
        object.header_size = WARREN_BOOT_INFORMATION_HEADER_SIZE;
        object.total_size = sizeof(fixture.bytes);
        object.present_features = WARREN_BOOT_FEATURE_EARLY_CONSOLE;
        object.early_console = {
            WARREN_BOOT_INFORMATION_HEADER_SIZE,
            1,
            sizeof(warren_boot_early_console_t),
            0,
        };
        console(fixture) = {
            WARREN_BOOT_CONSOLE_PL011,
            WARREN_BOOT_CONSOLE_OUTPUT,
            burrow::platform::qemu_virt::k_pl011_physical_address,
            sizeof(uint32_t),
            32,
            0,
            0,
            0,
            { 0, 0, 0 },
        };
        return fixture;
    }

    [[nodiscard]] bool expect_u64(const char* name,
                                  uint64_t actual,
                                  uint64_t expected) noexcept
    {
        if (actual == expected) return true;
        std::fprintf(stderr, "FAIL: %s: expected 0x%llx, received 0x%llx\n",
                     name,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] uint32_t select(fixture_t& fixture) noexcept
    {
        burrow::drivers::pl011_device_t device{};
        burrow::drivers::console_writer_t writer{};
        return static_cast<uint32_t>(burrow::platform::qemu_virt::select_early_console(
            header(fixture), device, writer));
    }
}

int main()
{
    using burrow::platform::qemu_virt::console_selection_error_t;

    fixture_t fixture{ make_fixture() };
    burrow::drivers::pl011_device_t device{};
    burrow::drivers::console_writer_t writer{};
    bool passed{ expect_u64(
        "valid console",
        static_cast<uint32_t>(burrow::platform::qemu_virt::select_early_console(
            header(fixture), device, writer)),
        static_cast<uint32_t>(console_selection_error_t::success)) };
    passed &= expect_u64(
        "stable virtual alias",
        reinterpret_cast<uintptr_t>(device.registers),
        burrow::platform::qemu_virt::k_pl011_virtual_address);
    passed &= expect_u64("poll limit", device.transmit_poll_limit,
                         burrow::drivers::k_pl011_default_poll_limit);
    passed &= expect_u64("writer context", writer.context == &device ? 1 : 0, 1);
    passed &= expect_u64("writer function",
                         writer.write_byte == burrow::drivers::write_pl011_byte ? 1 : 0,
                         1);

    fixture = make_fixture();
    header(fixture).present_features = 0;
    passed &= expect_u64("missing feature", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::missing_console));
    fixture = make_fixture();
    header(fixture).early_console.count = 0;
    passed &= expect_u64("invalid section", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::invalid_section));
    fixture = make_fixture();
    header(fixture).early_console.offset++;
    passed &= expect_u64("misaligned section", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::invalid_section));
    fixture = make_fixture();
    console(fixture).kind++;
    passed &= expect_u64("unsupported kind", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).flags = WARREN_BOOT_CONSOLE_INPUT;
    passed &= expect_u64("missing output", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).flags |= UINT32_C(0x80000000);
    passed &= expect_u64("unknown flag", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).physical_address += 4096;
    passed &= expect_u64("wrong physical address", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).register_stride = 8;
    passed &= expect_u64("wrong stride", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).register_width = 8;
    passed &= expect_u64("wrong width", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));
    fixture = make_fixture();
    console(fixture).reserved_1[2] = 1;
    passed &= expect_u64("nonzero reserved field", select(fixture),
                         static_cast<uint32_t>(console_selection_error_t::unsupported_console));

    if (!passed) return 1;
    std::puts("Warren QEMU-virt console tests passed.");
    return 0;
}
