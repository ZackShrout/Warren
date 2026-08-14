//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BurrowLoader.h>

namespace warren::boot
{
    namespace
    {
        constexpr uint16_t k_elf_type_dynamic{ 3 };
        constexpr uint16_t k_elf_machine_aarch64{ 183 };
        constexpr uint32_t k_elf_version_current{ 1 };
        constexpr uint16_t k_elf_header_size{ 64 };
        constexpr uint16_t k_program_header_size{ 56 };
        constexpr uint16_t k_maximum_program_headers{ 16 };

        constexpr uint32_t k_program_type_null{ 0 };
        constexpr uint32_t k_program_type_load{ 1 };
        constexpr uint32_t k_program_type_dynamic{ 2 };
        constexpr uint32_t k_program_type_program_headers{ 6 };
        constexpr uint32_t k_program_type_gnu_stack{ 0x6474e551 };
        constexpr uint32_t k_program_type_gnu_relro{ 0x6474e552 };

        constexpr uint32_t k_program_flag_execute{ 1 };
        constexpr uint32_t k_program_flag_write{ 2 };
        constexpr uint32_t k_program_flag_read{ 4 };

        constexpr int64_t k_dynamic_tag_hash{ 4 };
        constexpr int64_t k_dynamic_tag_string_table{ 5 };
        constexpr int64_t k_dynamic_tag_symbol_table{ 6 };
        constexpr int64_t k_dynamic_tag_rela{ 7 };
        constexpr int64_t k_dynamic_tag_rela_size{ 8 };
        constexpr int64_t k_dynamic_tag_rela_entry_size{ 9 };
        constexpr int64_t k_dynamic_tag_string_size{ 10 };
        constexpr int64_t k_dynamic_tag_symbol_entry_size{ 11 };
        constexpr int64_t k_dynamic_tag_flags{ 30 };
        constexpr int64_t k_dynamic_tag_gnu_hash{ 0x6ffffef5 };

        constexpr uint64_t k_dynamic_entry_size{ 16 };
        constexpr uint64_t k_rela_entry_size{ 24 };
        constexpr uint32_t k_relocation_aarch64_relative{ 1027 };

        struct program_header_t
        {
            uint32_t type;
            uint32_t flags;
            uint64_t offset;
            uint64_t virtual_address;
            uint64_t physical_address;
            uint64_t file_size;
            uint64_t memory_size;
            uint64_t alignment;
        };

        struct dynamic_state_t
        {
            bool terminated;
            bool has_hash;
            bool has_string_table;
            bool has_symbol_table;
            bool has_rela;
            bool has_rela_size;
            bool has_rela_entry_size;
            bool has_string_size;
            bool has_symbol_entry_size;
            bool has_flags;
            bool has_gnu_hash;
            uint64_t rela_virtual_address;
            uint64_t rela_size;
            uint64_t rela_entry_size;
        };

        [[nodiscard]] bool add_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            if (right > UINT64_MAX - left)
                return false;

            result = left + right;
            return true;
        }

        [[nodiscard]] bool multiply_without_overflow(
            uint64_t left,
            uint64_t right,
            uint64_t& result) noexcept
        {
            if (left != 0 && right > UINT64_MAX / left)
                return false;

            result = left * right;
            return true;
        }

        [[nodiscard]] bool range_within(
            uint64_t begin,
            uint64_t size,
            uint64_t outer_begin,
            uint64_t outer_size) noexcept
        {
            uint64_t end{ 0 };
            uint64_t outer_end{ 0 };
            return add_without_overflow(begin, size, end) &&
                   add_without_overflow(outer_begin, outer_size, outer_end) &&
                   begin >= outer_begin && end <= outer_end;
        }

        [[nodiscard]] bool ranges_overlap(
            uint64_t left_begin,
            uint64_t left_size,
            uint64_t right_begin,
            uint64_t right_size) noexcept
        {
            uint64_t left_end{ 0 };
            uint64_t right_end{ 0 };
            if (!add_without_overflow(left_begin, left_size, left_end) ||
                !add_without_overflow(right_begin, right_size, right_end))
                return true;

            return left_begin < right_end && right_begin < left_end;
        }

        [[nodiscard]] bool round_up_to_page(
            uint64_t value,
            uint64_t& rounded) noexcept
        {
            if (value > UINT64_MAX - (k_burrow_page_size - 1))
                return false;

            rounded = (value + k_burrow_page_size - 1) &
                      ~(k_burrow_page_size - 1);
            return true;
        }

        [[nodiscard]] bool read_bytes(
            const byte_source_t& source,
            uint64_t offset,
            uint8_t* destination,
            uint64_t byte_count) noexcept
        {
            if (source.read == nullptr || destination == nullptr)
                return false;

            uint64_t end{ 0 };
            if (!add_without_overflow(offset, byte_count, end) ||
                end > source.byte_count)
                return false;

            return source.read(source.context, offset, destination, byte_count);
        }

