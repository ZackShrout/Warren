//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

.text
.p2align 2

.globl warren_aarch64_synchronize_instruction_range
.def warren_aarch64_synchronize_instruction_range
.scl 2
.type 32
.endef
warren_aarch64_synchronize_instruction_range:
    mrs x2, ctr_el0

    ubfx x3, x2, #16, #4
    mov x4, #4
    lsl x3, x4, x3
    sub x4, x3, #1
    bic x5, x0, x4
.Lclean_data_line:
    dc cvau, x5
    adds x5, x5, x3
    b.hs .Ldata_clean_complete
    cmp x5, x1
    b.lo .Lclean_data_line
.Ldata_clean_complete:
    dsb ish

    ubfx x3, x2, #0, #4
    mov x4, #4
    lsl x3, x4, x3
    sub x4, x3, #1
    bic x5, x0, x4
.Linvalidate_instruction_line:
    ic ivau, x5
    adds x5, x5, x3
    b.hs .Linstruction_invalidate_complete
    cmp x5, x1
    b.lo .Linvalidate_instruction_line
.Linstruction_invalidate_complete:
    dsb ish
    isb
    ret

.p2align 2
.globl warren_aarch64_handoff
.def warren_aarch64_handoff
.scl 2
.type 32
.endef
warren_aarch64_handoff:
    mov x11, x0
    mov x9, x1
    mov x10, x2
    mov x12, x3

    mrs x8, CurrentEL
    cmp x8, #4
    b.eq .Lselect_el1_message
    cmp x8, #8
    b.eq .Lselect_el2_message
    mov x13, xzr
    adr x5, .Lunsupported_el_message
    b .Lwrite_post_exit_message
.Lselect_el1_message:
    mov x13, #1
    adr x5, .Lel1_message
    b .Lwrite_post_exit_message
.Lselect_el2_message:
    mov x13, #1
    adr x5, .Lel2_message

.Lwrite_post_exit_message:
    ldrb w6, [x5], #1
    cbz w6, .Lpost_exit_message_complete
.Lwait_for_transmit_space:
    ldr w7, [x12, #0x18]
    tbnz w7, #5, .Lwait_for_transmit_space
    str w6, [x12]
    b .Lwrite_post_exit_message

.Lpost_exit_message_complete:
    cbz x13, .Lunsupported_el_wait
    msr DAIFSet, #0xf
    mov sp, x10
    mov x0, x11
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    br x9

.Lunsupported_el_wait:
    msr DAIFSet, #0xf
.Lunsupported_el_wait_loop:
    wfe
    b .Lunsupported_el_wait_loop

.section .rdata,"dr"
.p2align 2
.Lel1_message:
    .asciz "WARREN_POST_EXIT:ExitBootServices:EL1\r\n"
.Lel2_message:
    .asciz "WARREN_POST_EXIT:ExitBootServices:EL2\r\n"
.Lunsupported_el_message:
    .asciz "WARREN_POST_EXIT:ExitBootServices:EL?\r\n"
