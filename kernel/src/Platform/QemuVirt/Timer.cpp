//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Platform/QemuVirt/Timer.h>

#include <burrow/Arch/AArch64/GenericTimer.h>
#include <burrow/Core/TransitionPlan.h>
#include <burrow/Drivers/Pl011.h>
#include <burrow/Platform/QemuVirt/Console.h>

#include <stdint.h>

namespace
{
    constexpr uint32_t k_gic_distributor_control{ 0x0000 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_distributor_enable_group1{ 0x12 };
    constexpr uint32_t k_gic_register_write_pending{ UINT32_C(1) << 31 };

    constexpr uint32_t k_gic_redistributor_waker{ 0x0014 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_redistributor_processor_sleep{ UINT32_C(1) << 1 };
    constexpr uint32_t k_gic_redistributor_children_asleep{ UINT32_C(1) << 2 };
    constexpr uint32_t k_gic_sgi_frame{ 0x10000 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_group0{ 0x0080 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_enable0{ 0x0100 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_disable0{ 0x0180 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_clear_pending0{ 0x0280 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_priority0{ 0x0400 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_configuration1{ 0x0c04 / sizeof(uint32_t) };
    constexpr uint32_t k_gic_group_modifier0{ 0x0d00 / sizeof(uint32_t) };
    constexpr uint32_t k_timer_interrupt_bit{
        UINT32_C(1) << burrow::arch::aarch64::k_physical_timer_interrupt_id
    };
    constexpr uint32_t k_timer_priority_word{
        k_gic_priority0 + burrow::arch::aarch64::k_physical_timer_interrupt_id / 4
    };
    constexpr uint32_t k_timer_priority_shift{
        (burrow::arch::aarch64::k_physical_timer_interrupt_id % 4) * 8
    };
    constexpr uint32_t k_timer_configuration_shift{
        (burrow::arch::aarch64::k_physical_timer_interrupt_id - 16) * 2
    };
    constexpr uint32_t k_spurious_interrupt_minimum{ 1020 };
    constexpr uint32_t k_interrupt_id_mask{ 0x00ffffff };

    extern "C" uint32_t burrow_aarch64_gicv3_initialize_cpu_interface() noexcept;
    extern "C" uint32_t burrow_aarch64_gicv3_acknowledge() noexcept;
    extern "C" void burrow_aarch64_gicv3_end_interrupt(uint32_t interrupt) noexcept;

    [[nodiscard]] volatile uint32_t* distributor() noexcept
    {
        return reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
            burrow::core::k_reference_gic_distributor_virtual_address));
    }

    [[nodiscard]] volatile uint32_t* redistributor() noexcept
    {
        return reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
            burrow::core::k_reference_gic_redistributor_virtual_address));
    }

    void synchronize_device_writes() noexcept
    {
        __asm__ volatile("dsb sy" ::: "memory");
    }

    [[nodiscard]] bool wait_for_clear(
        volatile uint32_t& value,
        uint32_t mask) noexcept
    {
        for (uint32_t poll{ 0 };
             poll < burrow::platform::qemu_virt::k_gic_poll_limit;
             ++poll)
        {
            if ((value & mask) == 0) return true;
        }
        return false;
    }
}

extern "C" [[gnu::visibility("hidden")]] [[gnu::used]] uint64_t
    burrow_qemu_virt_timer_ticks{};

namespace
{
    uint64_t burrow_qemu_virt_timer_frequency{};
}

extern "C" [[gnu::visibility("hidden")]] uint32_t
    burrow_qemu_virt_start_timer() noexcept
{
    using burrow::platform::qemu_virt::timer_start_error_t;

    burrow_qemu_virt_timer_ticks = 0;
    burrow_qemu_virt_timer_frequency = burrow_aarch64_read_counter_frequency();
    uint32_t interval{};
    const burrow::arch::aarch64::generic_timer_error_t interval_result{
        burrow::arch::aarch64::calculate_timer_interval(
            burrow_qemu_virt_timer_frequency,
            burrow::arch::aarch64::k_reference_timer_rate_hz,
            interval)
    };
    if (interval_result ==
        burrow::arch::aarch64::generic_timer_error_t::invalid_frequency)
        return static_cast<uint32_t>(timer_start_error_t::invalid_counter_frequency);
    if (interval_result != burrow::arch::aarch64::generic_timer_error_t::success)
        return static_cast<uint32_t>(timer_start_error_t::invalid_interval);

    volatile uint32_t* const gicd{ distributor() };
    gicd[k_gic_distributor_control] |= k_gic_distributor_enable_group1;
    synchronize_device_writes();
    if (!wait_for_clear(gicd[k_gic_distributor_control],
                        k_gic_register_write_pending))
        return static_cast<uint32_t>(timer_start_error_t::distributor_timeout);

    volatile uint32_t* const gicr{ redistributor() };
    gicr[k_gic_redistributor_waker] &= ~k_gic_redistributor_processor_sleep;
    synchronize_device_writes();
    if (!wait_for_clear(gicr[k_gic_redistributor_waker],
                        k_gic_redistributor_children_asleep))
        return static_cast<uint32_t>(timer_start_error_t::redistributor_timeout);

    volatile uint32_t* const sgi{ gicr + k_gic_sgi_frame };
    sgi[k_gic_disable0] = k_timer_interrupt_bit;
    sgi[k_gic_group0] |= k_timer_interrupt_bit;
    sgi[k_gic_group_modifier0] &= ~k_timer_interrupt_bit;
    uint32_t priority{ sgi[k_timer_priority_word] };
    priority &= ~(UINT32_C(0xff) << k_timer_priority_shift);
    priority |= UINT32_C(0x80) << k_timer_priority_shift;
    sgi[k_timer_priority_word] = priority;
    sgi[k_gic_configuration1] &= ~(UINT32_C(3) << k_timer_configuration_shift);
    sgi[k_gic_clear_pending0] = k_timer_interrupt_bit;
    sgi[k_gic_enable0] = k_timer_interrupt_bit;
    synchronize_device_writes();

    if (burrow_aarch64_gicv3_initialize_cpu_interface() != 0)
        return static_cast<uint32_t>(timer_start_error_t::cpu_interface_unavailable);

    burrow_aarch64_arm_physical_timer(interval);
    return static_cast<uint32_t>(timer_start_error_t::success);
}

extern "C" [[gnu::visibility("hidden")]] uint32_t burrow_aarch64_dispatch_irq(
    const burrow::arch::aarch64::exception_frame_t* frame) noexcept
{
    if (frame == nullptr || frame->abi_major !=
            burrow::arch::aarch64::k_exception_frame_abi_major ||
        frame->structure_size != burrow::arch::aarch64::k_exception_frame_size ||
        frame->vector != 5 || frame->current_exception_level != 1)
        return 0;

    const uint32_t acknowledged{ burrow_aarch64_gicv3_acknowledge() };
    const uint32_t interrupt{ acknowledged & k_interrupt_id_mask };
    if (interrupt >= k_spurious_interrupt_minimum) return 0;

    bool handled{ false };
    if (interrupt == burrow::arch::aarch64::k_physical_timer_interrupt_id)
    {
        burrow_aarch64_disable_physical_timer();
        volatile uint32_t* const sgi{ redistributor() + k_gic_sgi_frame };
        sgi[k_gic_disable0] = k_timer_interrupt_bit;
        synchronize_device_writes();
        ++burrow_qemu_virt_timer_ticks;
        handled = true;
    }

    burrow_aarch64_gicv3_end_interrupt(acknowledged);
    return handled ? 1U : 0U;
}

extern "C" [[gnu::visibility("hidden")]] uint32_t
    burrow_qemu_virt_publish_timer_tick() noexcept
{
    burrow::drivers::pl011_device_t device{};
    if (burrow::drivers::initialize_pl011(
            reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(
                burrow::platform::qemu_virt::k_pl011_virtual_address)),
            burrow::drivers::k_pl011_default_poll_limit,
            device) != burrow::drivers::pl011_error_t::success)
        return 1;
    const burrow::drivers::console_writer_t console{
        burrow::drivers::make_pl011_writer(device)
    };
    return burrow::arch::aarch64::report_timer_tick(
               console,
               burrow_qemu_virt_timer_frequency,
               burrow_qemu_virt_timer_ticks) ==
            burrow::arch::aarch64::generic_timer_error_t::success ? 0U : 1U;
}
