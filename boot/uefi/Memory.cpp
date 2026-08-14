//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include <stddef.h>

extern "C" void* memset(void* destination, int value, size_t byte_count) noexcept
{
    auto* output{ static_cast<volatile unsigned char*>(destination) };

    for (size_t index{ 0 }; index < byte_count; ++index)
        output[index] = static_cast<unsigned char>(value);

    return destination;
}

extern "C" void* memcpy(void* destination, const void* source, size_t byte_count) noexcept
{
    auto* output{ static_cast<volatile unsigned char*>(destination) };
    const auto* input{ static_cast<const volatile unsigned char*>(source) };

    for (size_t index{ 0 }; index < byte_count; ++index)
        output[index] = input[index];

    return destination;
}
