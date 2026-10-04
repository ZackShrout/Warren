//
// Created by Zack Shrout on 10/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Terminal EL1 vectors for the owned higher-half environment. Each slot saves
// x0/x1 before using a classification register. The common entry then captures
// the remaining GPRs and architectural state in the fixed 320-byte frame.

.equ FRAME_ABI_MAJOR,       1
.equ FRAME_SIZE,            0x140
.equ FRAME_VECTOR,          0x008
.equ FRAME_REGISTERS,       0x010
.equ FRAME_STACK_POINTER,   0x108
.equ FRAME_STAGE,           0x110
.equ FRAME_CURRENT_EL,      0x118
.equ FRAME_ESR,             0x120
.equ FRAME_ELR,             0x128
.equ FRAME_FAR,             0x130
.equ FRAME_SPSR,            0x138

.equ EMERGENCY_ACTIVE,      0x000

.macro BURROW_STABLE_VECTOR number
    sub sp, sp, #FRAME_SIZE
    stp x0, x1, [sp, #FRAME_REGISTERS]
    mov x0, #\number
    b burrow_aarch64_stable_exception
    .space 112
.endm

.section .text.burrow_aarch64_stable_vectors, "ax", %progbits
.p2align 11
.global burrow_aarch64_stable_vectors
.hidden burrow_aarch64_stable_vectors
.type burrow_aarch64_stable_vectors, %function
burrow_aarch64_stable_vectors:
    BURROW_STABLE_VECTOR 0
    BURROW_STABLE_VECTOR 1
    BURROW_STABLE_VECTOR 2
    BURROW_STABLE_VECTOR 3
    BURROW_STABLE_VECTOR 4
    BURROW_STABLE_VECTOR 5
    BURROW_STABLE_VECTOR 6
    BURROW_STABLE_VECTOR 7
    BURROW_STABLE_VECTOR 8
    BURROW_STABLE_VECTOR 9
    BURROW_STABLE_VECTOR 10
    BURROW_STABLE_VECTOR 11
    BURROW_STABLE_VECTOR 12
    BURROW_STABLE_VECTOR 13
    BURROW_STABLE_VECTOR 14
    BURROW_STABLE_VECTOR 15
.size burrow_aarch64_stable_vectors, . - burrow_aarch64_stable_vectors

.section .text.burrow_aarch64_stable_exception, "ax", %progbits
.p2align 2
.global burrow_aarch64_stable_exception
.hidden burrow_aarch64_stable_exception
.type burrow_aarch64_stable_exception, %function
burrow_aarch64_stable_exception:
    stp x2, x3, [sp, #0x020]
    stp x4, x5, [sp, #0x030]
    stp x6, x7, [sp, #0x040]
    stp x8, x9, [sp, #0x050]
    stp x10, x11, [sp, #0x060]
    stp x12, x13, [sp, #0x070]
    stp x14, x15, [sp, #0x080]
    stp x16, x17, [sp, #0x090]
    stp x18, x19, [sp, #0x0a0]
    stp x20, x21, [sp, #0x0b0]
    stp x22, x23, [sp, #0x0c0]
    stp x24, x25, [sp, #0x0d0]
    stp x26, x27, [sp, #0x0e0]
    stp x28, x29, [sp, #0x0f0]
    str x30, [sp, #0x100]

    msr DAIFSet, #0xf
    adrp x16, burrow_aarch64_emergency_state
    add x16, x16, :lo12:burrow_aarch64_emergency_state
    ldr x17, [x16, #EMERGENCY_ACTIVE]
    cbnz x17, .Lstable_nested_wait
    mov x17, #1
    str x17, [x16, #EMERGENCY_ACTIVE]

    mov w1, #FRAME_ABI_MAJOR
    str w1, [sp, #0x000]
    mov w1, #FRAME_SIZE
    str w1, [sp, #0x004]
    str w0, [sp, #FRAME_VECTOR]
    str wzr, [sp, #0x00c]

    add x1, sp, #FRAME_SIZE
    str x1, [sp, #FRAME_STACK_POINTER]
    adrp x1, burrow_aarch64_transition_stage
    add x1, x1, :lo12:burrow_aarch64_transition_stage
    ldr x1, [x1]
    str x1, [sp, #FRAME_STAGE]
    mrs x1, CurrentEL
    lsr x1, x1, #2
    str x1, [sp, #FRAME_CURRENT_EL]
    mrs x1, ESR_EL1
    str x1, [sp, #FRAME_ESR]
    mrs x1, ELR_EL1
    str x1, [sp, #FRAME_ELR]
    mrs x1, FAR_EL1
    str x1, [sp, #FRAME_FAR]
    mrs x1, SPSR_EL1
    str x1, [sp, #FRAME_SPSR]

    mov x0, sp
    bl burrow_aarch64_report_exception
    cbz x0, .Lstable_terminal_wait
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_exception
#endif

.Lstable_terminal_wait:
    msr DAIFSet, #0xf
.Lstable_terminal_wait_loop:
    wfe
    b .Lstable_terminal_wait_loop

.Lstable_nested_wait:
    msr DAIFSet, #0xf
.Lstable_nested_wait_loop:
    wfe
    b .Lstable_nested_wait_loop
.size burrow_aarch64_stable_exception, . - burrow_aarch64_stable_exception

.section .note.GNU-stack, "", %progbits
