//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformation.h>

extern "C" uint32_t
warren_boot_information_cpp_abi_check() noexcept
{
    return static_cast<uint32_t>(sizeof(warren_boot_information_t));
}
