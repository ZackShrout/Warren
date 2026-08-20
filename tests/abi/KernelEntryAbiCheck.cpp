//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <burrow/Core/KernelEntry.h>

static_assert(sizeof(burrow::core::KernelEntryContext) ==
              sizeof(burrow_kernel_entry_context_t));