        [[nodiscard]] constexpr uint16_t decode_u16(
            const uint8_t* bytes) noexcept
        {
            return static_cast<uint16_t>(bytes[0]) |
                   static_cast<uint16_t>(bytes[1]) << 8U;
        }

        [[nodiscard]] constexpr uint32_t decode_u32(
            const uint8_t* bytes) noexcept
        {
            return static_cast<uint32_t>(bytes[0]) |
                   static_cast<uint32_t>(bytes[1]) << 8U |
                   static_cast<uint32_t>(bytes[2]) << 16U |
                   static_cast<uint32_t>(bytes[3]) << 24U;
        }

        [[nodiscard]] constexpr uint64_t decode_u64(
            const uint8_t* bytes) noexcept
        {
            return static_cast<uint64_t>(decode_u32(bytes)) |
                   static_cast<uint64_t>(decode_u32(bytes + 4)) << 32U;
        }

        void encode_u64(uint8_t* bytes, uint64_t value) noexcept
        {
            for (uint32_t index{ 0 }; index < 8; ++index)
                bytes[index] = static_cast<uint8_t>(value >> (index * 8U));
        }

        [[nodiscard]] program_header_t decode_program_header(
            const uint8_t* bytes) noexcept
        {
            return {
                decode_u32(bytes),
                decode_u32(bytes + 4),
                decode_u64(bytes + 8),
                decode_u64(bytes + 16),
                decode_u64(bytes + 24),
                decode_u64(bytes + 32),
                decode_u64(bytes + 40),
                decode_u64(bytes + 48)
            };
        }

