//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/ExceptionFrame.h>

static_assert(sizeof(burrow::arch::aarch64::exception_frame_t) ==
              sizeof(burrow_aarch64_exception_frame_t));
static_assert(burrow::arch::aarch64::k_exception_frame_size == 320);
