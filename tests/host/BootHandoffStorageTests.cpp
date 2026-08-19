//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootHandoffStorage.h>
#include <warren/boot/BootInformationProducer.h>
#include <warren/boot/BootInformationValidation.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::allocate_boot_handoff_storage;
    using warren::boot::boot_handoff_page_allocation_t;
    using warren::boot::boot_handoff_page_allocator_t;
    using warren::boot::boot_handoff_storage_error_t;
    using warren::boot::boot_handoff_storage_plan_t;
    using warren::boot::boot_handoff_storage_t;
    using warren::boot::boot_information_producer_error_t;
    using warren::boot::boot_information_producer_input_t;
    using warren::boot::boot_information_producer_output_t;
    using warren::boot::boot_information_source_descriptor_t;
    using warren::boot::boot_information_error_t;
    using warren::boot::k_bootstrap_stack_page_count;
    using warren::boot::plan_boot_handoff_storage;
    using warren::boot::produce_boot_information;
    using warren::boot::release_boot_handoff_storage;
    using warren::boot::resize_boot_handoff_storage;
    using warren::boot::uefi_memory_type_t;
    using warren::boot::validate_boot_information;

    constexpr uint32_t k_slot_count{ 9 };
    constexpr uint32_t k_slot_size{ 65536 };
    constexpr uint64_t k_fake_error_base{ 0x8000000000000100 };

    struct fake_allocator_t
    {
        alignas(4096) uint8_t buffers[k_slot_count][k_slot_size]{};
        uint64_t physical_starts[k_slot_count]{};
        uint64_t page_counts[k_slot_count]{};
        bool active[k_slot_count]{};
        uint32_t freed_slots[k_slot_count]{};
        uint32_t allocate_calls{ 0 };
        uint32_t free_calls{ 0 };
        uint32_t fail_allocate_call{ UINT32_MAX };
        uint32_t fail_free_call{ UINT32_MAX };
        uint32_t invalid_allocation_call{ UINT32_MAX };
        uint32_t overlapping_allocation_call{ UINT32_MAX };
    };

    void prepare_allocator(fake_allocator_t& allocator) noexcept
    {
        for (uint32_t slot{ 0 }; slot < k_slot_count; ++slot)
        {
            for (uint32_t index{ 0 }; index < k_slot_size; ++index)
                allocator.buffers[slot][index] = 0xa5;

            allocator.freed_slots[slot] = UINT32_MAX;
        }
    }

    uint64_t fake_allocate_pages(void* opaque_context, uint64_t page_count,
                                 boot_handoff_page_allocation_t& allocation) noexcept
    {
        auto& context{ *static_cast<fake_allocator_t*>(opaque_context) };
        const uint32_t call{ context.allocate_calls++ };

        if (call == context.fail_allocate_call)
            return k_fake_error_base + call;

        if (call >= k_slot_count || page_count == 0 || page_count > k_slot_size / 4096)
            return k_fake_error_base + 0xf0;

        uint64_t physical_start{ 0x00100000 + static_cast<uint64_t>(call) * 0x00100000 };

        if (call == context.invalid_allocation_call)
            ++physical_start;
        else if (call == context.overlapping_allocation_call)
            physical_start = context.physical_starts[0];

        context.physical_starts[call] = physical_start;
        context.page_counts[call] = page_count;
        context.active[call] = true;
        allocation = { physical_start, context.buffers[call] };
        return 0;
    }

    uint64_t fake_free_pages(void* opaque_context, uint64_t physical_start, uint64_t page_count) noexcept
    {
        auto& context{ *static_cast<fake_allocator_t*>(opaque_context) };
        const uint32_t call{ context.free_calls++ };

        if (call == context.fail_free_call)
            return k_fake_error_base + 0x40 + call;

        for (uint32_t slot{ 0 }; slot < k_slot_count; ++slot)
        {
            if (!context.active[slot] || context.physical_starts[slot] != physical_start ||
                context.page_counts[slot] != page_count)
                continue;

            context.active[slot] = false;
            context.freed_slots[call] = slot;
            return 0;
        }

        return k_fake_error_base + 0xfe;
    }

    [[nodiscard]] boot_handoff_page_allocator_t make_page_allocator(fake_allocator_t& context) noexcept
    {
        return { &context, fake_allocate_pages, fake_free_pages };
    }

    bool expect_error(const char* name, boot_handoff_storage_error_t actual,
                      boot_handoff_storage_error_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected result %u, received %u\n", name,
                     static_cast<uint32_t>(expected), static_cast<uint32_t>(actual));
        return false;
    }

    bool expect_u32(const char* name, uint32_t actual, uint32_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected %u, received %u\n", name, expected, actual);
        return false;
    }

    bool expect_u64(const char* name, uint64_t actual, uint64_t expected) noexcept
    {
        if (actual == expected) return true;

        std::fprintf(stderr, "FAIL: %s: expected 0x%016llx, received 0x%016llx\n", name,
                     static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
        return false;
    }

    bool expect_true(const char* name, bool value) noexcept
    {
        if (value) return true;

        std::fprintf(stderr, "FAIL: %s\n", name);
        return false;
    }

    bool allocation_is_zero(const uint8_t* bytes, uint64_t byte_count) noexcept
    {
        for (uint64_t index{ 0 }; index < byte_count; ++index)
        {
            if (bytes[index] != 0) return false;
        }

        return true;
    }

    [[nodiscard]] warren_boot_early_console_t make_console() noexcept
    {
        warren_boot_early_console_t console{};
        console.kind = WARREN_BOOT_CONSOLE_PL011;
        console.flags = WARREN_BOOT_CONSOLE_OUTPUT;
        console.physical_address = 0x09000000;
        console.register_stride = 4;
        console.register_width = 32;
        return console;
    }

    [[nodiscard]] boot_handoff_storage_plan_t make_plan(bool console_present = true) noexcept
    {
        boot_handoff_storage_plan_t plan{};
        static_cast<void>(plan_boot_handoff_storage(4000, 40, console_present, plan));
        return plan;
    }

    bool run_planning_tests() noexcept
    {
        bool passed{ true };
        boot_handoff_storage_plan_t plan{};
        passed &= expect_error("reference plan", plan_boot_handoff_storage(4000, 40, true, plan),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u64("reference map pages", plan.memory_map_page_count, 2);
        passed &= expect_u64("reference map capacity", plan.memory_map_capacity, 8192);
        passed &= expect_u64("reference descriptor size", plan.memory_descriptor_size, 40);
        passed &= expect_u32("reference source capacity", plan.source_descriptor_capacity, 204);
        passed &= expect_u64("reference source pages", plan.source_page_count, 2);
        passed &= expect_u64("reference source allocation", plan.source_capacity, 8192);
        passed &= expect_u64("reference work pages", plan.work_page_count, 3);
        passed &= expect_u64("reference work capacity", plan.work_capacity, 12288);
        passed &= expect_u32("reference work entries", plan.work_entry_capacity, 210);
        passed &= expect_u64("reference object pages", plan.object_page_count, 3);
        passed &= expect_u64("reference object allocation", plan.object_allocation_size, 12288);
        passed &= expect_u32("reference object required", plan.object_required_capacity, 8720);

        passed &= expect_error("no-console plan", plan_boot_handoff_storage(4000, 40, false, plan),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u32("no-console object required", plan.object_required_capacity, 8656);

        passed &= expect_error("extended descriptor plan", plan_boot_handoff_storage(4800, 48, true, plan),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u64("extended map pages", plan.memory_map_page_count, 2);
        passed &= expect_u64("extended descriptor size", plan.memory_descriptor_size, 48);
        passed &= expect_u32("extended source capacity", plan.source_descriptor_capacity, 170);
        passed &= expect_u32("extended work entries", plan.work_entry_capacity, 176);
        passed &= expect_u32("extended object required", plan.object_required_capacity, 7360);

        passed &= expect_error("zero descriptor size", plan_boot_handoff_storage(4000, 0, true, plan),
                               boot_handoff_storage_error_t::invalid_descriptor_size);
        passed &= expect_error("short descriptor size", plan_boot_handoff_storage(4000, 32, true, plan),
                               boot_handoff_storage_error_t::invalid_descriptor_size);
        passed &= expect_error("misaligned descriptor size", plan_boot_handoff_storage(4200, 42, true, plan),
                               boot_handoff_storage_error_t::invalid_descriptor_size);
        passed &= expect_error("zero map estimate", plan_boot_handoff_storage(0, 40, true, plan),
                               boot_handoff_storage_error_t::invalid_memory_map_size);
        passed &= expect_error("partial descriptor map", plan_boot_handoff_storage(4097, 40, true, plan),
                               boot_handoff_storage_error_t::invalid_memory_map_size);
        passed &= expect_error("map growth overflow",
                               plan_boot_handoff_storage(UINT64_MAX - 15, 40, true, plan),
                               boot_handoff_storage_error_t::arithmetic_overflow);
        constexpr uint64_t excessive_descriptor_count{ static_cast<uint64_t>(UINT32_MAX) + 1 };
        passed &= expect_error("descriptor capacity overflow",
                               plan_boot_handoff_storage(excessive_descriptor_count * 40, 40, true, plan),
                               boot_handoff_storage_error_t::capacity_overflow);
        return passed;
    }

    bool run_allocation_and_release_test() noexcept
    {
        fake_allocator_t context{};
        prepare_allocator(context);
        const boot_handoff_page_allocator_t allocator{ make_page_allocator(context) };
        const boot_handoff_storage_plan_t plan{ make_plan() };
        boot_handoff_storage_t storage{};
        uint64_t platform_status{ UINT64_MAX };
        bool passed{ expect_error("storage allocation",
                                  allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                                  boot_handoff_storage_error_t::success) };
        passed &= expect_u64("allocation platform status", platform_status, 0);
        passed &= expect_u32("allocation call count", context.allocate_calls, 5);
        passed &= expect_u64("bootstrap stack pages", storage.bootstrap_stack.page_count,
                             k_bootstrap_stack_page_count);
        passed &= expect_u64("map pages", storage.memory_map.page_count, plan.memory_map_page_count);
        passed &= expect_u64("source pages", storage.source_descriptors.page_count, plan.source_page_count);
        passed &= expect_u64("work pages", storage.work_entries.page_count, plan.work_page_count);
        passed &= expect_u64("object pages", storage.object.page_count, plan.object_page_count);
        passed &= expect_true("bootstrap stack zeroed",
                              allocation_is_zero(storage.bootstrap_stack.writable_start,
                                                 storage.bootstrap_stack.page_count * 4096));
        passed &= expect_true("map storage zeroed",
                              allocation_is_zero(storage.memory_map.writable_start,
                                                 storage.memory_map.page_count * 4096));
        passed &= expect_true("source storage zeroed",
                              allocation_is_zero(storage.source_descriptors.writable_start,
                                                 storage.source_descriptors.page_count * 4096));
        passed &= expect_true("work storage zeroed",
                              allocation_is_zero(storage.work_entries.writable_start,
                                                 storage.work_entries.page_count * 4096));
        passed &= expect_true("object storage zeroed",
                              allocation_is_zero(storage.object.writable_start,
                                                 storage.object.page_count * 4096));
        passed &= expect_u64("checked stack top",
                             storage.bootstrap_stack.physical_start +
                                 storage.bootstrap_stack.page_count * 4096,
                             0x00110000);

        const boot_information_source_descriptor_t descriptors[]{
            { storage.bootstrap_stack.physical_start, storage.bootstrap_stack.page_count,
              static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
            { storage.memory_map.physical_start, storage.memory_map.page_count,
              static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
            { storage.source_descriptors.physical_start, storage.source_descriptors.page_count,
              static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
            { storage.work_entries.physical_start, storage.work_entries.page_count,
              static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
            { storage.object.physical_start, storage.object.page_count,
              static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
            { 0x00600000, 2, static_cast<uint32_t>(uefi_memory_type_t::loader_data), 8 },
        };
        const warren_boot_early_console_t console{ make_console() };
        const boot_information_producer_input_t producer_input{
            descriptors,
            6,
            storage.object.physical_start,
            storage.object.page_count * 4096,
            0x00600000,
            0x2000,
            0x100000,
            0x00600100,
            storage.bootstrap_stack.physical_start,
            storage.bootstrap_stack.page_count * 4096,
            &console,
        };
        boot_information_producer_output_t producer_output{};
        const boot_information_producer_error_t producer_result{
            produce_boot_information(
                producer_input,
                storage.object.writable_start,
                plan.object_required_capacity,
                reinterpret_cast<warren_boot_memory_entry_t*>(storage.work_entries.writable_start),
                plan.work_entry_capacity,
                producer_output)
        };
        passed &= expect_u32("allocated object production", static_cast<uint32_t>(producer_result),
                             static_cast<uint32_t>(boot_information_producer_error_t::success));
        passed &= expect_true("produced object fits planned capacity",
                              producer_output.total_size <= plan.object_required_capacity);
        passed &= expect_u32("allocated object consumer validation",
                             static_cast<uint32_t>(validate_boot_information(
                                 storage.object.writable_start,
                                 producer_output.total_size,
                                 storage.object.physical_start)),
                             static_cast<uint32_t>(boot_information_error_t::success));

        passed &= expect_error("storage release",
                               release_boot_handoff_storage(allocator, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u64("release platform status", platform_status, 0);
        passed &= expect_u32("release call count", context.free_calls, 5);
        passed &= expect_u32("object released first", context.freed_slots[0], 4);
        passed &= expect_u32("work released second", context.freed_slots[1], 3);
        passed &= expect_u32("source released third", context.freed_slots[2], 2);
        passed &= expect_u32("map released fourth", context.freed_slots[3], 1);
        passed &= expect_u32("stack released last", context.freed_slots[4], 0);
        passed &= expect_u64("released storage cleared", storage.bootstrap_stack.physical_start, 0);
        return passed;
    }

    bool run_allocation_failure_tests() noexcept
    {
        bool passed{ true };
        const boot_handoff_storage_plan_t plan{ make_plan() };

        for (uint32_t failure_call{ 0 }; failure_call < 5; ++failure_call)
        {
            fake_allocator_t context{};
            prepare_allocator(context);
            context.fail_allocate_call = failure_call;
            const boot_handoff_page_allocator_t allocator{ make_page_allocator(context) };
            boot_handoff_storage_t storage{};
            uint64_t platform_status{ 0 };
            passed &= expect_error("allocation failure",
                                   allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                                   boot_handoff_storage_error_t::allocation_failed);
            passed &= expect_u64("allocation failure status", platform_status,
                                 k_fake_error_base + failure_call);
            passed &= expect_u32("allocation failure unwind count", context.free_calls, failure_call);
            passed &= expect_u64("allocation failure clears storage", storage.bootstrap_stack.physical_start, 0);
        }

        fake_allocator_t context{};
        prepare_allocator(context);
        context.invalid_allocation_call = 2;
        boot_handoff_page_allocator_t allocator{ make_page_allocator(context) };
        boot_handoff_storage_t storage{};
        uint64_t platform_status{ 0 };
        passed &= expect_error("invalid allocator result",
                               allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::invalid_allocation);
        passed &= expect_u32("invalid result unwind count", context.free_calls, 3);

        context = {};
        prepare_allocator(context);
        context.overlapping_allocation_call = 2;
        allocator = make_page_allocator(context);
        passed &= expect_error("overlapping allocator result",
                               allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::overlapping_allocations);
        passed &= expect_u32("overlap unwind count", context.free_calls, 3);

        boot_handoff_storage_plan_t invalid_plan{ plan };
        invalid_plan.work_capacity = 4096;
        passed &= expect_error("invalid plan",
                               allocate_boot_handoff_storage(invalid_plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::invalid_plan);
        const boot_handoff_page_allocator_t invalid_allocator{ nullptr, nullptr, nullptr };
        passed &= expect_error("invalid allocator",
                               allocate_boot_handoff_storage(plan, invalid_allocator, storage, platform_status),
                               boot_handoff_storage_error_t::invalid_allocator);
        storage.bootstrap_stack = { 0x1000, 1, context.buffers[0] };
        passed &= expect_error("nonempty destination storage",
                               allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::storage_not_empty);
        passed &= expect_u64("nonempty destination retained", storage.bootstrap_stack.physical_start, 0x1000);
        return passed;
    }

    bool run_resize_tests() noexcept
    {
        bool passed{ true };
        fake_allocator_t context{};
        prepare_allocator(context);
        const boot_handoff_page_allocator_t allocator{ make_page_allocator(context) };
        const boot_handoff_storage_plan_t initial_plan{ make_plan() };
        boot_handoff_storage_plan_t replacement_plan{};
        static_cast<void>(plan_boot_handoff_storage(16000, 40, true, replacement_plan));
        boot_handoff_storage_t storage{};
        uint64_t platform_status{ 0 };
        passed &= expect_error("resize initial allocation",
                               allocate_boot_handoff_storage(
                                   initial_plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        const uint64_t stack_start{ storage.bootstrap_stack.physical_start };
        const uint8_t* stack_bytes{ storage.bootstrap_stack.writable_start };
        passed &= expect_error("pre-exit resize",
                               resize_boot_handoff_storage(
                                   replacement_plan, allocator, false, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u32("pre-exit replacement allocations", context.allocate_calls, 9);
        passed &= expect_u32("pre-exit superseded frees", context.free_calls, 4);
        passed &= expect_u64("resize reuses stack address", storage.bootstrap_stack.physical_start,
                             stack_start);
        passed &= expect_true("resize reuses stack bytes",
                              storage.bootstrap_stack.writable_start == stack_bytes);
        passed &= expect_u64("resize map pages", storage.memory_map.page_count,
                             replacement_plan.memory_map_page_count);
        passed &= expect_u32("old object released first", context.freed_slots[0], 4);
        passed &= expect_u32("old map released last", context.freed_slots[3], 1);
        passed &= expect_error("release resized storage",
                               release_boot_handoff_storage(allocator, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u32("all current allocations released", context.free_calls, 9);

        context = {};
        prepare_allocator(context);
        storage = {};
        passed &= expect_error("restricted initial allocation",
                               allocate_boot_handoff_storage(
                                   initial_plan, allocator, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_error("restricted resize",
                               resize_boot_handoff_storage(
                                   replacement_plan, allocator, true, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u32("restricted resize does not free", context.free_calls, 0);
        passed &= expect_true("restricted old map retained", context.active[1]);
        passed &= expect_true("restricted old object retained", context.active[4]);
        passed &= expect_u64("restricted resize reuses stack", storage.bootstrap_stack.physical_start,
                             context.physical_starts[0]);
        return passed;
    }

    bool run_cleanup_failure_test() noexcept
    {
        fake_allocator_t context{};
        prepare_allocator(context);
        context.fail_allocate_call = 4;
        context.fail_free_call = 0;
        const boot_handoff_page_allocator_t allocator{ make_page_allocator(context) };
        const boot_handoff_storage_plan_t plan{ make_plan() };
        boot_handoff_storage_t storage{};
        uint64_t platform_status{ 0 };
        bool passed{ expect_error("allocation unwind cleanup failure",
                                  allocate_boot_handoff_storage(plan, allocator, storage, platform_status),
                                  boot_handoff_storage_error_t::cleanup_failed) };
        passed &= expect_u64("cleanup failure status", platform_status, k_fake_error_base + 0x40);
        passed &= expect_u64("failed cleanup extent retained", storage.work_entries.physical_start,
                             context.physical_starts[3]);
        passed &= expect_u64("successfully freed source cleared", storage.source_descriptors.physical_start, 0);
        passed &= expect_u64("successfully freed map cleared", storage.memory_map.physical_start, 0);
        passed &= expect_u64("successfully freed stack cleared", storage.bootstrap_stack.physical_start, 0);

        context.fail_free_call = UINT32_MAX;
        passed &= expect_error("cleanup retry",
                               release_boot_handoff_storage(allocator, storage, platform_status),
                               boot_handoff_storage_error_t::success);
        passed &= expect_u64("cleanup retry clears retained extent", storage.work_entries.physical_start, 0);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_planning_tests();
    passed &= run_allocation_and_release_test();
    passed &= run_allocation_failure_tests();
    passed &= run_resize_tests();
    passed &= run_cleanup_failure_test();

    if (!passed) return 1;

    std::puts("Warren boot-handoff storage tests passed.");
    return 0;
}
