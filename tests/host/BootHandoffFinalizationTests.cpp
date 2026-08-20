//
// Created by Zack Shrout on 8/18/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootHandoffFinalization.h>

#include <cstdint>
#include <cstdio>

namespace
{
    using warren::boot::boot_handoff_finalization_error_t;
    using warren::boot::boot_handoff_finalization_result_t;
    using warren::boot::boot_handoff_firmware_operations_t;
    using warren::boot::boot_handoff_firmware_result_t;
    using warren::boot::boot_handoff_memory_map_snapshot_t;
    using warren::boot::boot_handoff_storage_plan_t;
    using warren::boot::boot_handoff_storage_t;
    using warren::boot::boot_information_error_t;
    using warren::boot::burrow_loaded_image_t;
    using warren::boot::finalize_boot_handoff;
    using warren::boot::k_boot_services_exit_attempt_limit;
    using warren::boot::k_handoff_storage_resize_attempt_limit;
    using warren::boot::plan_boot_handoff_storage;
    using warren::boot::uefi_memory_type_t;
    using warren::boot::validate_boot_information;

    constexpr uint32_t k_maximum_calls{ 16 };
    constexpr uint64_t k_map_failure_status{ 0x8000000000000201 };
    constexpr uint64_t k_stale_key_status{ 0x8000000000000202 };
    constexpr uint64_t k_exit_failure_status{ 0x8000000000000203 };

    struct storage_fixture_t
    {
        alignas(4096) uint8_t stack[65536]{};
        alignas(4096) uint8_t memory_map[32768]{};
        alignas(4096) uint8_t source_descriptors[16384]{};
        alignas(4096) uint8_t work_entries[32768]{};
        alignas(4096) uint8_t object[32768]{};
        boot_handoff_storage_t storage{};
    };

    struct firmware_fixture_t
    {
        boot_handoff_firmware_result_t map_results[k_maximum_calls]{};
        boot_handoff_firmware_result_t exit_results[k_maximum_calls]{};
        boot_handoff_firmware_result_t resize_results[k_maximum_calls]{};
        uint64_t exit_keys[k_maximum_calls]{};
        char sequence[k_maximum_calls * 2]{};
        uint32_t map_result_count{ 0 };
        uint32_t exit_result_count{ 0 };
        uint32_t resize_result_count{ 0 };
        uint32_t map_calls{ 0 };
        uint32_t exit_calls{ 0 };
        uint32_t resize_calls{ 0 };
        uint32_t sequence_count{ 0 };
        uint64_t required_map_size{ 16000 };
        uint64_t snapshot_map_size{ 0 };
        uint64_t snapshot_descriptor_size{ 40 };
        uint32_t snapshot_descriptor_version{ 1 };
        bool zero_page_descriptor{ false };
        bool misaligned_descriptor{ false };
        bool omit_kernel_descriptor{ false };
        bool unsorted_map{ false };
        storage_fixture_t* storage_fixture{ nullptr };
        bool last_resize_was_restricted{ false };
    };

