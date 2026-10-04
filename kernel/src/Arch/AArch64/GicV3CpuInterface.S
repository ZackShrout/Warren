//
// Created by Zack Shrout on 10/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

.section .text.burrow_aarch64_gicv3_cpu_interface, "ax", %progbits
.p2align 2
.global burrow_aarch64_gicv3_initialize_cpu_interface
.hidden burrow_aarch64_gicv3_initialize_cpu_interface
.type burrow_aarch64_gicv3_initialize_cpu_interface, %function
burrow_aarch64_gicv3_initialize_cpu_interface:
    mrs x1, ICC_SRE_EL1
    orr x1, x1, #1
    msr ICC_SRE_EL1, x1
    isb
    mrs x1, ICC_SRE_EL1
    tbz x1, #0, .Lgicv3_cpu_interface_failure

    mrs x1, ICC_CTLR_EL1
    bic x1, x1, #2
    msr ICC_CTLR_EL1, x1
    mov x1, #0xff
    msr ICC_PMR_EL1, x1
    msr ICC_BPR1_EL1, xzr
    mov x1, #1
    msr ICC_IGRPEN1_EL1, x1
    isb
    mov w0, wzr
    ret

.Lgicv3_cpu_interface_failure:
    mov w0, #1
    ret
.size burrow_aarch64_gicv3_initialize_cpu_interface, . - burrow_aarch64_gicv3_initialize_cpu_interface

.p2align 2
.global burrow_aarch64_gicv3_acknowledge
.hidden burrow_aarch64_gicv3_acknowledge
.type burrow_aarch64_gicv3_acknowledge, %function
burrow_aarch64_gicv3_acknowledge:
    mrs x0, ICC_IAR1_EL1
    ret
.size burrow_aarch64_gicv3_acknowledge, . - burrow_aarch64_gicv3_acknowledge

.p2align 2
.global burrow_aarch64_gicv3_end_interrupt
.hidden burrow_aarch64_gicv3_end_interrupt
.type burrow_aarch64_gicv3_end_interrupt, %function
burrow_aarch64_gicv3_end_interrupt:
    msr ICC_EOIR1_EL1, x0
    isb
    ret
.size burrow_aarch64_gicv3_end_interrupt, . - burrow_aarch64_gicv3_end_interrupt

.section .note.GNU-stack, "", %progbits
