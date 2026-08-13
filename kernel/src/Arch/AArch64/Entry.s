//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

/*
 * Burrow AArch64 image-entry marker owned by the architecture layer.
 *
 * This branch never transfers control here and defines no accepted execution
 * level, register, stack, interrupt, or memory state. The symbol is exported
 * only as the ELF entry contract. It imports nothing, clobbers nothing, does
 * not access memory, and does not call C++. If reached accidentally, BRK makes
 * the unsupported transfer fail immediately rather than resembling startup.
 */

.section .text.burrow_aarch64_entry, "ax", %progbits
.p2align 2
.global burrow_aarch64_entry
.hidden burrow_aarch64_entry
.type burrow_aarch64_entry, %function
burrow_aarch64_entry:
    brk #0
.size burrow_aarch64_entry, . - burrow_aarch64_entry

.section .note.GNU-stack, "", %progbits