    void write_u32(uint8_t* bytes, uint64_t offset, uint32_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 4; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8U));
    }

    void write_u64(uint8_t* bytes, uint64_t offset, uint64_t value) noexcept
    {
        for (uint32_t index{ 0 }; index < 8; ++index)
            bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8U));
    }

    void write_descriptor(uint8_t* bytes, uint64_t stride, uint32_t index, uint32_t type,
                          uint64_t physical_start, uint64_t page_count, uint64_t attributes) noexcept
    {
        const uint64_t offset{ static_cast<uint64_t>(index) * stride };
        write_u32(bytes, offset + 0x00, type);
        write_u64(bytes, offset + 0x08, physical_start);
        write_u64(bytes, offset + 0x10, 0);
        write_u64(bytes, offset + 0x18, page_count);
        write_u64(bytes, offset + 0x20, attributes);
    }

    void write_reference_map(uint8_t* memory_map, uint64_t stride, uint64_t attributes,
                             bool zero_page_descriptor, bool misaligned_descriptor,
                             bool omit_kernel_descriptor) noexcept
    {
        constexpr uint32_t loader_data{ static_cast<uint32_t>(uefi_memory_type_t::loader_data) };
        write_descriptor(memory_map, stride, 0, loader_data,
                         misaligned_descriptor ? 0x00100001 : 0x00100000,
                         zero_page_descriptor ? 0 : 16, attributes);
        write_descriptor(memory_map, stride, 1, loader_data, 0x00200000, 2, attributes);
        write_descriptor(memory_map, stride, 2, loader_data, 0x00300000, 2, attributes);
        write_descriptor(memory_map, stride, 3, loader_data, 0x00400000, 3, attributes);
        write_descriptor(memory_map, stride, 4, loader_data, 0x00500000, 3, attributes);

        if (!omit_kernel_descriptor)
            write_descriptor(memory_map, stride, 5, loader_data, 0x00600000, 2, attributes);
    }

    boot_handoff_firmware_result_t get_memory_map(
        void* opaque_context,
        uint8_t* memory_map,
        uint64_t memory_map_capacity,
        boot_handoff_memory_map_snapshot_t& snapshot,
        uint64_t& platform_status) noexcept
    {
        auto& context{ *static_cast<firmware_fixture_t*>(opaque_context) };
        context.sequence[context.sequence_count++] = 'G';
        const uint32_t call{ context.map_calls++ };
        const boot_handoff_firmware_result_t configured_result{
            call < context.map_result_count ? context.map_results[call]
                                            : boot_handoff_firmware_result_t::success
        };

        if (configured_result == boot_handoff_firmware_result_t::buffer_too_small)
        {
            snapshot = {
                context.required_map_size,
                0,
                context.snapshot_descriptor_size,
                context.snapshot_descriptor_version,
            };
            platform_status = 5;
            return configured_result;
        }

        if (configured_result != boot_handoff_firmware_result_t::success)
        {
            platform_status = k_map_failure_status;
            return configured_result;
        }

        const uint32_t descriptor_count{ context.omit_kernel_descriptor ? 5U : 6U };
        const uint64_t natural_map_size{ context.snapshot_descriptor_size * descriptor_count };
        snapshot = {
            context.snapshot_map_size == 0 ? natural_map_size : context.snapshot_map_size,
            0x1000 + call,
            context.snapshot_descriptor_size,
            context.snapshot_descriptor_version,
        };
        platform_status = 0;

        if (memory_map != nullptr && natural_map_size <= memory_map_capacity)
        {
            write_reference_map(memory_map, context.snapshot_descriptor_size, 0x80 + call,
                                context.zero_page_descriptor, context.misaligned_descriptor,
                                context.omit_kernel_descriptor);

            if (context.storage_fixture != nullptr)
            {
                const boot_handoff_storage_t& storage{ context.storage_fixture->storage };
                write_u64(memory_map, context.snapshot_descriptor_size * 1 + 0x18,
                          storage.memory_map.page_count);
                write_u64(memory_map, context.snapshot_descriptor_size * 2 + 0x18,
                          storage.source_descriptors.page_count);
                write_u64(memory_map, context.snapshot_descriptor_size * 3 + 0x18,
                          storage.work_entries.page_count);
                write_u64(memory_map, context.snapshot_descriptor_size * 4 + 0x18,
                          storage.object.page_count);
            }

            if (context.unsorted_map && !context.omit_kernel_descriptor)
            {
                for (uint64_t byte{ 0 }; byte < context.snapshot_descriptor_size; ++byte)
                {
                    const uint64_t last{ context.snapshot_descriptor_size * 5 + byte };
                    const uint8_t temporary{ memory_map[byte] };
                    memory_map[byte] = memory_map[last];
                    memory_map[last] = temporary;
                }
            }
        }

        return configured_result;
    }

    boot_handoff_firmware_result_t exit_boot_services(void* opaque_context, uint64_t map_key,
                                                      uint64_t& platform_status) noexcept
    {
        auto& context{ *static_cast<firmware_fixture_t*>(opaque_context) };
        context.sequence[context.sequence_count++] = 'E';
        const uint32_t call{ context.exit_calls++ };
        context.exit_keys[call] = map_key;
        const boot_handoff_firmware_result_t configured_result{
            call < context.exit_result_count ? context.exit_results[call]
                                             : boot_handoff_firmware_result_t::success
        };

        if (configured_result == boot_handoff_firmware_result_t::stale_map_key)
            platform_status = k_stale_key_status;
        else if (configured_result == boot_handoff_firmware_result_t::success)
            platform_status = 0;
        else
            platform_status = k_exit_failure_status;

        return configured_result;
    }

    boot_handoff_firmware_result_t resize_storage(
        void* opaque_context,
        uint64_t required_memory_map_size,
        uint64_t memory_descriptor_size,
        bool exit_attempted,
        boot_handoff_storage_t& storage,
        uint64_t& platform_status) noexcept
    {
        auto& context{ *static_cast<firmware_fixture_t*>(opaque_context) };
        context.sequence[context.sequence_count++] = 'R';
        const uint32_t call{ context.resize_calls++ };
        context.last_resize_was_restricted = exit_attempted;
        const boot_handoff_firmware_result_t configured_result{
            call < context.resize_result_count ? context.resize_results[call]
                                               : boot_handoff_firmware_result_t::success
        };

        if (configured_result != boot_handoff_firmware_result_t::success ||
            context.storage_fixture == nullptr)
        {
            platform_status = k_map_failure_status;
            return boot_handoff_firmware_result_t::failure;
        }

        boot_handoff_storage_plan_t replacement_plan{};
        if (plan_boot_handoff_storage(required_memory_map_size, memory_descriptor_size, true,
                                      replacement_plan) != warren::boot::boot_handoff_storage_error_t::success)
        {
            platform_status = k_map_failure_status;
            return boot_handoff_firmware_result_t::failure;
        }

        storage.plan = replacement_plan;
        storage.memory_map.page_count = replacement_plan.memory_map_page_count;
        storage.source_descriptors.page_count = replacement_plan.source_page_count;
        storage.work_entries.page_count = replacement_plan.work_page_count;
        storage.object.page_count = replacement_plan.object_page_count;
        platform_status = 0;
        return boot_handoff_firmware_result_t::success;
    }

    void initialize_storage(storage_fixture_t& fixture) noexcept
    {
        boot_handoff_storage_plan_t plan{};
        static_cast<void>(plan_boot_handoff_storage(4000, 40, true, plan));
        fixture.storage = {
            plan,
            { 0x00100000, 16, fixture.stack },
            { 0x00200000, plan.memory_map_page_count, fixture.memory_map },
            { 0x00300000, plan.source_page_count, fixture.source_descriptors },
            { 0x00400000, plan.work_page_count, fixture.work_entries },
            { 0x00500000, plan.object_page_count, fixture.object },
        };
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

    [[nodiscard]] burrow_loaded_image_t make_loaded_image() noexcept
    {
        return { 0x00600000, 0x2000, 0x100000, 0x00600100 };
    }

    [[nodiscard]] boot_handoff_firmware_operations_t make_firmware(firmware_fixture_t& fixture) noexcept
    {
        return { &fixture, get_memory_map, exit_boot_services, resize_storage };
    }

    bool expect_error(const char* name, boot_handoff_finalization_error_t actual,
                      boot_handoff_finalization_error_t expected) noexcept
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

    bool expect_sequence(const char* name, const firmware_fixture_t& firmware, const char* expected) noexcept
    {
        uint32_t expected_count{ 0 };
        while (expected[expected_count] != 0) ++expected_count;

        if (firmware.sequence_count != expected_count)
        {
            std::fprintf(stderr, "FAIL: %s: sequence length mismatch\n", name);
            return false;
        }

        for (uint32_t index{ 0 }; index < expected_count; ++index)
        {
            if (firmware.sequence[index] == expected[index]) continue;

            std::fprintf(stderr, "FAIL: %s: sequence differs at %u\n", name, index);
            return false;
        }

        return true;
    }

    boot_handoff_finalization_error_t run_finalization(
        storage_fixture_t& storage,
        firmware_fixture_t& firmware,
        boot_handoff_finalization_result_t& result) noexcept
    {
        const burrow_loaded_image_t image{ make_loaded_image() };
        const warren_boot_early_console_t console{ make_console() };
        const boot_handoff_firmware_operations_t operations{ make_firmware(firmware) };
        firmware.storage_fixture = &storage;
        return finalize_boot_handoff(image, console, storage.storage, operations, result);
    }

    bool run_immediate_success_test() noexcept
    {
        storage_fixture_t storage{};
        initialize_storage(storage);
        firmware_fixture_t firmware{};
        boot_handoff_finalization_result_t result{};
        bool passed{ expect_error("immediate success", run_finalization(storage, firmware, result),
                                  boot_handoff_finalization_error_t::success) };
        passed &= expect_sequence("immediate call sequence", firmware, "GE");
        passed &= expect_u32("immediate map captures", result.memory_map_capture_count, 1);
        passed &= expect_u32("immediate exit attempts", result.exit_attempt_count, 1);
        passed &= expect_true("immediate exit boundary recorded", result.exit_attempted);
        passed &= expect_u64("immediate map key", firmware.exit_keys[0], 0x1000);
        passed &= expect_u32("immediate descriptor count", result.source_descriptor_count, 6);
        passed &= expect_true("immediate object has bytes", result.object_size > 0);
        passed &= expect_u32("immediate independent validation",
                             static_cast<uint32_t>(validate_boot_information(
                                 storage.object, result.object_size, storage.storage.object.physical_start)),
                             static_cast<uint32_t>(boot_information_error_t::success));
        return passed;
    }

    bool run_unsorted_map_test() noexcept
    {
        storage_fixture_t storage{};
        initialize_storage(storage);
        firmware_fixture_t firmware{};
        firmware.unsorted_map = true;
        boot_handoff_finalization_result_t result{};
        bool passed{ expect_error("unsorted firmware map", run_finalization(storage, firmware, result),
                                  boot_handoff_finalization_error_t::success) };
        passed &= expect_sequence("unsorted map call sequence", firmware, "GE");
        passed &= expect_u32("unsorted map descriptor count", result.source_descriptor_count, 6);
        return passed;
    }

    bool run_stale_retry_test() noexcept
    {
        storage_fixture_t storage{};
        initialize_storage(storage);
        firmware_fixture_t firmware{};
        firmware.exit_result_count = 2;
        firmware.exit_results[0] = boot_handoff_firmware_result_t::stale_map_key;
        firmware.exit_results[1] = boot_handoff_firmware_result_t::success;
        boot_handoff_finalization_result_t result{};
        bool passed{ expect_error("stale retry", run_finalization(storage, firmware, result),
                                  boot_handoff_finalization_error_t::success) };
        passed &= expect_sequence("stale retry sequence", firmware, "GEGE");
        passed &= expect_u32("stale retry map captures", result.memory_map_capture_count, 2);
        passed &= expect_u32("stale retry exit attempts", result.exit_attempt_count, 2);
        passed &= expect_u64("first stale map key", firmware.exit_keys[0], 0x1000);
        passed &= expect_u64("second current map key", firmware.exit_keys[1], 0x1001);

        const auto* header{ reinterpret_cast<const warren_boot_information_t*>(storage.object) };
        const auto* entries{ reinterpret_cast<const warren_boot_memory_entry_t*>(
            storage.object + header->memory_map.offset) };
        passed &= expect_u64("object rebuilt from second snapshot", entries[0].source_attributes, 0x81);
        return passed;
    }

    bool run_resize_tests() noexcept
    {
        bool passed{ true };
        storage_fixture_t storage{};
        initialize_storage(storage);
        firmware_fixture_t firmware{};
        firmware.map_result_count = 1;
        firmware.map_results[0] = boot_handoff_firmware_result_t::buffer_too_small;
        boot_handoff_finalization_result_t result{};
        passed &= expect_error("pre-exit resize", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::success);
        passed &= expect_sequence("pre-exit resize sequence", firmware, "GRGE");
        passed &= expect_true("pre-exit resize reaches exit", result.exit_attempted);
        passed &= expect_true("pre-exit resize phase", !firmware.last_resize_was_restricted);
        passed &= expect_u32("pre-exit resize count", result.storage_resize_count, 1);
        passed &= expect_u64("pre-exit required size", result.required_memory_map_size,
                             firmware.required_map_size);

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.map_result_count = 2;
        firmware.map_results[0] = boot_handoff_firmware_result_t::success;
        firmware.map_results[1] = boot_handoff_firmware_result_t::buffer_too_small;
        firmware.exit_result_count = 1;
        firmware.exit_results[0] = boot_handoff_firmware_result_t::stale_map_key;
        result = {};
        passed &= expect_error("restricted-phase resize", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::success);
        passed &= expect_sequence("restricted-phase resize sequence", firmware, "GEGRGE");
        passed &= expect_true("restricted-phase resize remembers exit", result.exit_attempted);
        passed &= expect_true("restricted-phase resize phase", firmware.last_resize_was_restricted);
        passed &= expect_u32("restricted-phase resize exit count", result.exit_attempt_count, 2);

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.map_result_count = 1;
        firmware.map_results[0] = boot_handoff_firmware_result_t::buffer_too_small;
        firmware.resize_result_count = 1;
        firmware.resize_results[0] = boot_handoff_firmware_result_t::failure;
        result = {};
        passed &= expect_error("resize failure", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::storage_resize_failed);
        passed &= expect_sequence("resize failure sequence", firmware, "GR");

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.map_result_count = k_handoff_storage_resize_attempt_limit + 1;
        for (uint32_t index{ 0 }; index < firmware.map_result_count; ++index)
            firmware.map_results[index] = boot_handoff_firmware_result_t::buffer_too_small;
        result = {};
        passed &= expect_error("resize exhaustion", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::storage_resize_retry_exhausted);
        passed &= expect_u32("resize exhaustion count", result.storage_resize_count,
                             k_handoff_storage_resize_attempt_limit);
        passed &= expect_u32("resize exhaustion captures", result.memory_map_capture_count,
                             k_handoff_storage_resize_attempt_limit + 1);
        return passed;
    }

    bool run_failure_tests() noexcept
    {
        bool passed{ true };
        storage_fixture_t storage{};
        initialize_storage(storage);
        firmware_fixture_t firmware{};
        firmware.map_result_count = 1;
        firmware.map_results[0] = boot_handoff_firmware_result_t::failure;
        boot_handoff_finalization_result_t result{};
        passed &= expect_error("map failure", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::memory_map_failed);
        passed &= expect_u64("map failure status", result.platform_status, k_map_failure_status);
        passed &= expect_true("map failure before exit", !result.exit_attempted);

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.map_result_count = 2;
        firmware.map_results[0] = boot_handoff_firmware_result_t::success;
        firmware.map_results[1] = boot_handoff_firmware_result_t::failure;
        firmware.exit_result_count = 1;
        firmware.exit_results[0] = boot_handoff_firmware_result_t::stale_map_key;
        result = {};
        passed &= expect_error("map failure after stale exit", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::memory_map_failed);
        passed &= expect_sequence("post-stale map failure sequence", firmware, "GEG");
        passed &= expect_u32("post-stale map failure invalidates object", result.object_size, 0);

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.exit_result_count = 1;
        firmware.exit_results[0] = boot_handoff_firmware_result_t::failure;
        result = {};
        passed &= expect_error("exit failure", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::exit_failed);
        passed &= expect_u64("exit failure status", result.platform_status, k_exit_failure_status);
        passed &= expect_true("exit failure boundary", result.exit_attempted);
        passed &= expect_u32("exit failure invalidates object", result.object_size, 0);

        storage = {};
        initialize_storage(storage);
        firmware = {};
        firmware.exit_result_count = k_boot_services_exit_attempt_limit;
        for (uint32_t index{ 0 }; index < k_boot_services_exit_attempt_limit; ++index)
            firmware.exit_results[index] = boot_handoff_firmware_result_t::stale_map_key;
        result = {};
        passed &= expect_error("retry exhaustion", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::exit_retry_exhausted);
        passed &= expect_u32("retry exhaustion captures", result.memory_map_capture_count,
                             k_boot_services_exit_attempt_limit);
        passed &= expect_u32("retry exhaustion exits", result.exit_attempt_count,
                             k_boot_services_exit_attempt_limit);
        passed &= expect_u32("retry exhaustion invalidates object", result.object_size, 0);
        return passed;
    }

    bool run_snapshot_validation_tests() noexcept
    {
        bool passed{ true };

        const auto run_case = [&passed](const char* name, firmware_fixture_t firmware,
                                        boot_handoff_finalization_error_t expected) noexcept {
            storage_fixture_t storage{};
            initialize_storage(storage);
            boot_handoff_finalization_result_t result{};
            passed &= expect_error(name, run_finalization(storage, firmware, result), expected);
            passed &= expect_u32("invalid snapshot does not exit", result.exit_attempt_count, 0);
        };

        firmware_fixture_t firmware{};
        firmware.snapshot_descriptor_version = 2;
        run_case("unsupported descriptor version", firmware,
                 boot_handoff_finalization_error_t::invalid_memory_map_snapshot);

        firmware = {};
        firmware.snapshot_descriptor_size = 48;
        run_case("changed descriptor stride", firmware,
                 boot_handoff_finalization_error_t::invalid_memory_map_snapshot);

        firmware = {};
        firmware.snapshot_map_size = 241;
        run_case("partial final descriptor", firmware,
                 boot_handoff_finalization_error_t::invalid_memory_map_snapshot);

        firmware = {};
        firmware.zero_page_descriptor = true;
        run_case("zero-page descriptor", firmware,
                 boot_handoff_finalization_error_t::invalid_memory_descriptor);

        firmware = {};
        firmware.misaligned_descriptor = true;
        run_case("misaligned descriptor", firmware,
                 boot_handoff_finalization_error_t::invalid_memory_descriptor);

        firmware = {};
        firmware.omit_kernel_descriptor = true;
        run_case("missing live resource coverage", firmware,
                 boot_handoff_finalization_error_t::producer_failed);

        storage_fixture_t storage{};
        initialize_storage(storage);
        storage.storage.plan.source_descriptor_capacity = 1;
        boot_handoff_finalization_result_t result{};
        firmware = {};
        passed &= expect_error("tampered source capacity", run_finalization(storage, firmware, result),
                               boot_handoff_finalization_error_t::invalid_input);

        initialize_storage(storage);
        const burrow_loaded_image_t image{ make_loaded_image() };
        const warren_boot_early_console_t console{ make_console() };
        const boot_handoff_firmware_operations_t invalid_firmware{};
        result = {};
        passed &= expect_error("missing firmware callbacks",
                               finalize_boot_handoff(image, console, storage.storage, invalid_firmware, result),
                               boot_handoff_finalization_error_t::invalid_input);
        return passed;
    }
}

int main()
{
    bool passed{ true };
    passed &= run_immediate_success_test();
    passed &= run_unsorted_map_test();
    passed &= run_stale_retry_test();
    passed &= run_resize_tests();
    passed &= run_failure_tests();
    passed &= run_snapshot_validation_tests();

    if (!passed) return 1;

    std::puts("Warren boot-handoff finalization tests passed.");
    return 0;
}
