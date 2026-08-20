//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Trusted QEMU-only terminal result for the loader's post-exit failure
// fixture. The production containment path branches here only in the
// explicitly selected UEFI test build.

.text
.p2align 2
.globl warren_qemu_post_exit_failure_result
.def warren_qemu_post_exit_failure_result
.scl 2
.type 32
.endef
warren_qemu_post_exit_failure_result:
    mov x12, x0
    adr x5, .Lterminal_marker
.Lwrite_terminal_marker:
    ldrb w6, [x5], #1
    cbz w6, .Lwait_for_transmit_drain
.Lwait_for_transmit_space:
    ldr w7, [x12, #0x18]
    tbnz w7, #5, .Lwait_for_transmit_space
    str w6, [x12]
    b .Lwrite_terminal_marker

.Lwait_for_transmit_drain:
    ldr w7, [x12, #0x18]
    tbnz w7, #3, .Lwait_for_transmit_drain

    mov w0, #0x20
    adr x1, warren_qemu_post_exit_arguments
    hlt #0xf000

    msr DAIFSet, #0xf
.Lunexpected_return_wait:
    wfe
    b .Lunexpected_return_wait

.section .rdata,"dr"
.p2align 3
.Lterminal_marker:
    .asciz "WARREN_TEST:1:FAIL:burrow-first-entry:73\r\n"

.data
.p2align 3
warren_qemu_post_exit_arguments:
    .quad 0x20026
    .quad 73
