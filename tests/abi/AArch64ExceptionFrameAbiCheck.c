//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Arch/AArch64/ExceptionFrame.h>

_Static_assert(BURROW_AARCH64_EXCEPTION_FRAME_ABI_MAJOR == 1,
               "unexpected exception-frame ABI");
_Static_assert(BURROW_AARCH64_EXCEPTION_FRAME_SIZE == 320,
               "unexpected exception-frame size");
