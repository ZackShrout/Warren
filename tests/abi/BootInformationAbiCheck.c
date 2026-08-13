//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <warren/boot/BootInformation.h>

uint32_t
warren_boot_information_c_abi_check(void)
{
    return (uint32_t)sizeof(warren_boot_information_t);
}
