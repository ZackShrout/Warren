//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#define BURROW_AARCH64_EXCEPTION_FRAME_ABI_MAJOR UINT32_C(1)
#define BURROW_AARCH64_EXCEPTION_FRAME_SIZE UINT32_C(320)
#define BURROW_AARCH64_EXCEPTION_REGISTER_COUNT UINT32_C(31)

typedef struct burrow_aarch64_exception_frame
{
    uint32_t abi_major;
    uint32_t structure_size;
    uint32_t vector;
    uint32_t reserved;
    uint64_t registers[BURROW_AARCH64_EXCEPTION_REGISTER_COUNT];
    uint64_t stack_pointer;
    uint64_t transition_stage;
    uint64_t current_exception_level;
    uint64_t esr;
    uint64_t elr;
    uint64_t far;
    uint64_t spsr;
} burrow_aarch64_exception_frame_t;

#if defined(__cplusplus)
namespace burrow::arch::aarch64 {
    constexpr uint32_t k_exception_frame_abi_major{
        BURROW_AARCH64_EXCEPTION_FRAME_ABI_MAJOR
    };
    constexpr uint32_t k_exception_frame_size{ BURROW_AARCH64_EXCEPTION_FRAME_SIZE };
    constexpr uint32_t k_exception_register_count{
        BURROW_AARCH64_EXCEPTION_REGISTER_COUNT
    };
    using exception_frame_t = ::burrow_aarch64_exception_frame_t;
} // namespace burrow::arch::aarch64

#define BURROW_AARCH64_FRAME_STATIC_ASSERT(condition) static_assert(condition)
#define BURROW_AARCH64_FRAME_ALIGNOF(type) alignof(type)
#else
#define BURROW_AARCH64_FRAME_STATIC_ASSERT(condition) _Static_assert(condition, #condition)
#define BURROW_AARCH64_FRAME_ALIGNOF(type) _Alignof(type)
#endif

BURROW_AARCH64_FRAME_STATIC_ASSERT(sizeof(burrow_aarch64_exception_frame_t) == 320);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    BURROW_AARCH64_FRAME_ALIGNOF(burrow_aarch64_exception_frame_t) == 8);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, abi_major) == 0x000);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, structure_size) == 0x004);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, vector) == 0x008);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, reserved) == 0x00c);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, registers) == 0x010);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, stack_pointer) == 0x108);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, transition_stage) == 0x110);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, current_exception_level) == 0x118);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, esr) == 0x120);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, elr) == 0x128);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, far) == 0x130);
BURROW_AARCH64_FRAME_STATIC_ASSERT(
    offsetof(burrow_aarch64_exception_frame_t, spsr) == 0x138);

#undef BURROW_AARCH64_FRAME_STATIC_ASSERT
#undef BURROW_AARCH64_FRAME_ALIGNOF
