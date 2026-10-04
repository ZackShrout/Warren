//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <burrow/Core/Panic.h>

extern "C" [[noreturn]] void burrow_qemu_virt_panic(
    const burrow::core::panic_record_t* record) noexcept;

extern "C" [[noreturn]] void burrow_qemu_virt_trigger_assertion_fixture() noexcept;
extern "C" [[noreturn]] void burrow_qemu_virt_trigger_panic_fixture() noexcept;
