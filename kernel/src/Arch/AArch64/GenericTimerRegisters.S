//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

.section .text.burrow_aarch64_generic_timer_registers, "ax", %progbits
.p2align 2
.global burrow_aarch64_read_counter_frequency
.hidden burrow_aarch64_read_counter_frequency
.type burrow_aarch64_read_counter_frequency, %function
burrow_aarch64_read_counter_frequency:
    mrs x0, CNTFRQ_EL0
    ret
.size burrow_aarch64_read_counter_frequency, . - burrow_aarch64_read_counter_frequency

.p2align 2
.global burrow_aarch64_arm_physical_timer
.hidden burrow_aarch64_arm_physical_timer
.type burrow_aarch64_arm_physical_timer, %function
burrow_aarch64_arm_physical_timer:
    uxtw x0, w0
    msr CNTP_TVAL_EL0, x0
    mov x1, #1
    msr CNTP_CTL_EL0, x1
    isb
    ret
.size burrow_aarch64_arm_physical_timer, . - burrow_aarch64_arm_physical_timer

.p2align 2
.global burrow_aarch64_disable_physical_timer
.hidden burrow_aarch64_disable_physical_timer
.type burrow_aarch64_disable_physical_timer, %function
burrow_aarch64_disable_physical_timer:
    msr CNTP_CTL_EL0, xzr
    isb
    ret
.size burrow_aarch64_disable_physical_timer, . - burrow_aarch64_disable_physical_timer

.section .note.GNU-stack, "", %progbits
