//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#define WARREN_BOOT_INFORMATION_MAGIC "WARRENBI"
#define WARREN_BOOT_INFORMATION_MAGIC_SIZE UINT32_C(8)
#define WARREN_BOOT_INFORMATION_MAJOR UINT16_C(1)
#define WARREN_BOOT_INFORMATION_MINOR UINT16_C(0)
#define WARREN_BOOT_INFORMATION_HEADER_SIZE UINT32_C(256)
#define WARREN_BOOT_INFORMATION_PAGE_SIZE UINT32_C(4096)

#define WARREN_BOOT_FEATURE_MEMORY_MAP UINT64_C(0x0001)
#define WARREN_BOOT_FEATURE_COMMAND_LINE UINT64_C(0x0002)
#define WARREN_BOOT_FEATURE_INITIAL_IMAGE UINT64_C(0x0004)
#define WARREN_BOOT_FEATURE_EARLY_CONSOLE UINT64_C(0x0008)
#define WARREN_BOOT_FEATURE_FRAMEBUFFER UINT64_C(0x0010)
#define WARREN_BOOT_FEATURE_ACPI_RSDP UINT64_C(0x0020)
#define WARREN_BOOT_FEATURE_DEVICE_TREE UINT64_C(0x0040)
#define WARREN_BOOT_FEATURE_KNOWN_MASK UINT64_C(0x007f)

#define WARREN_BOOT_MEMORY_RESERVED UINT32_C(0)
#define WARREN_BOOT_MEMORY_USABLE UINT32_C(1)
#define WARREN_BOOT_MEMORY_LOADER_RECLAIMABLE UINT32_C(2)
#define WARREN_BOOT_MEMORY_BOOT_INFORMATION UINT32_C(3)
#define WARREN_BOOT_MEMORY_KERNEL_IMAGE UINT32_C(4)
#define WARREN_BOOT_MEMORY_BOOTSTRAP_STACK UINT32_C(5)
#define WARREN_BOOT_MEMORY_INITIAL_IMAGE UINT32_C(6)
#define WARREN_BOOT_MEMORY_FIRMWARE_RECLAIMABLE UINT32_C(7)
#define WARREN_BOOT_MEMORY_FIRMWARE_RUNTIME UINT32_C(8)
#define WARREN_BOOT_MEMORY_ACPI_RECLAIMABLE UINT32_C(9)
#define WARREN_BOOT_MEMORY_ACPI_NVS UINT32_C(10)
#define WARREN_BOOT_MEMORY_MMIO UINT32_C(11)
#define WARREN_BOOT_MEMORY_PERSISTENT UINT32_C(12)
#define WARREN_BOOT_MEMORY_UNUSABLE UINT32_C(13)
#define WARREN_BOOT_MEMORY_KIND_MAXIMUM WARREN_BOOT_MEMORY_UNUSABLE

#define WARREN_BOOT_MEMORY_SOURCE_NONE UINT32_C(0)
#define WARREN_BOOT_MEMORY_SOURCE_UEFI UINT32_C(1)

#define WARREN_BOOT_CONSOLE_PL011 UINT32_C(1)
#define WARREN_BOOT_CONSOLE_OUTPUT UINT32_C(0x01)
#define WARREN_BOOT_CONSOLE_INPUT UINT32_C(0x02)
#define WARREN_BOOT_CONSOLE_KNOWN_FLAGS UINT32_C(0x03)

#define WARREN_BOOT_PIXEL_RGB_RESERVED_8 UINT32_C(1)
#define WARREN_BOOT_PIXEL_BGR_RESERVED_8 UINT32_C(2)
#define WARREN_BOOT_PIXEL_BIT_MASK UINT32_C(3)

typedef struct warren_boot_section
{
    uint32_t offset;
    uint32_t count;
    uint32_t stride;
    uint32_t reserved;
} warren_boot_section_t;

typedef struct warren_boot_memory_entry
{
    uint64_t physical_start;
    uint64_t page_count;
    uint32_t memory_kind;
    uint32_t source_kind;
    uint32_t source_type;
    uint32_t reserved;
    uint64_t source_attributes;
} warren_boot_memory_entry_t;

typedef struct warren_boot_early_console
{
    uint32_t kind;
    uint32_t flags;
    uint64_t physical_address;
    uint32_t register_stride;
    uint32_t register_width;
    uint64_t input_clock_hz;
    uint32_t baud_rate;
    uint32_t reserved_0;
    uint64_t reserved_1[3];
} warren_boot_early_console_t;

