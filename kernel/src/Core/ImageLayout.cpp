//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

namespace
{

// These sentinels prove that every initial load-permission class contains real
// input in both build profiles. Remove each sentinel only after non-sentinel
// Burrow content permanently exercises the same audited class.
[[gnu::used, gnu::section(".rodata.burrow.layout_sentinel")]]
const unsigned char k_read_only_layout_sentinel[]{
    'B', 'U', 'R', 'R', 'O', 'W', '-', 'R', 'E', 'A', 'D', '-', 'O', 'N', 'L', 'Y'
};

[[gnu::used, gnu::section(".data.burrow.layout_sentinel")]]
unsigned long g_writable_layout_sentinel{ 0x425552524f572d57UL };

[[gnu::used, gnu::section(".bss.burrow.layout_sentinel")]]
unsigned char g_zero_filled_layout_sentinel[64]{};

[[gnu::used, gnu::section(".text.burrow.layout_sentinel")]]
unsigned long layout_sentinel_value() noexcept
{
    return 0x425552524f572d58UL;
}

} // namespace