        [[nodiscard]] bool segment_class_for_flags(
            uint32_t flags,
            burrow_segment_class_t& segment_class) noexcept
        {
            switch (flags)
            {
            case k_program_flag_read:
                segment_class = burrow_segment_class_t::read_only;
                return true;
            case k_program_flag_read | k_program_flag_execute:
                segment_class = burrow_segment_class_t::executable;
                return true;
            case k_program_flag_read | k_program_flag_write:
                segment_class = burrow_segment_class_t::writable;
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] uint32_t segment_index(
            burrow_segment_class_t segment_class) noexcept
        {
            return static_cast<uint32_t>(segment_class);
        }

        [[nodiscard]] bool virtual_range_to_file_offset(
            const burrow_load_plan_t& plan,
            uint64_t virtual_address,
            uint64_t byte_count,
            uint64_t& file_offset) noexcept
        {
            for (uint32_t index{ 0 }; index < k_burrow_load_segment_count; ++index)
            {
                const burrow_load_segment_t& segment{ plan.segments[index] };
                if (!range_within(
                        virtual_address,
                        byte_count,
                        segment.virtual_address,
                        segment.file_size))
                    continue;

                const uint64_t relative_offset{
                    virtual_address - segment.virtual_address
                };
                return add_without_overflow(
                    segment.file_offset,
                    relative_offset,
                    file_offset);
            }

            return false;
        }

        [[nodiscard]] bool virtual_address_is_loaded(
            const burrow_load_plan_t& plan,
            uint64_t virtual_address) noexcept
        {
            for (uint32_t index{ 0 }; index < k_burrow_load_segment_count; ++index)
            {
                const burrow_load_segment_t& segment{ plan.segments[index] };
                if (range_within(
                        virtual_address,
                        1,
                        segment.virtual_address,
                        segment.memory_size))
                    return true;
            }

            return false;
        }

        [[nodiscard]] bool dynamic_tag_is_forbidden(int64_t tag) noexcept
        {
            switch (tag)
            {
            case 1:  // DT_NEEDED
            case 2:  // DT_PLTRELSZ
            case 3:  // DT_PLTGOT
            case 12: // DT_INIT
            case 13: // DT_FINI
            case 14: // DT_SONAME
            case 15: // DT_RPATH
            case 16: // DT_SYMBOLIC
            case 17: // DT_REL
            case 18: // DT_RELSZ
            case 19: // DT_RELENT
            case 20: // DT_PLTREL
            case 21: // DT_DEBUG
            case 22: // DT_TEXTREL
            case 23: // DT_JMPREL
            case 24: // DT_BIND_NOW
            case 25: // DT_INIT_ARRAY
            case 26: // DT_FINI_ARRAY
            case 27: // DT_INIT_ARRAYSZ
            case 28: // DT_FINI_ARRAYSZ
            case 29: // DT_RUNPATH
            case 32: // DT_PREINIT_ARRAY
            case 33: // DT_PREINIT_ARRAYSZ
            case 35: // DT_RELRSZ
            case 36: // DT_RELR
            case 37: // DT_RELRENT
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] burrow_load_error_t record_dynamic_tag(
            int64_t tag,
            uint64_t value,
            dynamic_state_t& state) noexcept
        {
            bool* present{ nullptr };
            switch (tag)
            {
            case k_dynamic_tag_hash:
                present = &state.has_hash;
                break;
            case k_dynamic_tag_string_table:
                present = &state.has_string_table;
                break;
            case k_dynamic_tag_symbol_table:
                present = &state.has_symbol_table;
                break;
            case k_dynamic_tag_rela:
                present = &state.has_rela;
                state.rela_virtual_address = value;
                break;
            case k_dynamic_tag_rela_size:
                present = &state.has_rela_size;
                state.rela_size = value;
                break;
            case k_dynamic_tag_rela_entry_size:
                present = &state.has_rela_entry_size;
                state.rela_entry_size = value;
                break;
            case k_dynamic_tag_string_size:
                present = &state.has_string_size;
                break;
            case k_dynamic_tag_symbol_entry_size:
                if (value != 24)
                    return burrow_load_error_t::unsupported_dynamic_tag;
                present = &state.has_symbol_entry_size;
                break;
            case k_dynamic_tag_flags:
                if (value != 0)
                    return burrow_load_error_t::forbidden_dynamic_tag;
                present = &state.has_flags;
                break;
            case k_dynamic_tag_gnu_hash:
                present = &state.has_gnu_hash;
                break;
            default:
                if (dynamic_tag_is_forbidden(tag))
                    return burrow_load_error_t::forbidden_dynamic_tag;
                return burrow_load_error_t::unsupported_dynamic_tag;
            }

            if (*present)
                return burrow_load_error_t::duplicate_dynamic_tag;

            *present = true;
            return burrow_load_error_t::success;
        }

        [[nodiscard]] burrow_load_error_t validate_relocations(
            const byte_source_t& source,
            burrow_load_plan_t& plan,
            const dynamic_state_t& dynamic_state) noexcept
        {
            const uint32_t relocation_tag_count{
                static_cast<uint32_t>(dynamic_state.has_rela) +
                static_cast<uint32_t>(dynamic_state.has_rela_size) +
                static_cast<uint32_t>(dynamic_state.has_rela_entry_size)
            };
            if (relocation_tag_count == 0)
                return burrow_load_error_t::success;
            if (relocation_tag_count != 3)
                return burrow_load_error_t::incomplete_relocation_table;
            if (dynamic_state.rela_entry_size != k_rela_entry_size)
                return burrow_load_error_t::invalid_relocation_entry_size;
            if (dynamic_state.rela_size == 0 ||
                dynamic_state.rela_size % k_rela_entry_size != 0)
                return burrow_load_error_t::invalid_relocation_table_size;
            if ((dynamic_state.rela_virtual_address & 7U) != 0)
                return burrow_load_error_t::invalid_relocation_table_range;

            const uint64_t relocation_count{
                dynamic_state.rela_size / k_rela_entry_size
            };
            if (relocation_count > k_burrow_maximum_relocations)
                return burrow_load_error_t::invalid_relocation_table_size;

            uint64_t relocation_file_offset{ 0 };
            if (!virtual_range_to_file_offset(
                    plan,
                    dynamic_state.rela_virtual_address,
                    dynamic_state.rela_size,
                    relocation_file_offset))
                return burrow_load_error_t::invalid_relocation_table_range;

            uint8_t relocation_bytes[k_rela_entry_size]{};
            for (uint64_t index{ 0 }; index < relocation_count; ++index)
            {
                uint64_t entry_delta{ 0 };
                uint64_t entry_offset{ 0 };
                if (!multiply_without_overflow(index, k_rela_entry_size, entry_delta) ||
                    !add_without_overflow(
                        relocation_file_offset,
                        entry_delta,
                        entry_offset) ||
                    !read_bytes(
                        source,
                        entry_offset,
                        relocation_bytes,
                        sizeof(relocation_bytes)))
                    return burrow_load_error_t::source_read_failed;

                const uint64_t target{ decode_u64(relocation_bytes) };
                const uint64_t information{ decode_u64(relocation_bytes + 8) };
                const uint64_t addend{ decode_u64(relocation_bytes + 16) };
                if (static_cast<uint32_t>(information) !=
                    k_relocation_aarch64_relative)
                    return burrow_load_error_t::invalid_relocation_type;
                if ((information >> 32U) != 0)
                    return burrow_load_error_t::invalid_relocation_symbol;
                if (addend > static_cast<uint64_t>(INT64_MAX) ||
                    !virtual_address_is_loaded(plan, addend))
                    return burrow_load_error_t::invalid_relocation_addend;

                const burrow_load_segment_t& writable{
                    plan.segments[segment_index(
                        burrow_segment_class_t::writable)]
                };
                if ((target & 7U) != 0 ||
                    !range_within(
                        target,
                        8,
                        writable.virtual_address,
                        writable.memory_size))
                    return burrow_load_error_t::invalid_relocation_target;

                for (uint64_t previous{ 0 }; previous < index; ++previous)
                {
                    uint64_t previous_delta{ 0 };
                    uint64_t previous_offset{ 0 };
                    if (!multiply_without_overflow(
                            previous,
                            k_rela_entry_size,
                            previous_delta) ||
                        !add_without_overflow(
                            relocation_file_offset,
                            previous_delta,
                            previous_offset) ||
                        !read_bytes(
                            source,
                            previous_offset,
                            relocation_bytes,
                            sizeof(relocation_bytes)))
                        return burrow_load_error_t::source_read_failed;
                    if (decode_u64(relocation_bytes) == target)
                        return burrow_load_error_t::duplicate_relocation_target;
                }
            }

            plan.relocations = {
                relocation_file_offset,
                dynamic_state.rela_virtual_address,
                dynamic_state.rela_size,
                relocation_count
            };
            return burrow_load_error_t::success;
        }
    }

    burrow_load_error_t plan_burrow_image(
        const byte_source_t& source,
        burrow_load_plan_t& plan) noexcept
    {
        if (source.read == nullptr || source.byte_count < k_elf_header_size)
            return burrow_load_error_t::invalid_source;

        uint8_t elf_header[k_elf_header_size]{};
        if (!read_bytes(source, 0, elf_header, sizeof(elf_header)))
            return burrow_load_error_t::source_read_failed;

        if (elf_header[0] != 0x7f || elf_header[1] != 'E' ||
            elf_header[2] != 'L' || elf_header[3] != 'F')
            return burrow_load_error_t::invalid_elf_magic;
        if (elf_header[4] != 2)
            return burrow_load_error_t::unsupported_elf_class;
        if (elf_header[5] != 1)
            return burrow_load_error_t::unsupported_byte_order;
        if (elf_header[6] != k_elf_version_current ||
            decode_u32(elf_header + 20) != k_elf_version_current)
            return burrow_load_error_t::invalid_elf_version;
        if (elf_header[7] != 0 || elf_header[8] != 0)
            return burrow_load_error_t::unsupported_elf_abi;
        for (uint32_t index{ 9 }; index < 16; ++index)
        {
            if (elf_header[index] != 0)
                return burrow_load_error_t::unsupported_elf_abi;
        }
        if (decode_u16(elf_header + 16) != k_elf_type_dynamic)
            return burrow_load_error_t::unsupported_elf_type;
        if (decode_u16(elf_header + 18) != k_elf_machine_aarch64)
            return burrow_load_error_t::unsupported_machine;
        if (decode_u32(elf_header + 48) != 0)
            return burrow_load_error_t::invalid_machine_flags;
        if (decode_u16(elf_header + 52) != k_elf_header_size)
            return burrow_load_error_t::invalid_elf_header_size;
        if (decode_u16(elf_header + 54) != k_program_header_size)
            return burrow_load_error_t::invalid_program_header_size;

        const uint64_t entry_virtual_address{ decode_u64(elf_header + 24) };
        const uint64_t program_table_offset{ decode_u64(elf_header + 32) };
        const uint16_t program_count{ decode_u16(elf_header + 56) };
        if (program_count == 0)
            return burrow_load_error_t::invalid_program_table;
        if (program_count > k_maximum_program_headers)
            return burrow_load_error_t::too_many_program_headers;

        uint64_t program_table_size{ 0 };
        uint64_t program_table_end{ 0 };
        if (!multiply_without_overflow(
                program_count,
                k_program_header_size,
                program_table_size) ||
            !add_without_overflow(
                program_table_offset,
                program_table_size,
                program_table_end) ||
            program_table_end > source.byte_count)
            return burrow_load_error_t::invalid_program_table;

        burrow_load_plan_t candidate{};
        candidate.source_size = source.byte_count;
        candidate.entry_virtual_address = entry_virtual_address;
        bool segment_seen[k_burrow_load_segment_count]{};
        uint32_t load_count{ 0 };
        bool dynamic_seen{ false };
        program_header_t dynamic_program{};

        uint8_t program_bytes[k_program_header_size]{};
        for (uint16_t index{ 0 }; index < program_count; ++index)
        {
            uint64_t entry_delta{ 0 };
            uint64_t program_offset{ 0 };
            if (!multiply_without_overflow(
                    index,
                    k_program_header_size,
                    entry_delta) ||
                !add_without_overflow(
                    program_table_offset,
                    entry_delta,
                    program_offset) ||
                !read_bytes(
                    source,
                    program_offset,
                    program_bytes,
                    sizeof(program_bytes)))
                return burrow_load_error_t::source_read_failed;

            const program_header_t program{
                decode_program_header(program_bytes)
            };
            if ((program.flags & ~(k_program_flag_read |
                                   k_program_flag_write |
                                   k_program_flag_execute)) != 0)
                return burrow_load_error_t::invalid_load_permissions;
            if (program.type == k_program_type_load)
            {
                ++load_count;
                burrow_segment_class_t segment_class{};
                if (!segment_class_for_flags(program.flags, segment_class))
                    return burrow_load_error_t::invalid_load_permissions;

                const uint32_t class_index{ segment_index(segment_class) };
                if (segment_seen[class_index])
                    return burrow_load_error_t::invalid_load_permissions;
                if (program.file_size == 0 || program.memory_size == 0 ||
                    program.file_size > program.memory_size)
                    return burrow_load_error_t::invalid_load_size;
                if (program.alignment != k_burrow_page_size)
                    return burrow_load_error_t::invalid_load_alignment;
                if (program.offset % program.alignment !=
                    program.virtual_address % program.alignment)
                    return burrow_load_error_t::invalid_load_congruence;
                if ((program.offset & (k_burrow_page_size - 1)) != 0 ||
                    (program.virtual_address & (k_burrow_page_size - 1)) != 0)
                    return burrow_load_error_t::invalid_load_alignment;
                if (!range_within(
                        program.offset,
                        program.file_size,
                        0,
                        source.byte_count))
                    return burrow_load_error_t::invalid_load_file_range;

                uint64_t memory_end{ 0 };
                if (!add_without_overflow(
                        program.virtual_address,
                        program.memory_size,
                        memory_end) ||
                    memory_end > k_burrow_image_limit)
                    return burrow_load_error_t::invalid_load_address_range;
                if (program.physical_address != program.virtual_address)
                    return burrow_load_error_t::invalid_load_physical_address;

                candidate.segments[class_index] = {
                    program.offset,
                    program.virtual_address,
                    program.file_size,
                    program.memory_size,
                    segment_class
                };
                segment_seen[class_index] = true;
            }
            else if (program.type == k_program_type_dynamic)
            {
                if (dynamic_seen)
                    return burrow_load_error_t::duplicate_dynamic_segment;
                dynamic_seen = true;
                dynamic_program = program;
            }
            else if (program.type == k_program_type_gnu_stack)
            {
                if ((program.flags & k_program_flag_execute) != 0)
                    return burrow_load_error_t::invalid_load_permissions;
            }
            else if (program.type != k_program_type_null &&
                     program.type != k_program_type_program_headers &&
                     program.type != k_program_type_gnu_relro)
                return burrow_load_error_t::unsupported_program_type;
        }

        if (load_count != k_burrow_load_segment_count ||
            !segment_seen[0] || !segment_seen[1] || !segment_seen[2])
            return burrow_load_error_t::invalid_load_segment_count;
        if (!dynamic_seen)
            return burrow_load_error_t::missing_dynamic_segment;

        const burrow_load_segment_t& read_only{ candidate.segments[0] };
        const burrow_load_segment_t& executable{ candidate.segments[1] };
        const burrow_load_segment_t& writable{ candidate.segments[2] };
        if (read_only.virtual_address != 0 || read_only.file_offset != 0)
            return burrow_load_error_t::invalid_load_span;
        if (program_table_end > read_only.file_size)
            return burrow_load_error_t::invalid_load_span;

        uint64_t allocation_size{ 0 };
        for (uint32_t left{ 0 }; left < k_burrow_load_segment_count; ++left)
        {
            const burrow_load_segment_t& left_segment{ candidate.segments[left] };
            uint64_t left_memory_end{ 0 };
            uint64_t left_page_end{ 0 };
            uint64_t left_file_end{ 0 };
            uint64_t left_file_page_end{ 0 };
            if (!add_without_overflow(
                    left_segment.virtual_address,
                    left_segment.memory_size,
                    left_memory_end) ||
                !round_up_to_page(left_memory_end, left_page_end) ||
                !add_without_overflow(
                    left_segment.file_offset,
                    left_segment.file_size,
                    left_file_end) ||
                !round_up_to_page(left_file_end, left_file_page_end))
                return burrow_load_error_t::allocation_span_overflow;
            if (left_page_end > allocation_size)
                allocation_size = left_page_end;

            const uint64_t left_page_start{
                left_segment.virtual_address & ~(k_burrow_page_size - 1)
            };
            const uint64_t left_file_page_start{
                left_segment.file_offset & ~(k_burrow_page_size - 1)
            };

            for (uint32_t right{ left + 1 };
                 right < k_burrow_load_segment_count;
                 ++right)
            {
                const burrow_load_segment_t& right_segment{
                    candidate.segments[right]
                };
                uint64_t right_memory_end{ 0 };
                uint64_t right_page_end{ 0 };
                uint64_t right_file_end{ 0 };
                uint64_t right_file_page_end{ 0 };
                if (!add_without_overflow(
                        right_segment.virtual_address,
                        right_segment.memory_size,
                        right_memory_end) ||
                    !round_up_to_page(right_memory_end, right_page_end) ||
                    !add_without_overflow(
                        right_segment.file_offset,
                        right_segment.file_size,
                        right_file_end) ||
                    !round_up_to_page(right_file_end, right_file_page_end))
                    return burrow_load_error_t::allocation_span_overflow;

                const uint64_t right_page_start{
                    right_segment.virtual_address & ~(k_burrow_page_size - 1)
                };
                const uint64_t right_file_page_start{
                    right_segment.file_offset & ~(k_burrow_page_size - 1)
                };

                if (ranges_overlap(
                        left_page_start,
                        left_page_end - left_page_start,
                        right_page_start,
                        right_page_end - right_page_start) ||
                    ranges_overlap(
                        left_file_page_start,
                        left_file_page_end - left_file_page_start,
                        right_file_page_start,
                        right_file_page_end - right_file_page_start))
                    return burrow_load_error_t::overlapping_load_segments;
            }
        }
        candidate.allocation_size = allocation_size;

        if (!range_within(
                entry_virtual_address,
                1,
                executable.virtual_address,
                executable.file_size))
            return burrow_load_error_t::invalid_entry;

        if (dynamic_program.flags !=
                (k_program_flag_read | k_program_flag_write) ||
            dynamic_program.file_size == 0 ||
            dynamic_program.file_size != dynamic_program.memory_size ||
            dynamic_program.file_size % k_dynamic_entry_size != 0 ||
            dynamic_program.alignment < 8 ||
            (dynamic_program.alignment & (dynamic_program.alignment - 1)) != 0 ||
            (dynamic_program.virtual_address & 7U) != 0 ||
            dynamic_program.physical_address != dynamic_program.virtual_address ||
            !range_within(
                dynamic_program.virtual_address,
                dynamic_program.memory_size,
                writable.virtual_address,
                writable.memory_size) ||
            !range_within(
                dynamic_program.virtual_address,
                dynamic_program.file_size,
                writable.virtual_address,
                writable.file_size))
            return burrow_load_error_t::invalid_dynamic_segment;

        uint64_t expected_dynamic_offset{ 0 };
        if (!add_without_overflow(
                writable.file_offset,
                dynamic_program.virtual_address - writable.virtual_address,
                expected_dynamic_offset) ||
            dynamic_program.offset != expected_dynamic_offset ||
            !range_within(
                dynamic_program.offset,
                dynamic_program.file_size,
                0,
                source.byte_count))
            return burrow_load_error_t::invalid_dynamic_segment;

        candidate.dynamic_virtual_address = dynamic_program.virtual_address;
        candidate.dynamic_size = dynamic_program.file_size;

        dynamic_state_t dynamic_state{};
        const uint64_t dynamic_entry_count{
            dynamic_program.file_size / k_dynamic_entry_size
        };
        uint8_t dynamic_bytes[k_dynamic_entry_size]{};
        for (uint64_t index{ 0 }; index < dynamic_entry_count; ++index)
        {
            uint64_t entry_delta{ 0 };
            uint64_t entry_offset{ 0 };
            if (!multiply_without_overflow(
                    index,
                    k_dynamic_entry_size,
                    entry_delta) ||
                !add_without_overflow(
                    dynamic_program.offset,
                    entry_delta,
                    entry_offset) ||
                !read_bytes(
                    source,
                    entry_offset,
                    dynamic_bytes,
                    sizeof(dynamic_bytes)))
                return burrow_load_error_t::source_read_failed;

            const uint64_t raw_tag{ decode_u64(dynamic_bytes) };
            const uint64_t value{ decode_u64(dynamic_bytes + 8) };
            if (raw_tag == 0)
            {
                dynamic_state.terminated = true;
                break;
            }
            if (raw_tag > static_cast<uint64_t>(INT64_MAX))
                return burrow_load_error_t::unsupported_dynamic_tag;

            const burrow_load_error_t tag_result{
                record_dynamic_tag(
                    static_cast<int64_t>(raw_tag),
                    value,
                    dynamic_state)
            };
            if (tag_result != burrow_load_error_t::success)
                return tag_result;
        }
        if (!dynamic_state.terminated)
            return burrow_load_error_t::unterminated_dynamic_table;

        const burrow_load_error_t relocation_result{
            validate_relocations(source, candidate, dynamic_state)
        };
        if (relocation_result != burrow_load_error_t::success)
            return relocation_result;

        plan = candidate;
        return burrow_load_error_t::success;
    }

    burrow_load_error_t materialize_burrow_image(
        const byte_source_t& source,
        const burrow_load_plan_t& plan,
        uint64_t physical_start,
        uint8_t* destination,
        uint64_t destination_size,
        burrow_loaded_image_t& image) noexcept
    {
        if (source.read == nullptr || source.byte_count != plan.source_size)
            return burrow_load_error_t::invalid_source;
        if (destination == nullptr ||
            (reinterpret_cast<uintptr_t>(destination) &
             (k_burrow_page_size - 1)) != 0 ||
            destination_size == 0 || destination_size != plan.allocation_size ||
            (destination_size & (k_burrow_page_size - 1)) != 0)
            return burrow_load_error_t::invalid_destination;
        if ((physical_start & (k_burrow_page_size - 1)) != 0)
            return burrow_load_error_t::invalid_load_bias;

        uint64_t physical_end{ 0 };
        if (!add_without_overflow(
                physical_start,
                destination_size,
                physical_end))
            return burrow_load_error_t::physical_extent_overflow;

        for (uint64_t index{ 0 }; index < destination_size; ++index)
            destination[index] = 0;

        for (uint32_t index{ 0 }; index < k_burrow_load_segment_count; ++index)
        {
            const burrow_load_segment_t& segment{ plan.segments[index] };
            if (segment.segment_class !=
                    static_cast<burrow_segment_class_t>(index) ||
                segment.file_size > segment.memory_size ||
                !range_within(
                    segment.file_offset,
                    segment.file_size,
                    0,
                    source.byte_count) ||
                !range_within(
                    segment.virtual_address,
                    segment.memory_size,
                    0,
                    destination_size))
                return burrow_load_error_t::invalid_destination;

            if (!read_bytes(
                    source,
                    segment.file_offset,
                    destination + segment.virtual_address,
                    segment.file_size))
                return burrow_load_error_t::source_read_failed;
        }

        uint8_t relocation_bytes[k_rela_entry_size]{};
        uint64_t expected_relocation_size{ 0 };
        if (!multiply_without_overflow(
                plan.relocations.entry_count,
                k_rela_entry_size,
                expected_relocation_size) ||
            expected_relocation_size != plan.relocations.byte_count ||
            !range_within(
                plan.relocations.file_offset,
                plan.relocations.byte_count,
                0,
                source.byte_count))
            return burrow_load_error_t::invalid_relocation_table_range;

        const burrow_load_segment_t& writable{
            plan.segments[segment_index(burrow_segment_class_t::writable)]
        };
        if (plan.relocations.entry_count > k_burrow_maximum_relocations)
            return burrow_load_error_t::invalid_relocation_table_size;
        for (uint64_t index{ 0 }; index < plan.relocations.entry_count; ++index)
        {
            uint64_t entry_delta{ 0 };
            uint64_t entry_offset{ 0 };
            if (!multiply_without_overflow(index, k_rela_entry_size, entry_delta) ||
                !add_without_overflow(
                    plan.relocations.file_offset,
                    entry_delta,
                    entry_offset) ||
                !read_bytes(
                    source,
                    entry_offset,
                    relocation_bytes,
                    sizeof(relocation_bytes)))
                return burrow_load_error_t::source_read_failed;

            const uint64_t target{ decode_u64(relocation_bytes) };
            const uint64_t information{ decode_u64(relocation_bytes + 8) };
            const uint64_t addend{ decode_u64(relocation_bytes + 16) };
            if (static_cast<uint32_t>(information) !=
                k_relocation_aarch64_relative)
                return burrow_load_error_t::invalid_relocation_type;
            if ((information >> 32U) != 0)
                return burrow_load_error_t::invalid_relocation_symbol;
            if (addend > static_cast<uint64_t>(INT64_MAX) ||
                !virtual_address_is_loaded(plan, addend))
                return burrow_load_error_t::invalid_relocation_addend;
            if ((target & 7U) != 0 ||
                !range_within(
                    target,
                    8,
                    writable.virtual_address,
                    writable.memory_size) ||
                !range_within(target, 8, 0, destination_size))
                return burrow_load_error_t::invalid_relocation_target;

            for (uint64_t previous{ 0 }; previous < index; ++previous)
            {
                uint64_t previous_delta{ 0 };
                uint64_t previous_offset{ 0 };
                if (!multiply_without_overflow(
                        previous,
                        k_rela_entry_size,
                        previous_delta) ||
                    !add_without_overflow(
                        plan.relocations.file_offset,
                        previous_delta,
                        previous_offset) ||
                    !read_bytes(
                        source,
                        previous_offset,
                        relocation_bytes,
                        sizeof(relocation_bytes)))
                    return burrow_load_error_t::source_read_failed;
                if (decode_u64(relocation_bytes) == target)
                    return burrow_load_error_t::duplicate_relocation_target;
            }

            uint64_t relocated_value{ 0 };
            if (!add_without_overflow(
                    physical_start,
                    addend,
                    relocated_value))
                return burrow_load_error_t::physical_extent_overflow;

            encode_u64(destination + target, relocated_value);
        }

        uint64_t entry_physical_address{ 0 };
        if (!add_without_overflow(
                physical_start,
                plan.entry_virtual_address,
                entry_physical_address))
            return burrow_load_error_t::physical_extent_overflow;

        const burrow_loaded_image_t candidate{
            physical_start,
            destination_size,
            physical_start,
            entry_physical_address
        };
        image = candidate;
        static_cast<void>(physical_end);
        return burrow_load_error_t::success;
    }

    const char* burrow_load_error_name(burrow_load_error_t error) noexcept
    {
        switch (error)
        {
        case burrow_load_error_t::success: return "success";
        case burrow_load_error_t::invalid_source: return "invalid-source";
        case burrow_load_error_t::source_read_failed: return "source-read-failed";
        case burrow_load_error_t::invalid_elf_magic: return "invalid-elf-magic";
        case burrow_load_error_t::unsupported_elf_class: return "unsupported-elf-class";
        case burrow_load_error_t::unsupported_byte_order: return "unsupported-byte-order";
        case burrow_load_error_t::invalid_elf_version: return "invalid-elf-version";
        case burrow_load_error_t::unsupported_elf_abi: return "unsupported-elf-abi";
        case burrow_load_error_t::unsupported_elf_type: return "unsupported-elf-type";
        case burrow_load_error_t::unsupported_machine: return "unsupported-machine";
        case burrow_load_error_t::invalid_machine_flags: return "invalid-machine-flags";
        case burrow_load_error_t::invalid_elf_header_size: return "invalid-elf-header-size";
        case burrow_load_error_t::invalid_program_header_size: return "invalid-program-header-size";
        case burrow_load_error_t::invalid_program_table: return "invalid-program-table";
        case burrow_load_error_t::too_many_program_headers: return "too-many-program-headers";
        case burrow_load_error_t::unsupported_program_type: return "unsupported-program-type";
        case burrow_load_error_t::invalid_load_segment_count: return "invalid-load-segment-count";
        case burrow_load_error_t::invalid_load_permissions: return "invalid-load-permissions";
        case burrow_load_error_t::invalid_load_size: return "invalid-load-size";
        case burrow_load_error_t::invalid_load_alignment: return "invalid-load-alignment";
        case burrow_load_error_t::invalid_load_congruence: return "invalid-load-congruence";
        case burrow_load_error_t::invalid_load_file_range: return "invalid-load-file-range";
        case burrow_load_error_t::invalid_load_address_range: return "invalid-load-address-range";
        case burrow_load_error_t::invalid_load_physical_address: return "invalid-load-physical-address";
        case burrow_load_error_t::overlapping_load_segments: return "overlapping-load-segments";
        case burrow_load_error_t::invalid_load_span: return "invalid-load-span";
        case burrow_load_error_t::missing_dynamic_segment: return "missing-dynamic-segment";
        case burrow_load_error_t::duplicate_dynamic_segment: return "duplicate-dynamic-segment";
        case burrow_load_error_t::invalid_dynamic_segment: return "invalid-dynamic-segment";
        case burrow_load_error_t::invalid_entry: return "invalid-entry";
        case burrow_load_error_t::unterminated_dynamic_table: return "unterminated-dynamic-table";
        case burrow_load_error_t::duplicate_dynamic_tag: return "duplicate-dynamic-tag";
        case burrow_load_error_t::forbidden_dynamic_tag: return "forbidden-dynamic-tag";
        case burrow_load_error_t::unsupported_dynamic_tag: return "unsupported-dynamic-tag";
        case burrow_load_error_t::incomplete_relocation_table: return "incomplete-relocation-table";
        case burrow_load_error_t::invalid_relocation_entry_size: return "invalid-relocation-entry-size";
        case burrow_load_error_t::invalid_relocation_table_size: return "invalid-relocation-table-size";
        case burrow_load_error_t::invalid_relocation_table_range: return "invalid-relocation-table-range";
        case burrow_load_error_t::invalid_relocation_type: return "invalid-relocation-type";
        case burrow_load_error_t::invalid_relocation_symbol: return "invalid-relocation-symbol";
        case burrow_load_error_t::invalid_relocation_addend: return "invalid-relocation-addend";
        case burrow_load_error_t::invalid_relocation_target: return "invalid-relocation-target";
        case burrow_load_error_t::duplicate_relocation_target: return "duplicate-relocation-target";
        case burrow_load_error_t::allocation_span_overflow: return "allocation-span-overflow";
        case burrow_load_error_t::invalid_destination: return "invalid-destination";
        case burrow_load_error_t::invalid_load_bias: return "invalid-load-bias";
        case burrow_load_error_t::physical_extent_overflow: return "physical-extent-overflow";
        }

        return "unknown";
    }
}