typedef struct warren_boot_framebuffer
{
    uint64_t physical_address;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
    uint32_t pixel_format;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
    uint64_t reserved[2];
} warren_boot_framebuffer_t;

typedef struct warren_boot_information
{
    uint8_t magic[8];
    uint16_t major;
    uint16_t minor;
    uint32_t header_size;
    uint32_t total_size;
    uint32_t page_size;
    uint64_t present_features;
    uint64_t required_features;
    uint64_t self_physical_address;
    uint64_t kernel_physical_start;
    uint64_t kernel_physical_size;
    uint64_t kernel_load_bias;
    uint64_t kernel_entry_physical_address;
    uint64_t bootstrap_stack_physical_start;
    uint64_t bootstrap_stack_size;
    uint64_t initial_image_physical_start;
    uint64_t initial_image_size;
    uint64_t acpi_rsdp_physical_address;
    uint64_t device_tree_physical_address;
    uint64_t device_tree_size;
    warren_boot_section_t memory_map;
    warren_boot_section_t command_line;
    warren_boot_section_t early_console;
    warren_boot_section_t framebuffer;
    uint8_t reserved[56];
} warren_boot_information_t;

#if defined(__cplusplus)
#define WARREN_BOOT_STATIC_ASSERT(condition) static_assert(condition)
#define WARREN_BOOT_ALIGNOF(type) alignof(type)
#else
#define WARREN_BOOT_STATIC_ASSERT(condition) _Static_assert(condition, #condition)
#define WARREN_BOOT_ALIGNOF(type) _Alignof(type)
#endif

WARREN_BOOT_STATIC_ASSERT(sizeof(warren_boot_section_t) == 16);
WARREN_BOOT_STATIC_ASSERT(WARREN_BOOT_ALIGNOF(warren_boot_section_t) == 4);

WARREN_BOOT_STATIC_ASSERT(sizeof(warren_boot_memory_entry_t) == 40);
WARREN_BOOT_STATIC_ASSERT(WARREN_BOOT_ALIGNOF(warren_boot_memory_entry_t) == 8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, physical_start) == 0x00);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, page_count) == 0x08);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, memory_kind) == 0x10);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, source_kind) == 0x14);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, source_type) == 0x18);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, reserved) == 0x1c);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_memory_entry_t, source_attributes) == 0x20);

WARREN_BOOT_STATIC_ASSERT(sizeof(warren_boot_early_console_t) == 64);
WARREN_BOOT_STATIC_ASSERT(WARREN_BOOT_ALIGNOF(warren_boot_early_console_t) == 8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_early_console_t, physical_address) == 0x08);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_early_console_t, input_clock_hz) == 0x18);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_early_console_t, reserved_1) == 0x28);

WARREN_BOOT_STATIC_ASSERT(sizeof(warren_boot_framebuffer_t) == 64);
WARREN_BOOT_STATIC_ASSERT(WARREN_BOOT_ALIGNOF(warren_boot_framebuffer_t) == 8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_framebuffer_t, physical_address) == 0x00);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_framebuffer_t, width) == 0x10);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_framebuffer_t, reserved) == 0x30);

WARREN_BOOT_STATIC_ASSERT(sizeof(warren_boot_information_t) == WARREN_BOOT_INFORMATION_HEADER_SIZE);
WARREN_BOOT_STATIC_ASSERT(WARREN_BOOT_ALIGNOF(warren_boot_information_t) == 8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, magic) == 0x000);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, major) == 0x008);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, minor) == 0x00a);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, header_size) == 0x00c);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, total_size) == 0x010);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, page_size) == 0x014);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, present_features) == 0x018);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, required_features) == 0x020);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, self_physical_address) == 0x028);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, kernel_physical_start) == 0x030);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, kernel_physical_size) == 0x038);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, kernel_load_bias) == 0x040);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, kernel_entry_physical_address) == 0x048);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, bootstrap_stack_physical_start) == 0x050);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, bootstrap_stack_size) == 0x058);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, initial_image_physical_start) == 0x060);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, initial_image_size) == 0x068);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, acpi_rsdp_physical_address) == 0x070);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, device_tree_physical_address) == 0x078);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, device_tree_size) == 0x080);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, memory_map) == 0x088);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, command_line) == 0x098);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, early_console) == 0x0a8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, framebuffer) == 0x0b8);
WARREN_BOOT_STATIC_ASSERT(offsetof(warren_boot_information_t, reserved) == 0x0c8);

#undef WARREN_BOOT_ALIGNOF
#undef WARREN_BOOT_STATIC_ASSERT
