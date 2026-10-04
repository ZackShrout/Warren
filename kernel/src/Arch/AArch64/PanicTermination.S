//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

.section .text.burrow_aarch64_panic, "ax", %progbits
.p2align 2
.global burrow_aarch64_mask_panic_interrupts
.hidden burrow_aarch64_mask_panic_interrupts
.type burrow_aarch64_mask_panic_interrupts, %function
burrow_aarch64_mask_panic_interrupts:
    msr DAIFSet, #0xf
    ret
.size burrow_aarch64_mask_panic_interrupts, . - burrow_aarch64_mask_panic_interrupts

.p2align 2
.global burrow_aarch64_terminate_panic
.hidden burrow_aarch64_terminate_panic
.type burrow_aarch64_terminate_panic, %function
burrow_aarch64_terminate_panic:
    msr DAIFSet, #0xf
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_panic
#else
.Lpanic_wait:
    wfe
    b .Lpanic_wait
#endif
.size burrow_aarch64_terminate_panic, . - burrow_aarch64_terminate_panic

.section .note.GNU-stack, "", %progbits
